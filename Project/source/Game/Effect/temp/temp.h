#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/utilities/misc.h"

class $safeitemrootname$
{
public:
	struct alignas(16) Constants
	{};
	static_assert(sizeof(Constants) % 16 == 0, "$safeitemrootname$Constants must be 16-byte aligned");

	$safeitemrootname$(ID3D11Device* device, uint32_t width, uint32_t height);
	~$safeitemrootname$() = default;

	void make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* input_hdr_srv);

	ID3D11ShaderResourceView* get_color_map() const { return color_map->GetColorMap(); }

	ID3D11ShaderResourceView** get_color_map_Adress() const { return color_map->GetColorMapAddress(); }

private:
	std::unique_ptr<FullscreenQuad> bit_block_transfer;
	std::unique_ptr<Framebuffer>     color_map;
	// 定数バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> constants;
};
