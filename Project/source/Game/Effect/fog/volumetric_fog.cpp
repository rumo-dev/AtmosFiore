#include "volumetric_fog.h"
#include "Engine/System/render_state.h"
#include "Engine/Graphics/Shader/shader.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"
#include "Engine/System/graphics_core.h"
#include "Engine/System/Manager/Light/point_light_manager.h"
#include "Engine/System/Manager/Light/spot_light_manager.h"
#include <cassert>

VolumetricFog::VolumetricFog(ID3D11Device* device, uint32_t width, uint32_t height)
{
	blit_ = std::make_unique<FullscreenQuad>(device);
	target_ = std::make_unique<Framebuffer>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, false);

	// シェーダーのロード
	create_ps_from_cso(device, "volumetric_fog_ps.cso", ps_.GetAddressOf());
	create_cs_from_cso(device, "volumetric_fog_injection_cs.cso", cs_injection_.GetAddressOf());
	create_cs_from_cso(device, "volumetric_fog_accumulation_cs.cso", cs_accumulation_.GetAddressOf());

	// 定数バッファの作成
	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.ByteWidth = sizeof(Config);
	HRESULT hr = device->CreateBuffer(&desc, nullptr, cb_.GetAddressOf());
	assert(SUCCEEDED(hr));

	// ── 3Dテクスチャの作成 ─────────────────────────────────────────
	D3D11_TEXTURE3D_DESC tex3d_desc{};
	tex3d_desc.Width = config.grid_width;
	tex3d_desc.Height = config.grid_height;
	tex3d_desc.Depth = config.grid_depth;
	tex3d_desc.MipLevels = 1;
	tex3d_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	tex3d_desc.Usage = D3D11_USAGE_DEFAULT;
	tex3d_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	tex3d_desc.CPUAccessFlags = 0;
	tex3d_desc.MiscFlags = 0;

	// ライト注入バッファ
	hr = device->CreateTexture3D(&tex3d_desc, nullptr, light_volume_tex_.GetAddressOf());
	assert(SUCCEEDED(hr));

	hr = device->CreateShaderResourceView(light_volume_tex_.Get(), nullptr, light_volume_srv_.GetAddressOf());
	assert(SUCCEEDED(hr));

	hr = device->CreateUnorderedAccessView(light_volume_tex_.Get(), nullptr, light_volume_uav_.GetAddressOf());
	assert(SUCCEEDED(hr));

	// 累積バッファ
	hr = device->CreateTexture3D(&tex3d_desc, nullptr, accum_volume_tex_.GetAddressOf());
	assert(SUCCEEDED(hr));

	hr = device->CreateShaderResourceView(accum_volume_tex_.Get(), nullptr, accum_volume_srv_.GetAddressOf());
	assert(SUCCEEDED(hr));

	hr = device->CreateUnorderedAccessView(accum_volume_tex_.Get(), nullptr, accum_volume_uav_.GetAddressOf());
	assert(SUCCEEDED(hr));
}

void VolumetricFog::make(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src_srv)
{
	if (!is_enabled)
	{
		// 無効化されている場合は前段をクリアして直接 Blit するか何もしない
		// ここでは単に前段のテクスチャをそのままターゲットへ Blit して終了
		target_->Clear(ctx, 0, 0, 0, 1);
		target_->Activate(ctx);
		{
			blit_->Blit(ctx, &src_srv, 0, 1, nullptr); // デフォルトPSで転送
		}
		target_->Deactivate(ctx);
		return;
	}

	// 定数バッファの更新
	config.is_enabled = is_enabled ? 1 : 0;
	ctx->UpdateSubresource(cb_.Get(), 0, nullptr, &config, 0, 0);

	// ── パス1: ライト注入 (Compute Shader) ─────────────────────────
	ctx->CSSetShader(cs_injection_.Get(), nullptr, 0);

	// 定数バッファのバインド
	auto camera_cb = Graphics_Core::instance().get_constant_buffer(Graphics_Core::Conastant_Buffer_Type::Camera);
	auto light_cb = Graphics_Core::instance().get_constant_buffer(Graphics_Core::Conastant_Buffer_Type::Light);
	auto point_count_cb = Graphics_Core::instance().get_point_light_manager().get_cb();
	auto spot_count_cb = Graphics_Core::instance().get_spot_light_manager().get_cb();

	ctx->CSSetConstantBuffers(1, 1, &camera_cb);
	ctx->CSSetConstantBuffers(2, 1, &light_cb);
	ctx->CSSetConstantBuffers(4, 1, &point_count_cb);
	ctx->CSSetConstantBuffers(6, 1, &spot_count_cb);
	ctx->CSSetConstantBuffers(8, 1, cb_.GetAddressOf());

	// SRV のバインド
	auto ps_front = Graphics_Core::instance().post_procss.GetShadow().get_point_shadow_front_map();
	auto ps_back = Graphics_Core::instance().post_procss.GetShadow().get_point_shadow_back_map();
	auto dir_shadow = Graphics_Core::instance().post_procss.GetShadow().get_directional_shadow_map();
	auto point_srv = Graphics_Core::instance().get_point_light_manager().get_srv();
	auto spot_srv = Graphics_Core::instance().get_spot_light_manager().get_srv();

	ctx->CSSetShaderResources(5, 1, &ps_front);
	ctx->CSSetShaderResources(6, 1, &ps_back);
	ctx->CSSetShaderResources(7, 1, &dir_shadow);
	ctx->CSSetShaderResources(8, 1, &point_srv);
	ctx->CSSetShaderResources(9, 1, &spot_srv);

	// サンプラーのバインド
	Render_State::instance().set_cs_sampler_state(ctx);

	// 出力 UAV のバインド
	ctx->CSSetUnorderedAccessViews(0, 1, light_volume_uav_.GetAddressOf(), nullptr);

	// スレッドグループのディスパッチ
	// グループサイズ (8, 8, 4) に対するグリッドディスパッチ
	ctx->Dispatch(20, 12, config.grid_depth / 4);

	// UAV と SRV のアンバインド (D3D 警告防止)
	ID3D11UnorderedAccessView* null_uav = nullptr;
	ctx->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
	ID3D11ShaderResourceView* null_srvs[10]{};
	ctx->CSSetShaderResources(5, 5, null_srvs);

	// ── パス2: Z軸累積 (Compute Shader) ───────────────────────────
	ctx->CSSetShader(cs_accumulation_.Get(), nullptr, 0);

	// 入力と出力のバインド
	ctx->CSSetShaderResources(0, 1, light_volume_srv_.GetAddressOf());
	ctx->CSSetUnorderedAccessViews(0, 1, accum_volume_uav_.GetAddressOf(), nullptr);

	// ディスパッチ (8, 8, 1) スレッドグループ
	ctx->Dispatch(20, 12, 1);

	// アンバインド
	ctx->CSSetShaderResources(0, 1, null_srvs);
	ctx->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
	ctx->CSSetShader(nullptr, nullptr, 0);

	// ── パス3: 最終合成 (Pixel Shader / Fullscreen Quad) ───────────
	Render_State::instance().set_2d_render_states(ctx);

	// ピクセルシェーダー定数バッファのバインド
	ctx->PSSetConstantBuffers(8, 1, cb_.GetAddressOf());

	// SRV の作成とバインド
	ID3D11ShaderResourceView* srvs[GBUFFER_COUNT + 1]{};
	Graphics_Core::instance().get_geometry_buffer()->GetShaderResourceViews(srvs);
	srvs[GBUFFER_COUNT + 0] = src_srv;

	// 3D 累積テクスチャを t10 にバインド
	ctx->PSSetShaderResources(10, 1, accum_volume_srv_.GetAddressOf());

	target_->Clear(ctx, 0, 0, 0, 1);
	target_->Activate(ctx);
	{
		blit_->Blit(ctx, srvs, 0, (GBUFFER_COUNT + 1), ps_.Get());
	}
	target_->Deactivate(ctx);

	// 後始末
	ID3D11ShaderResourceView* null_ps_srv = nullptr;
	ctx->PSSetShaderResources(10, 1, &null_ps_srv);
}

ID3D11ShaderResourceView* VolumetricFog::get_color_map() const
{
	return target_->GetColorMap();
}

ID3D11ShaderResourceView** VolumetricFog::get_color_map_address() const
{
	return target_->GetColorMapAddress();
}

void VolumetricFog::DrawDebugUI()
{
	ImGui::Checkbox("Enable##VolFog", &is_enabled);
	if (!is_enabled) return;

	ImGui::SeparatorText("Froxel Media Settings");
	ImGui::SliderFloat("Density Base", &config.density_base, 0.0f, 0.5f, "%.4f");
	ImGui::SliderFloat("Scattering", &config.scattering, 0.0f, 1.0f);
	ImGui::SliderFloat("Absorption", &config.absorption, 0.0f, 1.0f);

	ImGui::SeparatorText("Light & Phase");
	ImGui::SliderFloat("Anisotropy (g)", &config.anisotropy, -0.99f, 0.99f);
	ImGui::SliderFloat("Intensity", &config.intensity, 0.0f, 10.0f);

	ImGui::SeparatorText("Noise & Animation");
	ImGui::SliderFloat("Noise Scale", &config.noise_scale, 0.001f, 1.0f, "%.4f");

	ImGui::SeparatorText("Froxel Ranges");
	ImGui::SliderFloat("Fog Near", &config.fog_near, 0.01f, 10.0f);
	ImGui::SliderFloat("Fog Far", &config.fog_far, 10.0f, 1000.0f);
}