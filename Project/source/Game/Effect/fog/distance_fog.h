#pragma once

#include "fog_base.h"

#include <DirectXMath.h>
/**
 * @brief 距離フォグ（Distance Fog）
 *
 * カメラからの距離に基づいてリニアにフォグを掛けるシンプルな実装。
 * 近距離・遠距離の明確なフォグ帯域を設定できる。
 *
 * 密度式: t = saturate( (dist - start) / (end - start) )
 *         fog = lerp(scene, fog_color, t * density)
 *
 * シェーダー: distance_fog_ps.cso
 * 定数バッファ スロット: b0
 */
class DistanceFog : public FogBase
{
public:
	// ── HLSL 側定数バッファ (b0) と 1:1 対応 ──────────────────────
	struct alignas(16) Config
	{
		float fog_start = 10.0f;    // フォグが始まるカメラ距離 [world unit]
		float fog_end = 1000.0f;   // フォグが最大密度になる距離 [world unit]
		float density = 1.0f;     // 最大到達時の密度乗数 [0..1]
		float intensity = 1.0f;     // フォグ輝度乗数

		float fog_color[3] = { 0.7f, 0.8f, 0.9f }; // フォグ色 (linear)
		int   is_enabled = 1;

		float padding[3]{};
	};
	static_assert(sizeof(Config) % 16 == 0, "DistanceFog::Config must be 16-byte aligned");

	DistanceFog(ID3D11Device* device, uint32_t width, uint32_t height);
	~DistanceFog() override = default;

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
