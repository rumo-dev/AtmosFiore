#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/shader/shader.h"
#include "Engine/utilities/misc.h"

class Adaptation
{
public:
	// シェーダー定数バッファ構造体（HLSL と同期）
	struct AdaptationConstants
	{
		float delta_time;      // フレーム間の経過時間（秒）
		float speed_to_light;  // 明るい場所への順応スピード（秒^-1）
		float speed_to_dark;   // 暗い場所への順応スピード（秒^-1）
		float target_lum;      // 目標輝度（基準値: 0.18 = 中間グレー）
	};

	// 露出値のクランプ範囲定数
	static constexpr float MIN_EXPOSURE = 0.05f;
	static constexpr float MAX_EXPOSURE = 10.0f;
	static constexpr float MIN_LUMINANCE = 0.001f;

	Adaptation(ID3D11Device* device, uint32_t width, uint32_t height);
	~Adaptation() = default;

	// 自動露出を計算し、入力画像に適用して露出調整済みのHDRバッファを出力する
	void make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* input_hdr_srv);

	// 露出調整済みのHDRバッファを取得 (Format: R16G16B16A16_FLOAT)
	ID3D11ShaderResourceView* get_color_map() const { return color_map->GetColorMap(); }

	ID3D11ShaderResourceView** get_color_map_Adress() const { return color_map->GetColorMapAddress(); }

	// 調整パラメータ
	float delta_time = 0.0f;
	float speed_to_light = 2.0f;
	float speed_to_dark = 1.0f;
	float target_lum = 0.18f;

private:
	std::unique_ptr<FullscreenQuad> bit_block_transfer;
	std::unique_ptr<Framebuffer>     color_map;

	// 露出値を1フレーム保持・更新するための1x1テクスチャ (R32_FLOAT)
	Microsoft::WRL::ComPtr<ID3D11Texture2D>           exposure_texture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  exposure_srv;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> exposure_uav;

	// 定数バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> cb_adaptation;
};
