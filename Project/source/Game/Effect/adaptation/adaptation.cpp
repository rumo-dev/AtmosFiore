#include "adaptation.h"
#include "Engine/system/render_state.h"
#include "Engine/System/Manager/resource_manager.h"

Adaptation::Adaptation(ID3D11Device* device, uint32_t width, uint32_t height)
{
	bit_block_transfer = std::make_unique<FullscreenQuad>(device);
	color_map = std::make_unique<Framebuffer>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, false);

	// 1x1 露出テクスチャ (SRV / UAV) の作成
	// 初期値: 1.0f (露出なし) から開始
	D3D11_TEXTURE2D_DESC tex_desc{};
	tex_desc.Width = 1;
	tex_desc.Height = 1;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = 1;
	tex_desc.Format = DXGI_FORMAT_R32_FLOAT;
	tex_desc.SampleDesc.Count = 1;
	tex_desc.Usage = D3D11_USAGE_DEFAULT;
	tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	// 初期露出値: 中立的な1.0f
	float initial_exposure = 1.0f;
	D3D11_SUBRESOURCE_DATA init_data{};
	init_data.pSysMem = &initial_exposure;
	init_data.SysMemPitch = sizeof(float);
	init_data.SysMemSlicePitch = sizeof(float);
	HRESULT hr;

	hr = device->CreateTexture2D(&tex_desc, &init_data, exposure_texture.GetAddressOf());
	assert(SUCCEEDED(hr) && "CreateTexture2D failed");

	hr = device->CreateShaderResourceView(exposure_texture.Get(), nullptr, exposure_srv.GetAddressOf());
	assert(SUCCEEDED(hr) && "CreateSRV failed");

	hr = device->CreateUnorderedAccessView(exposure_texture.Get(), nullptr, exposure_uav.GetAddressOf());
	assert(SUCCEEDED(hr) && "CreateUAV failed");

	// 定数バッファの作成
	D3D11_BUFFER_DESC cb_desc{};
	cb_desc.Usage = D3D11_USAGE_DEFAULT;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.ByteWidth = sizeof(AdaptationConstants);
	hr = device->CreateBuffer(&cb_desc, nullptr, cb_adaptation.GetAddressOf());
	assert(SUCCEEDED(hr) && "CreateBuffer failed");
}

void Adaptation::make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* input_hdr_srv)
{
	// パイプラインの衝突を防ぐため、最初にすべての関連スロットを一度安全にクリアする
	ID3D11ShaderResourceView* null_srvs[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* null_uav = nullptr;

	immediate_context->CSSetShaderResources(0, 1, null_srvs);
	immediate_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
	immediate_context->PSSetShaderResources(0, 2, null_srvs);

	Render_State::instance().set_2d_render_states(immediate_context);

	// 1. 定数バッファの更新
	AdaptationConstants cb_data{};
	cb_data.delta_time = delta_time;
	cb_data.speed_to_light = speed_to_light;
	cb_data.speed_to_dark = speed_to_dark;
	cb_data.target_lum = target_lum;
	immediate_context->UpdateSubresource(cb_adaptation.Get(), 0, 0, &cb_data, 0, 0);

	// 2. Compute Shader での露出値更新
	immediate_context->CSSetShaderResources(0, 1, &input_hdr_srv);
	immediate_context->CSSetUnorderedAccessViews(0, 1, exposure_uav.GetAddressOf(), nullptr);
	immediate_context->CSSetConstantBuffers(0, 1, cb_adaptation.GetAddressOf());
	immediate_context->CSSetShader(Resource_Manager::instance().shader_manager.GetNative<Compute_Shader>("ADAPTATION_CS"), nullptr, 0);

	immediate_context->Dispatch(1, 1, 1);

	// Compute Shader と Pixel Shader 間のメモリ同期を確保
	ID3D11UnorderedAccessView* null_uavs[1] = { nullptr };
	immediate_context->CSSetUnorderedAccessViews(0, 1, null_uavs, nullptr);
	immediate_context->CSSetShaderResources(0, 1, null_srvs);
	immediate_context->CSSetShader(nullptr, nullptr, 0);

	// 3. Pixel Shader での露出適用
	color_map->Clear(immediate_context, 0, 0, 0, 1);
	color_map->Activate(immediate_context);
	{
		// ここでバインドするとき、上記でUAVが外れているため SRV（exposure_srv）が正常に認識されます
		ID3D11ShaderResourceView* srvs[2] = { input_hdr_srv, exposure_srv.Get() };
		bit_block_transfer->Blit(immediate_context, srvs, 0, 2, Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("ADAPTATION_PS"));
	}
	color_map->Deactivate(immediate_context);

	// 次のパスのために使い終わったSRVをクリア
	immediate_context->PSSetShaderResources(0, 2, null_srvs);
}


