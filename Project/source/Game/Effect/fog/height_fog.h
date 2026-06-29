#pragma once

#include "fog_base.h"
#include <DirectXMath.h>

/**
 * @brief 高さフォグ（Height Fog）
 *
 * ワールド空間の Y 座標に基づき指数的にフォグ密度を計算する。
 * 地面付近にのみ霞が掛かるような表現に適している。
 *
 * 密度式: density = density_max * exp( -falloff * max(y - base_height, 0) )
 *
 * シェーダー: height_fog_ps.cso
 * 定数バッファ スロット: b0
 */
class HeightFog : public FogBase
{
public:
	// ── HLSL 側定数バッファ (b0) と 1:1 対応 ──────────────────────
	struct alignas(16) Config
	{
		float base_height = 0.0f;     // フォグが最大密度になる Y 座標
		float falloff = 1.549;     // 高さ方向の密度減衰係数（大きいほど薄くなる）
		float density_max = 0.8f;     // base_height における最大密度 [0..1]
		float intensity = 0.022f;     // フォグ輝度乗数

		DirectX::XMFLOAT3 fog_color = { 0.7f, 0.8f, 0.9f }; // フォグ色 (linear)
		int    is_enabled = 1;

		float noise_scale = 0.6305f;
		float noise_strength = 2.785f;
		float time;
		float padding1;

		DirectX::XMFLOAT3 wind_velocity = { 0.473f,0,0 };
		float padding2;

		float padding[3]{};
	};
	static_assert(sizeof(Config) % 16 == 0, "HeightFog::Config must be 16-byte aligned");

	HeightFog(ID3D11Device* device, uint32_t width, uint32_t height);
	~HeightFog() override = default;

	void make(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src_srv) override;

	ID3D11ShaderResourceView* get_color_map()         const override;
	ID3D11ShaderResourceView** get_color_map_address()  const override;

	void DrawDebugUI() override;

	Config config{};

private:
	std::unique_ptr<FullscreenQuad> blit_;
	std::unique_ptr<Framebuffer>    target_;

	Microsoft::WRL::ComPtr<ID3D11PixelShader> ps_;
	Microsoft::WRL::ComPtr<ID3D11Buffer>      cb_;
};
