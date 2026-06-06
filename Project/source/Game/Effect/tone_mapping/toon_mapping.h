#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/shader/shader.h"
#include "Engine/utilities/misc.h"

class ToneMapping
{
public:
	// シェーダー定数バッファ構造体（HLSL と同期）
	struct ToneMappingConfig {
		int mapping_type = 0;
		float exposure = 1.0f;
		float gamma = 2.2f;
		float m_param = 0.22f;

		float max_white = 10.0f;
		float shoulder = 0.22f;
		float linear_strength = 0.30f;
		float linear_angle = 0.10f;

		float toe_strength = 0.20f;
		int is_enabled = 1;
		float padding[2]{};
	};
	enum class ToneMappingType {
		ACES = 0, Reinhard,
		Unreal, Neutral,
		Linear, Hable,
		AgX, GT,
		Drago, Exponential,
		Logarithmic, Ward,
		Lottes, Hejl,
		RomBinDaHouse, ReinhardExtended,
		FilmicSimple, ACESApprox,
		PBRNeutral, Sigmoid,
		Piecewise, Cineon,
		Exposure, GammaOnly,
		PQApprox, HLGApprox,
		OpenDRTLike, CameraResponse,
		Uchimura, ClampOnly,
		WhitePreservingLuma, FilmicALU,
		AgXPunchy, CustomCurve
	};

	ToneMapping(ID3D11Device* device, uint32_t width, uint32_t height);
	~ToneMapping() = default;

	// 露出調整済みのHDRカラーをトーンマッピング・ガンマ補正する
	void make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* adjusted_hdr_srv);

	// 最終的なSDR描画結果を取得 (Format: R8G8B8A8_UNORM)
	ID3D11ShaderResourceView* get_color_map() const { return tone_mapping_target->GetColorMap(); }
	ID3D11ShaderResourceView** get_color_map_address() const { return tone_mapping_target->GetColorMapAddress(); }

	ToneMappingType mapping_type = ToneMappingType::ACES;
	float exposure = 1.0f;
	float gamma = 2.2f;
	float gt_param = 0.22f;
	float max_white = 10.0f;
	float shoulder = 0.22f;
	float linear_strength = 0.30f;
	float linear_angle = 0.10f;
	float toe_strength = 0.20f;
	int is_enabled = true;
private:
	std::unique_ptr<FullscreenQuad> bit_block_transfer;
	std::unique_ptr<Framebuffer>     tone_mapping_target;

	// シェーダー
	Microsoft::WRL::ComPtr<ID3D11PixelShader> tone_mapping_ps;

	// 定数バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> cb_config;
};
