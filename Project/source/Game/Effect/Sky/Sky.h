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

		float    gTime;         // 経過時間 (iTime)
		DirectX::XMFLOAT3 pad;
	};
	static_assert(sizeof(Constants) % 16 == 0, "SkyConstants must be 16-byte aligned");

	Sky(ID3D11Device* device, uint32_t width, uint32_t height);
	~Sky() = default;

	void make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* input_hdr_srv);

	ID3D11ShaderResourceView* get_color_map() const { return color_map->GetColorMap(); }

	ID3D11ShaderResourceView** get_color_map_Adress() const { return color_map->GetColorMapAddress(); }
	float time = 0;
private:
	std::unique_ptr<FullscreenQuad> bit_block_transfer;
	std::unique_ptr<Framebuffer>     color_map;
	// 定数バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> constants;
};
