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
		int   grid_width = 160;
		int   grid_height = 90;
		int   grid_depth = 128;
		int   pad_grid = 0;

		float density_base = 0.02f;
		float scattering = 0.8f;
		float absorption = 0.002f;
		float anisotropy = 0.65f;

		float noise_scale = 0.0f;
		float intensity = 1.0f;
		float fog_near = 0.1f;
		float fog_far = 60.0f;

		int   is_enabled = 1;
		float time = 0.0f;
		float padding[2]{};
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

	// Froxel 用コンピュートシェーダー
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> cs_injection_;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> cs_accumulation_;

	// 3D テクスチャ (ライト注入バッファ)
	Microsoft::WRL::ComPtr<ID3D11Texture3D>           light_volume_tex_;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> light_volume_uav_;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  light_volume_srv_;

	// 3D テクスチャ (累積バッファ)
	Microsoft::WRL::ComPtr<ID3D11Texture3D>           accum_volume_tex_;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> accum_volume_uav_;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  accum_volume_srv_;
};
