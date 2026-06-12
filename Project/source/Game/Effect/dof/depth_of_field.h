#pragma once

#include <memory>
#include <d3d11.h>
#include <wrl.h>

#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"

class DepthOfField
{
public:

	DepthOfField(
		ID3D11Device* device,
		uint32_t width,
		uint32_t height);

	void make(
		ID3D11DeviceContext* context,
		ID3D11ShaderResourceView* hdr,
		ID3D11ShaderResourceView* depth);

	ID3D11ShaderResourceView*
		GetColorMap()
	{
		return final_color->GetColorMap();
	}

private:

	// depth_of_field.h 内の修正
private:
	void create_rw_texture(
		ID3D11Device* device,
		UINT w, UINT h, // wとh両方必要ならここも修正
		DXGI_FORMAT format,
		Microsoft::WRL::ComPtr<ID3D11Texture2D>& tex,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv,
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>& uav);
private:

	uint32_t width;
	uint32_t height;

	std::unique_ptr<Framebuffer>		final_color;
	std::unique_ptr<FullscreenQuad>		quad;
	Microsoft::WRL::ComPtr<ID3D11Texture2D>		linear_depth;
	Microsoft::WRL::ComPtr<ID3D11Texture2D>		coc;
	Microsoft::WRL::ComPtr<ID3D11Texture2D>		near_blur;
	Microsoft::WRL::ComPtr<ID3D11Texture2D>		far_blur;
	Microsoft::WRL::ComPtr<ID3D11Texture2D>		tile;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>		linear_srv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>		coc_srv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>		near_srv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>		far_srv;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>		tile_srv;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>		linear_uav;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>		coc_uav;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>		near_uav;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>		far_uav;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>		tile_uav;
};
