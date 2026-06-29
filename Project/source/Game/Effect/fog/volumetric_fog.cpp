#include "volumetric_fog.h"
#include "Engine/system/render_state.h"
#include "Engine/Graphics/shader/shader.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"

#include "Engine/System/graphics_core.h"

VolumetricFog::VolumetricFog(ID3D11Device* device, uint32_t width, uint32_t height)
{
	blit_ = std::make_unique<FullscreenQuad>(device);
	target_ = std::make_unique<Framebuffer>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, false);

	create_ps_from_cso(device, "volumetric_fog_ps.cso", ps_.GetAddressOf());

	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.ByteWidth = sizeof(Config);
	device->CreateBuffer(&desc, nullptr, cb_.GetAddressOf());
}

void VolumetricFog::make(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src_srv)
{
	// 前段 SRV をアンバインド
	ID3D11ShaderResourceView* null_srv = nullptr;
	ctx->PSSetShaderResources(0, 1, &null_srv);

	Render_State::instance().set_2d_render_states(ctx);

	// 有効フラグを Config に反映
	config.is_enabled = is_enabled ? 1 : 0;

	ctx->UpdateSubresource(cb_.Get(), 0, nullptr, &config, 0, 0);
	ctx->PSSetConstantBuffers(8, 1, cb_.GetAddressOf());
	ID3D11ShaderResourceView* srvs[GBUFFER_COUNT + 4]{};
	Graphics_Core::instance().get_geometry_buffer()->GetShaderResourceViews(srvs);
	srvs[GBUFFER_COUNT + 0] = src_srv;
	srvs[GBUFFER_COUNT + 1] = Graphics_Core::instance().post_procss.GetShadow().get_point_shadow_front_map();
	srvs[GBUFFER_COUNT + 2] = Graphics_Core::instance().post_procss.GetShadow().get_point_shadow_back_map();
	srvs[GBUFFER_COUNT + 3] = Graphics_Core::instance().post_procss.GetShadow().get_directional_shadow_map();
	target_->Clear(ctx, 0, 0, 0, 1);
	target_->Activate(ctx);
	{
		blit_->Blit(ctx, srvs, 0, (GBUFFER_COUNT + 4), ps_.Get());
	}
	target_->Deactivate(ctx);

	ctx->PSSetShaderResources(0, 1, &null_srv);
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

	ImGui::SeparatorText("Volumetric Settings");
	ImGui::SliderInt("Step Count", &config.step_count, 16, 128);
	ImGui::SliderFloat("Density Base", &config.density_base, 0.0f, 1.0f);
	ImGui::SliderFloat("Scattering", &config.scattering, 0.0f, 1.0f);
	ImGui::SliderFloat("Absorption", &config.absorption, 0.0f, 1.0f);

	ImGui::SeparatorText("Light & Phase");
	ImGui::SliderFloat("Anisotropy (g)", &config.anisotropy, -0.99f, 0.99f);
	ImGui::SliderFloat("Intensity", &config.intensity, 0.0f, 10.0f);

	ImGui::SeparatorText("Noise & Animation");
	ImGui::SliderFloat("Noise Scale", &config.noise_scale, 0.001f, 1.0f, "%.4f");

}