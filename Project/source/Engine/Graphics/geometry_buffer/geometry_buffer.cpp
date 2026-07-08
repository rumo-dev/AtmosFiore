// DEFERRED_RENDERING
#include "geometry_buffer.h"
#include "Engine/Utilities/misc.h"

#include "Engine/System/graphics_core.h"
#include "Game/World/Camera/camera_manager.h"
GeometryBuffer::GeometryBuffer(ID3D11Device* device, UINT width, UINT height)
{
	HRESULT hr = S_OK;

	DXGI_FORMAT formats[GBUFFER_COUNT] =
	{
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
	};

	// ★ 各Gバッファの用途名を定義
	const wchar_t* semantic_names[GBUFFER_COUNT] =
	{
		L"GBuffer0_Normal_Roughness",
		L"GBuffer1_Albedo_Metalness",
		L"GBuffer2_Position_AO",
		L"GBuffer3_Emissive_Specular"
	};

	for (size_t render_target_index = 0; render_target_index < GBUFFER_COUNT; ++render_target_index)
	{
		D3D11_TEXTURE2D_DESC texture2d_desc = {};
		texture2d_desc.Width = width;
		texture2d_desc.Height = height;
		texture2d_desc.MipLevels = 1;
		texture2d_desc.ArraySize = 1;
		texture2d_desc.Format = formats[render_target_index];
		texture2d_desc.SampleDesc.Count = 1;
		texture2d_desc.SampleDesc.Quality = 0;
		texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
		texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		texture2d_desc.CPUAccessFlags = 0;
		texture2d_desc.MiscFlags = 0;
		hr = device->CreateTexture2D(&texture2d_desc, 0, &m_renderTargetBuffers[render_target_index]);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc = {};
		render_target_view_desc.Format = texture2d_desc.Format;
		render_target_view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		hr = device->CreateRenderTargetView(m_renderTargetBuffers[render_target_index], &render_target_view_desc, &m_renderTargetViews[render_target_index]);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc = {};
		shader_resource_view_desc.Format = texture2d_desc.Format;
		shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shader_resource_view_desc.Texture2D.MipLevels = 1;
		hr = device->CreateShaderResourceView(m_renderTargetBuffers[render_target_index], &shader_resource_view_desc, &m_renderTargetShaderResourceViews[render_target_index]);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		// ★ 用途に沿った名前を動的に生成して設定
		std::wstring base_name = semantic_names[render_target_index];
		DX_SET_NAME(m_renderTargetBuffers[render_target_index], (base_name + L"_Tex").c_str());
		DX_SET_NAME(m_renderTargetViews[render_target_index], (base_name + L"_RTV").c_str());
		DX_SET_NAME(m_renderTargetShaderResourceViews[render_target_index], (base_name + L"_SRV").c_str());
	}

	D3D11_TEXTURE2D_DESC texture2d_desc = {};
	texture2d_desc.Width = width;
	texture2d_desc.Height = height;
	texture2d_desc.MipLevels = 1;
	texture2d_desc.ArraySize = 1;
	texture2d_desc.Format = DXGI_FORMAT_R32_TYPELESS;
	texture2d_desc.SampleDesc.Count = 1;
	texture2d_desc.SampleDesc.Quality = 0;
	texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
	texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	texture2d_desc.CPUAccessFlags = 0;
	texture2d_desc.MiscFlags = 0;
	hr = device->CreateTexture2D(&texture2d_desc, 0, &m_depthStencilBuffer);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// ★ 深度バッファ本体
	DX_SET_NAME(m_depthStencilBuffer, L"GBuffer_DepthBuffer_Tex");

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
	depth_stencil_view_desc.Format = DXGI_FORMAT_D32_FLOAT;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Flags = 0;
	hr = device->CreateDepthStencilView(m_depthStencilBuffer, &depth_stencil_view_desc, &m_depthStencilView);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// ★ DSV
	DX_SET_NAME(m_depthStencilView, L"GBuffer_DepthBuffer_DSV");

	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc = {};
	shader_resource_view_desc.Format = DXGI_FORMAT_R32_FLOAT;
	shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shader_resource_view_desc.Texture2D.MipLevels = 1;
	hr = device->CreateShaderResourceView(m_depthStencilBuffer, &shader_resource_view_desc, &m_depthStencilShaderResourceView);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// ★ 深度SRV
	DX_SET_NAME(m_depthStencilShaderResourceView, L"GBuffer_DepthBuffer_SRV");

	m_viewport.Width = static_cast<float>(width);
	m_viewport.Height = static_cast<float>(height);
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;
	m_viewport.TopLeftX = 0.0f;
	m_viewport.TopLeftY = 0.0f;
}
void GeometryBuffer::Clear(ID3D11DeviceContext* immediate_context)
{
	float color[4] = { 0, 0, 0, 0 };
	for (size_t render_target_index = 0; render_target_index < GBUFFER_COUNT; ++render_target_index)
	{
		immediate_context->ClearRenderTargetView(m_renderTargetViews[render_target_index], color);
	}
	if (Camera_Manager::instance().get_active_camera()->get_camera().isReversed_Z) {

		immediate_context->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH, 0, 1.0);
	}
	else {

		immediate_context->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	}

}
void GeometryBuffer::Activate(ID3D11DeviceContext* immediate_context)
{
	m_cachedViewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	immediate_context->RSGetViewports(&m_cachedViewportCount, m_cachedViewports);
	immediate_context->OMGetRenderTargets(GBUFFER_COUNT, m_cachedRenderTargetViews, &m_cachedDepthStencilView);

	immediate_context->RSSetViewports(1, &m_viewport);

	immediate_context->OMSetRenderTargets(GBUFFER_COUNT, m_renderTargetViews, m_depthStencilView);
}
void GeometryBuffer::Deactivate(ID3D11DeviceContext* immediate_context)
{
	immediate_context->RSSetViewports(m_cachedViewportCount, m_cachedViewports);
	immediate_context->OMSetRenderTargets(1, m_cachedRenderTargetViews, m_cachedDepthStencilView);
	for (size_t render_target_index = 0; render_target_index < GBUFFER_COUNT; ++render_target_index)
	{
		if (m_cachedRenderTargetViews[render_target_index])
		{
			m_cachedRenderTargetViews[render_target_index]->Release();
		}
	}
	if (m_cachedDepthStencilView)
	{
		m_cachedDepthStencilView->Release();
	}

}
