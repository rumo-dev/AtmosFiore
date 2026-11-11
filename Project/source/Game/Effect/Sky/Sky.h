#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/utilities/misc.h"

class Sky
{
public:
	struct alignas(16) Constants
	{
		float gTime = 0;
		float art_strength = 0;
		float directional_strength = 0;
		float fog_blend = 0;
		DirectX::XMFLOAT4 zenith_tint{1,1,1,1};
		DirectX::XMFLOAT4 horizon_tint{1,1,1,1};
		DirectX::XMFLOAT4 directional_tint{0,0,0,1};
		DirectX::XMFLOAT4 fog_tint{1,1,1,1};
		DirectX::XMFLOAT4 cloud_tint{1,1,1,1};
		DirectX::XMFLOAT4 paint_direction_vignette{1,0,0,0};
		DirectX::XMFLOAT4 cloud_controls{0.5f,1,0,0};
	};
	static_assert(sizeof(Constants) % 16 == 0, "SkyConstants must be 16-byte aligned");

	Sky(ID3D11Device* device, uint32_t width, uint32_t height);
	~Sky() = default;

	void make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* input_hdr_srv);
	void set_look(const Constants& look) { look_ = look; }
	Constants look() const { return look_; }

	ID3D11ShaderResourceView* get_color_map() const { return color_map->GetColorMap(); }

	ID3D11ShaderResourceView** get_color_map_Adress() const { return color_map->GetColorMapAddress(); }
	float time = 0;
private:
	Constants look_{};
	std::unique_ptr<FullscreenQuad> bit_block_transfer;
	std::unique_ptr<Framebuffer>     color_map;
	std::unique_ptr<Framebuffer>     skybox_map;
	Microsoft::WRL::ComPtr<ID3D11Buffer> skybox_vertices;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> skybox_input_layout;
	// 定数バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> constants;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> atmosphere_lut;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> atmosphere_lut_srv;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> transmittance_lut;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> transmittance_lut_srv;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> multiscattering_lut;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> multiscattering_lut_srv;
	Microsoft::WRL::ComPtr<ID3D11Texture3D> aerial_perspective_lut;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> aerial_perspective_lut_srv;
	Microsoft::WRL::ComPtr<ID3D11Texture3D> cloud_base_noise;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cloud_base_noise_srv;
	Microsoft::WRL::ComPtr<ID3D11Texture3D> cloud_detail_noise;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cloud_detail_noise_srv;
	void create_atmosphere_luts(ID3D11Device* device);
	void create_cloud_noise_textures(ID3D11Device* device);
};
