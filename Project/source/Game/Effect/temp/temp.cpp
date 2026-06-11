#include "$safeitemrootname$.h"
#include "Engine/system/render_state.h"
#include "Engine/System/Manager/resource_manager.h"

$safeitemrootname$::$safeitemrootname$(ID3D11Device* device, uint32_t width, uint32_t height)
{
	bit_block_transfer = std::make_unique<FullscreenQuad>(device);
	color_map = std::make_unique<Framebuffer>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, false);
	HRESULT hr{ S_OK };
	// 定数バッファの作成
	D3D11_BUFFER_DESC cb_desc{};
	cb_desc.Usage = D3D11_USAGE_DEFAULT;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.ByteWidth = sizeof(Constants);
	hr = device->CreateBuffer(&cb_desc, nullptr, constants.GetAddressOf());
	assert(SUCCEEDED(hr) && "CreateBuffer failed");
}

void $safeitemrootname$::make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* input_hdr_srv)
{
	// パイプラインの衝突を防ぐため、最初にすべての関連スロットを一度安全にクリアする
	ID3D11ShaderResourceView* null_srvs[2] = { nullptr, nullptr };
	immediate_context->PSSetShaderResources(0, 2, null_srvs);

	Render_State::instance().set_2d_render_states(immediate_context);

	// 1. 定数バッファの更新
	Constants cb_data{};
	immediate_context->UpdateSubresource(constants.Get(), 0, 0, &cb_data, 0, 0);
	immediate_context->PSSetConstantBuffers(8, 1, constants.GetAddressOf());

	color_map->Clear(immediate_context, 0, 0, 0, 1);
	color_map->Activate(immediate_context);
	{
		ID3D11ShaderResourceView* srvs[1] = {};
		bit_block_transfer->Blit(immediate_context, srvs, 0, 2, Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("_PS"));
	}
	color_map->Deactivate(immediate_context);

	immediate_context->PSSetShaderResources(0, 2, null_srvs);
}


