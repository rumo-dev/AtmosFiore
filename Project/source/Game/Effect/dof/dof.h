#pragma once
#include <memory>
#include <d3d11.h>
#include <wrl.h>
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/shader/shader.h"

class dof {
public:
	dof(ID3D11Device* device, uint32_t width, uint32_t height);
	~dof() = default;

	// カメラパラメータをセットしてDoFを適用
	void make(ID3D11DeviceContext* immediate_context,
		ID3D11ShaderResourceView* color_map);

	ID3D11ShaderResourceView* getColorMap() const { return dof_final->GetColorMap(); }
	ID3D11ShaderResourceView** get_color_map_address() const { return dof_final->GetColorMapAddress(); }

private:
	std::unique_ptr<FullscreenQuad> bit_block_transfer;
	std::unique_ptr<Framebuffer> dof_final;

};