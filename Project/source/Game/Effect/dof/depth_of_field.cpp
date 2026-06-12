#include "depth_of_field.h"

#include"Engine/system/render_state.h"

#include"Engine/System/Manager/resource_manager.h"

DepthOfField::DepthOfField(ID3D11Device* device, uint32_t w, uint32_t h) : width(w), height(h)
{
	quad = std::make_unique <FullscreenQuad>(device);

	final_color = std::make_unique <Framebuffer>(device, w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, false);

	create_rw_texture(device, w, h, DXGI_FORMAT_R16_FLOAT, linear_depth, linear_srv, linear_uav);

	create_rw_texture(device, w, h, DXGI_FORMAT_R16_FLOAT, coc, coc_srv, coc_uav);
	create_rw_texture(device, w / 2, h / 2, DXGI_FORMAT_R16G16B16A16_FLOAT, near_blur, near_srv, near_uav);
	create_rw_texture(device, w / 2, h / 2, DXGI_FORMAT_R16G16B16A16_FLOAT, far_blur, far_srv, far_uav);

	create_rw_texture(device, w / 16, h / 16, DXGI_FORMAT_R16_FLOAT, tile, tile_srv, tile_uav);
}

void DepthOfField::create_rw_texture(
	ID3D11Device* device,
	UINT w,
	UINT h,
	DXGI_FORMAT format,
	Microsoft::WRL::ComPtr<ID3D11Texture2D>& tex,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv,
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>& uav)
{
	D3D11_TEXTURE2D_DESC d{};

	d.Width = w;

	d.Height = h;

	d.MipLevels = 1;

	d.ArraySize = 1;

	d.Format = format;

	d.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	d.SampleDesc.Count = 1;

	device->CreateTexture2D(&d, nullptr, tex.GetAddressOf());

	device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());

	device->CreateUnorderedAccessView(tex.Get(), nullptr, uav.GetAddressOf());
}

void DepthOfField::make(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* hdr, ID3D11ShaderResourceView* depth)
{
	ID3D11ShaderResourceView* nullsrv[4]{};

	ID3D11UnorderedAccessView* nulluav{};

	Render_State::instance().set_2d_render_states(ctx);

	//
	// Linear
	//
	ctx->CSSetShaderResources(0, 1, &depth);

	ctx->CSSetUnorderedAccessViews(0, 1, linear_uav.GetAddressOf(), 0);

	ctx->CSSetShader(Resource_Manager::instance().shader_manager.GetNative			<Compute_Shader>("DOF_LINEAR_DEPTH_CS"), 0, 0);

	ctx->Dispatch((width + 15) / 16, (height + 15) / 16, 1);

	//
	// CoC
	//

	ctx->CSSetShaderResources(0, 1, linear_srv.GetAddressOf());
	ctx->CSSetUnorderedAccessViews(0, 1, coc_uav.GetAddressOf(), 0);
	ctx->CSSetShader(Resource_Manager::instance().shader_manager.GetNative			<Compute_Shader>("DOF_COC_CS"), 0, 0);

	ctx->Dispatch((width + 15) / 16, (height + 15) / 16, 1);
	ctx->CSSetShaderResources(0, 4, nullsrv);

	ctx->CSSetUnorderedAccessViews(0, 1, &nulluav, 0);
	//
	// Composite
	//
	final_color->Clear(Graphics_Core::instance().get_device_context(), 1, 1, 1, 1);
	final_color->Activate(ctx);

	ID3D11ShaderResourceView* srv[4]{ hdr,	near_srv.Get(),	far_srv.Get(),	coc_srv.Get() };

	quad->Blit(ctx, srv, 0, 4, Resource_Manager::instance().shader_manager.GetNative		<Pixel_Shader>("DOF_COMPOSITE_PS"));

	final_color->Deactivate(ctx);

	ctx->PSSetShaderResources(0, 4, nullsrv);
}
