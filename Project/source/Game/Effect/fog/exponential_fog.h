#pragma once

#include "fog_base.h"
#include <DirectXMath.h>
/**
 * @brief 指数フォグ（Exponential Fog）
 *
 * カメラからの距離に対して指数関数的にフォグ密度を計算する。
 * 遠景が自然にフェードアウトする屋外シーン向け。
 *
 * 密度式(Exp):   fog = 1 - exp( -density * dist )
 * 密度式(Exp2):  fog = 1 - exp( -(density * dist)^2 )
 *
 * シェーダー: exponential_fog_ps.cso
 * 定数バッファ スロット: b0
 */
class ExponentialFog : public FogBase
{
public:
	enum class Mode : int
	{
		Exponential = 0,  // exp
		Exponential2 = 1,  // exp^2 (より急峻な減衰)
	};

	// ── HLSL 側定数バッファ (b0) と 1:1 対応 ──────────────────────
	struct alignas(16) Config
	{
		float density = 0.0026f;    // 密度係数（小さいほど薄い）
		float intensity = 1.0f;     // フォグ輝度乗数
		int   mode = static_cast<int>(Mode::Exponential2);
		int   is_enabled = 1;

		float fog_color[3] = { 0.7f, 0.8f, 0.9f }; // フォグ色 (linear)
		float padding = 0.0f;
	};
	static_assert(sizeof(Config) % 16 == 0, "ExponentialFog::Config must be 16-byte aligned");

	ExponentialFog(ID3D11Device* device, uint32_t width, uint32_t height);
	~ExponentialFog() override = default;

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
