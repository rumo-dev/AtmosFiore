// BLOOM
#include "fog.h"
#include "Engine/System/Manager/resource_manager.h"
#include "Engine/System/framework.h"
#include<vector>

#include"Engine/system/render_state.h"
Fog::Fog(ID3D11Device* device, uint32_t width, uint32_t height)
{
	bit_block_transfer = std::make_unique<FullscreenQuad>(device);
	result = std::make_unique<Framebuffer>(
		device,
		width,
		height, DXGI_FORMAT_R16G16B16A16_FLOAT,
		false

	);

	HRESULT hr{ S_OK };
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffer.GetAddressOf());
	fog_constans.FogColorNear = DirectX::XMFLOAT4{ 0.5f, 0.5f, 0.5f, 1.0f };
	fog_constans.FogColorFar = DirectX::XMFLOAT4{ 0.7f, 0.7f, 0.7f, 1.0f };
	fog_constans.FogDensity = 0.02f;
	fog_constans.HeightDensity = 0.02f;
	fog_constans.FogHeight = 0.0f;
	fog_constans.LightScatter = 0.5f;
	fog_constans.Time = 0.0f;
	fog_constans.NoiseScale = 0.1f;
	fog_constans.NoiseAmount = 0.5f;
	fog_constans.is_enabled = 1;

}

void Fog::make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map, ID3D11ShaderResourceView* deapth_map)
{
	// Store current states
	ID3D11ShaderResourceView* cached_shader_resource_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
	immediate_context->PSGetShaderResources(0, 1, cached_shader_resource_views);

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  cached_depth_stencil_state;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState>  cached_rasterizer_state;
	Microsoft::WRL::ComPtr<ID3D11BlendState>  cached_blend_state;

	FLOAT blend_factor[4];
	UINT sample_mask;
	immediate_context->OMGetDepthStencilState(cached_depth_stencil_state.GetAddressOf(), 0);
	immediate_context->RSGetState(cached_rasterizer_state.GetAddressOf());
	immediate_context->OMGetBlendState(cached_blend_state.GetAddressOf(), blend_factor, &sample_mask);

	Microsoft::WRL::ComPtr<ID3D11Buffer>  cached_constant_buffer;
	immediate_context->PSGetConstantBuffers(8, 1, cached_constant_buffer.GetAddressOf());

	// Bind states
	Render_State::instance().set_2d_render_states(immediate_context);


	immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &fog_constans, 0, 0);
	immediate_context->PSSetConstantBuffers(8, 1, constant_buffer.GetAddressOf());


	result->Clear(immediate_context, 1, 1, 1, 1);
	result->Activate(immediate_context);
	{
		ID3D11ShaderResourceView* shader_resource_views[2]{
			color_map,
			deapth_map
		};
		bit_block_transfer->Blit(immediate_context, shader_resource_views, 0, 2, Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("FOG_PS"));
	}


	result->Deactivate(immediate_context);
	// Restore states
	immediate_context->PSSetConstantBuffers(8, 1, cached_constant_buffer.GetAddressOf());

	immediate_context->OMSetDepthStencilState(cached_depth_stencil_state.Get(), 0);
	immediate_context->RSSetState(cached_rasterizer_state.Get());
	immediate_context->OMSetBlendState(cached_blend_state.Get(), blend_factor, sample_mask);

	immediate_context->PSSetShaderResources(0, 1, cached_shader_resource_views);
	for (ID3D11ShaderResourceView* cached_shader_resource_view : cached_shader_resource_views)
	{
		if (cached_shader_resource_view) cached_shader_resource_view->Release();
	}




}

