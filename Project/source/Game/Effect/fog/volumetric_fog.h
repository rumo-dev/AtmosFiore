#pragma once

#include "fog_base.h"
#include <DirectXMath.h>

/**
 * @brief ボリュームフォグ
 *
 * レイマーチング + FBM ノイズ + 各光源の散乱を計算する物理ベースフォグ。
 * 既存の deferred lighting pass 内実装をポストプロセスパスに分離したもの。
 *
 * パイプライン上の位置:
 *   Sky → [VolumetricFog] → DoF → Exposure → Bloom → ...
 *
 * シェーダー: volumetric_fog_ps.cso
 * 定数バッファ スロット: b0
 */
class VolumetricFog : public FogBase
{
public:
	// ── HLSL 側定数バッファ (b0) と 1:1 対応 ──────────────────────
	struct alignas(16) Config
	{
		int   step_count = 16;       // レイマーチングステップ数 [4..64]
		float density_base = 0.02f;    // 基底密度
		float scattering = 0.8f;     // 散乱係数
		float absorption = 0.002f;   // 吸収係数

		float anisotropy = 0.65f;    // Henyey-Greenstein g 値 [-1..1]
		float height_falloff = 0.15f;    // 高さ方向の密度減衰
		float noise_scale = 0.5f;     // FBM ノイズスケール
		float intensity = 1.0f;     // フォグ全体の輝度乗数

		int   is_enabled = 1;        // 有効フラグ
		float padding[3]{};
	};
	static_assert(sizeof(Config) % 16 == 0, "VolumetricFog::Config must be 16-byte aligned");

	VolumetricFog(ID3D11Device* device, uint32_t width, uint32_t height);
	~VolumetricFog() override = default;

	void make(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src_srv) override;

	ID3D11ShaderResourceView* get_color_map()         const override;
	ID3D11ShaderResourceView** get_color_map_address()  const override;

	void DrawDebugUI() override;

	// パラメータ（ImGui から直接操作可能にする）
	Config config{};

private:
	std::unique_ptr<FullscreenQuad> blit_;
	std::unique_ptr<Framebuffer>    target_;

	Microsoft::WRL::ComPtr<ID3D11PixelShader> ps_;
	Microsoft::WRL::ComPtr<ID3D11Buffer>      cb_;
};
