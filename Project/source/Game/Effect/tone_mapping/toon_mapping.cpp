#include "toon_mapping.h"	
#include "Engine/system/render_state.h"

ToneMapping::ToneMapping(ID3D11Device* device, uint32_t width, uint32_t height)
{
	bit_block_transfer = std::make_unique<FullscreenQuad>(device);
	tone_mapping_target = std::make_unique<Framebuffer>(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, false);

	create_ps_from_cso(device, "tonemapping_ps.cso", tone_mapping_ps.GetAddressOf());

	D3D11_BUFFER_DESC cb_desc{};
	cb_desc.Usage = D3D11_USAGE_DEFAULT;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.ByteWidth = sizeof(ToneMappingConfig);
	device->CreateBuffer(&cb_desc, nullptr, cb_config.GetAddressOf());
}

void ToneMapping::make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* adjusted_hdr_srv)
{
	// SRV をクリア
	ID3D11ShaderResourceView* null_srv{};
	immediate_context->PSSetShaderResources(0, 1, &null_srv);

	Render_State::instance().set_2d_render_states(immediate_context);

	// 定数バッファの更新
	ToneMappingConfig cb_data{};
	cb_data.mapping_type = static_cast<int>(mapping_type);
	cb_data.is_enabled = is_enabled;
	cb_data.max_white = max_white;
	cb_data.m_param = gt_param;
	cb_data.exposure = exposure;
	cb_data.gamma = gamma;
	cb_data.shoulder = shoulder;
	cb_data.linear_strength = linear_strength;
	cb_data.linear_angle = linear_angle;
	cb_data.toe_strength = toe_strength;

	immediate_context->UpdateSubresource(cb_config.Get(), 0, 0, &cb_data, 0, 0);
	immediate_context->PSSetConstantBuffers(0, 1, cb_config.GetAddressOf());

	// トーンマッピングの実行
	tone_mapping_target->Clear(immediate_context, 0, 0, 0, 1);
	tone_mapping_target->Activate(immediate_context);
	{
		bit_block_transfer->Blit(immediate_context, &adjusted_hdr_srv, 0, 1, tone_mapping_ps.Get());
	}
	tone_mapping_target->Deactivate(immediate_context);

	// 次のパスのために SRV をクリア
	immediate_context->PSSetShaderResources(0, 1, &null_srv);
}
