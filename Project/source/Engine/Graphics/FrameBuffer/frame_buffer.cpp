#include "frame_buffer.h"
#include "Engine/Utilities/misc.h"

Framebuffer::Framebuffer(ID3D11Device* device, uint32_t width, uint32_t height, DXGI_FORMAT format/*format of render target*/, int mip_levels, bool use_depth, bool use_stencil)
{

	HRESULT hr{ S_OK };
	// レンダーターゲットバッファの作成
	Microsoft::WRL::ComPtr<ID3D11Texture2D> render_target_buffer;
	D3D11_TEXTURE2D_DESC texture2d_desc{};
	texture2d_desc.Width = width;
	texture2d_desc.Height = height;
	texture2d_desc.MipLevels = 1;
	texture2d_desc.ArraySize = 1;
	texture2d_desc.Format = format;
	texture2d_desc.SampleDesc.Count = 1;
	texture2d_desc.SampleDesc.Quality = 0;
	texture2d_desc.Usage = D3D11_USAGE_DEFAULT;
	texture2d_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texture2d_desc.CPUAccessFlags = 0;
	texture2d_desc.MiscFlags = mip_levels > 1 ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
	hr = device->CreateTexture2D(&texture2d_desc, 0, render_target_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	// レンダーターゲットビューの作成
	D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc{};
	render_target_view_desc.Format = texture2d_desc.Format;
	render_target_view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	hr = device->CreateRenderTargetView(render_target_buffer.Get(), &render_target_view_desc, m_renderTargetView.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	// シェーダーリソースビューの作成
	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
	shader_resource_view_desc.Format = texture2d_desc.Format;
	shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shader_resource_view_desc.Texture2D.MipLevels = 1;
	hr = device->CreateShaderResourceView(render_target_buffer.Get(), &shader_resource_view_desc, m_colorMap.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	// 深度ステンシルバッファの作成
	if (use_depth)
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_stencil_buffer;
		texture2d_desc.Format = use_stencil ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_R32_TYPELESS;
		texture2d_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		hr = device->CreateTexture2D(&texture2d_desc, 0, depth_stencil_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc{};
		depth_stencil_view_desc.Format = use_stencil ? DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_D32_FLOAT;
		depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depth_stencil_view_desc.Flags = 0;
		hr = device->CreateDepthStencilView(depth_stencil_buffer.Get(), &depth_stencil_view_desc, m_depthStencilView.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

		shader_resource_view_desc.Format = use_stencil ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_R32_FLOAT;
		shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		hr = device->CreateShaderResourceView(depth_stencil_buffer.Get(), &shader_resource_view_desc, m_depthMap.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	}
	// ビューポートの設定
	m_viewport.Width = static_cast<float>(width);
	m_viewport.Height = static_cast<float>(height);
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;
	m_viewport.TopLeftX = 0.0f;
	m_viewport.TopLeftY = 0.0f;
}
void Framebuffer::Clear(ID3D11DeviceContext* immediate_context,
	float r, float g, float b, float a, float depth)
{
	// レンダーターゲットのクリア
	float color[4]{ r, g, b, a };
	immediate_context->ClearRenderTargetView(m_renderTargetView.Get(), color);
	// 深度ステンシルのクリア
	if (m_depthStencilView)
	{
		immediate_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, depth, 0);
	}
}
void Framebuffer::Activate(ID3D11DeviceContext* immediate_context, ID3D11DepthStencilView* adopted_depth_stencil_view)
{
	// 現在のビューポートとレンダーターゲットをキャッシュ
	m_viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	immediate_context->RSGetViewports(&m_viewportCount, m_cachedViewports);
	immediate_context->OMGetRenderTargets(1, m_cachedRenderTargetView.ReleaseAndGetAddressOf(), m_cachedDepthStencilView.ReleaseAndGetAddressOf());
	// 新しいビューポートとレンダーターゲットを設定
	immediate_context->RSSetViewports(1, &m_viewport);
	immediate_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), adopted_depth_stencil_view ? adopted_depth_stencil_view : m_depthStencilView.Get());
}
void Framebuffer::Deactivate(ID3D11DeviceContext* immediate_context)
{
	// キャッシュしておいたビューポートとレンダーターゲットを復元
	immediate_context->RSSetViewports(m_viewportCount, m_cachedViewports);
	immediate_context->OMSetRenderTargets(1, m_cachedRenderTargetView.GetAddressOf(),
		m_cachedDepthStencilView.Get());
}

