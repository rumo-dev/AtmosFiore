// Exposure.cpp
#include "Exposure.h"
#include "Engine/system/render_state.h"

Exposure::Exposure(ID3D11Device* device, uint32_t width, uint32_t height)
{
	bit_block_transfer = std::make_unique<FullscreenQuad>(device);

	exposure_buffer = std::make_unique<Framebuffer>(
		device, width, height,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		false
	);

	create_ps_from_cso(device, "exposure_ps.cso", exposure_ps.GetAddressOf());

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(ExposureConstants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	device->CreateBuffer(&buffer_desc, nullptr, constant_buffer.GetAddressOf());
}

void Exposure::make(ID3D11DeviceContext* immediate_context,
	ID3D11ShaderResourceView* color_map)
{
	// ---- ステート退避 ----
	ID3D11ShaderResourceView* null_srv{};
	ID3D11ShaderResourceView* cached_srvs[1]{};
	immediate_context->PSGetShaderResources(0, 1, cached_srvs);

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> cached_dss;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState>   cached_rs;
	Microsoft::WRL::ComPtr<ID3D11BlendState>        cached_bs;
	FLOAT blend_factor[4]; UINT sample_mask;
	immediate_context->OMGetDepthStencilState(cached_dss.GetAddressOf(), 0);
	immediate_context->RSGetState(cached_rs.GetAddressOf());
	immediate_context->OMGetBlendState(cached_bs.GetAddressOf(), blend_factor, &sample_mask);

	Microsoft::WRL::ComPtr<ID3D11Buffer> cached_cb;
	immediate_context->PSGetConstantBuffers(9, 1, cached_cb.GetAddressOf());

	// ---- ステート設定 ----
	Render_State::instance().set_2d_render_states(immediate_context);

	// ---- 定数バッファ更新 ----
	ExposureConstants data{};
	data.is_enabled = is_enabled ? 1 : 0;
	immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &data, 0, 0);
	immediate_context->PSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());

	// ---- 露出パス ----
	exposure_buffer->Clear(immediate_context, 0, 0, 0, 1);
	exposure_buffer->Activate(immediate_context);
	bit_block_transfer->Blit(immediate_context, &color_map, 0, 1, exposure_ps.Get());
	exposure_buffer->Deactivate(immediate_context);
	immediate_context->PSSetShaderResources(0, 1, &null_srv);

	// ---- ステート復元 ----
	immediate_context->PSSetConstantBuffers(9, 1, cached_cb.GetAddressOf());
	immediate_context->OMSetDepthStencilState(cached_dss.Get(), 0);
	immediate_context->RSSetState(cached_rs.Get());
	immediate_context->OMSetBlendState(cached_bs.Get(), blend_factor, sample_mask);
	immediate_context->PSSetShaderResources(0, 1, cached_srvs);
	if (cached_srvs[0]) cached_srvs[0]->Release();
}

void Exposure::DrawDebugUI()
{

	CustomUI::Checkbox("Enable", &is_enabled);

}