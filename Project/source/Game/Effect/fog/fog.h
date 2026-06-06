// BLOOM
#pragma once

#include <memory>
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl.h>
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/shader/shader.h"
#include "Engine/utilities/misc.h"

class Fog
{
public:
	Fog(ID3D11Device* device, uint32_t width, uint32_t height);
	~Fog() = default;
	Fog(const Fog&) = delete;
	Fog& operator =(const Fog&) = delete;
	Fog(Fog&&) noexcept = delete;
	Fog& operator =(Fog&&) noexcept = delete;

	void make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map, ID3D11ShaderResourceView* deapth_map);
	ID3D11ShaderResourceView* getColorMap() const
	{
		return result->GetColorMap();
	}
	ID3D11ShaderResourceView** GetColorMapAddress() const
	{
		return result->GetColorMapAddress();
	}


private:
	std::unique_ptr<FullscreenQuad> bit_block_transfer;


	std::unique_ptr<Framebuffer> result;




	struct constants
	{
		DirectX::XMFLOAT4 FogColorNear; // xyz: color, w: unused

		DirectX::XMFLOAT4 FogColorFar;



		float FogDensity; // 距離フォグ
		float HeightDensity; // 高さフォグ
		float FogHeight; // 基準高さ
		float LightScatter; // 散乱強度


		float Time;
		float NoiseScale;
		float NoiseAmount;
		int is_enabled;
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
public:
	constants fog_constans;
};
