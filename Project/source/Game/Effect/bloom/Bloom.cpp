// BLOOM
#include "bloom.h"

#include<vector>

#include"Engine/system/render_state.h"
bloom::bloom(ID3D11Device* device, uint32_t width, uint32_t height)
{
	bit_block_transfer = std::make_unique<FullscreenQuad>(device);

	glow_extraction = std::make_unique<Framebuffer>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT,
		false
	);

	for (size_t downsampled_index = 0; downsampled_index < downsampled_count; ++downsampled_index)
	{
		gaussian_blur[downsampled_index][0] = std::make_unique<Framebuffer>(
			device,
			width >> downsampled_index,
			height >> downsampled_index, DXGI_FORMAT_R16G16B16A16_FLOAT,
			false

		);
		gaussian_blur[downsampled_index][1] = std::make_unique<Framebuffer>(
			device,
			width >> downsampled_index,
			height >> downsampled_index, DXGI_FORMAT_R16G16B16A16_FLOAT,
			false

		);
	}
	bloom_final = std::make_unique<Framebuffer>(
		device,
		width,
		height, DXGI_FORMAT_R16G16B16A16_FLOAT,
		0,
		false

	);
	create_ps_from_cso(device, "glow_extraction_ps.cso", glow_extraction_ps.GetAddressOf());
	create_ps_from_cso(device, "gaussian_blur_downsampling_ps.cso", gaussian_blur_downsampling_ps.GetAddressOf());
	create_ps_from_cso(device, "gaussian_blur_horizontal_ps.cso", gaussian_blur_horizontal_ps.GetAddressOf());
	create_ps_from_cso(device, "gaussian_blur_vertical_ps.cso", gaussian_blur_vertical_ps.GetAddressOf());
	create_ps_from_cso(device, "gaussian_blur_upsampling_ps.cso", gaussian_blur_upsampling_ps.GetAddressOf());
	create_ps_from_cso(device, "bloomfinal.cso", bloom_final_ps.GetAddressOf());

	HRESULT hr{ S_OK };
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(bloom_constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffer.GetAddressOf());

}

void bloom::make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map)
{
	// Store current states
	ID3D11ShaderResourceView* null_shader_resource_view{};
	ID3D11ShaderResourceView* cached_shader_resource_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
	immediate_context->PSGetShaderResources(0, downsampled_count, cached_shader_resource_views);

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

	bloom_constants data{};
	data.is_bloom = is_bloom;
	data.bloom_extraction_threshold = bloom_extraction_threshold;
	data.bloom_intensity = bloom_intensity;
	immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &data, 0, 0);
	immediate_context->PSSetConstantBuffers(8, 1, constant_buffer.GetAddressOf());

	// Extracting bright color
	glow_extraction->Clear(immediate_context, 0, 0, 0, 1);
	glow_extraction->Activate(immediate_context);
	bit_block_transfer->Blit(immediate_context, &color_map, 0, 1, glow_extraction_ps.Get());
	glow_extraction->Deactivate(immediate_context);
	immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);

	// Gaussian blur
	// Efficient Gaussian blur with linear sampling
	// http://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
	// Downsampling
	gaussian_blur[0][0]->Clear(immediate_context, 0, 0, 0, 1);
	gaussian_blur[0][0]->Activate(immediate_context);
	bit_block_transfer->Blit(immediate_context, glow_extraction->GetColorMapAddress(), 0, 1, gaussian_blur_downsampling_ps.Get());
	gaussian_blur[0][0]->Deactivate(immediate_context);
	immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);

	// Ping-pong gaussian blur
	gaussian_blur[0][1]->Clear(immediate_context, 0, 0, 0, 1);
	gaussian_blur[0][1]->Activate(immediate_context);
	bit_block_transfer->Blit(immediate_context, gaussian_blur[0][0]->GetColorMapAddress(), 0, 1, gaussian_blur_horizontal_ps.Get());
	gaussian_blur[0][1]->Deactivate(immediate_context);
	immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);

	gaussian_blur[0][0]->Clear(immediate_context, 0, 0, 0, 1);
	gaussian_blur[0][0]->Activate(immediate_context);
	bit_block_transfer->Blit(immediate_context, gaussian_blur[0][1]->GetColorMapAddress(), 0, 1, gaussian_blur_vertical_ps.Get());
	gaussian_blur[0][0]->Deactivate(immediate_context);
	immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);

	for (size_t downsampled_index = 1; downsampled_index < downsampled_count; ++downsampled_index)
	{
		// Downsampling
		gaussian_blur[downsampled_index][0]->Clear(immediate_context, 0, 0, 0, 1);
		gaussian_blur[downsampled_index][0]->Activate(immediate_context);
		bit_block_transfer->Blit(immediate_context, gaussian_blur[downsampled_index - 1][0]->GetColorMapAddress(), 0, 1, gaussian_blur_downsampling_ps.Get());
		gaussian_blur[downsampled_index][0]->Deactivate(immediate_context);
		immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);

		// Ping-pong gaussian blur
		gaussian_blur[downsampled_index][1]->Clear(immediate_context, 0, 0, 0, 1);
		gaussian_blur[downsampled_index][1]->Activate(immediate_context);
		bit_block_transfer->Blit(immediate_context, gaussian_blur[downsampled_index][0]->GetColorMapAddress(), 0, 1, gaussian_blur_horizontal_ps.Get());
		gaussian_blur[downsampled_index][1]->Deactivate(immediate_context);
		immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);

		gaussian_blur[downsampled_index][0]->Clear(immediate_context, 0, 0, 0, 1);
		gaussian_blur[downsampled_index][0]->Activate(immediate_context);
		bit_block_transfer->Blit(immediate_context, gaussian_blur[downsampled_index][1]->GetColorMapAddress(), 0, 1, gaussian_blur_vertical_ps.Get());
		gaussian_blur[downsampled_index][0]->Deactivate(immediate_context);
		immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);
	}

	// Downsampling
	glow_extraction->Clear(immediate_context, 0, 0, 0, 1);
	glow_extraction->Activate(immediate_context); {


		std::vector<ID3D11ShaderResourceView*> shader_resource_views;
		for (size_t downsampled_index = 0; downsampled_index < downsampled_count; ++downsampled_index)
		{
			shader_resource_views.push_back(gaussian_blur[downsampled_index][0]->GetColorMap());
		}
		bit_block_transfer->Blit(immediate_context, shader_resource_views.data(), 0, downsampled_count, gaussian_blur_upsampling_ps.Get());
	}
	glow_extraction->Deactivate(immediate_context);
	immediate_context->PSSetShaderResources(0, 1, &null_shader_resource_view);
	// Final composite
	bloom_final->Clear(immediate_context, 1, 1, 1, 1);
	bloom_final->Activate(immediate_context);
	{
		ID3D11ShaderResourceView* shader_resource_views[2]{
			color_map,
			glow_extraction->GetColorMap() };
		bit_block_transfer->Blit(immediate_context, shader_resource_views, 0, 2, bloom_final_ps.Get());
	}


	bloom_final->Deactivate(immediate_context);
	// Restore states
	immediate_context->PSSetConstantBuffers(8, 1, cached_constant_buffer.GetAddressOf());

	immediate_context->OMSetDepthStencilState(cached_depth_stencil_state.Get(), 0);
	immediate_context->RSSetState(cached_rasterizer_state.Get());
	immediate_context->OMSetBlendState(cached_blend_state.Get(), blend_factor, sample_mask);

	immediate_context->PSSetShaderResources(0, downsampled_count, cached_shader_resource_views);
	for (ID3D11ShaderResourceView* cached_shader_resource_view : cached_shader_resource_views)
	{
		if (cached_shader_resource_view) cached_shader_resource_view->Release();
	}




}

