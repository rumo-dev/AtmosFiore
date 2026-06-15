// BLOOM
#pragma once

#include <memory>
#include <d3d11.h>
#include <wrl.h>
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/shader/shader.h"
#include "Engine/utilities/misc.h"

#include "Engine/Graphics/UI/DebugMenu/CustomWidgets.h"

/**
 * @brief Bloom（ブルーム）ポストエフェクトクラス
 *
 * @details
 * 高輝度部分を抽出し、ガウシアンブラーを複数段階で適用して
 * 元画像に合成することで発光表現を実現する。
 *
 * 処理フロー：
 * 1. 輝度抽出（threshold）
 * 2. ダウンサンプリング + ブラー
 * 3. アップサンプリング + 合成
 * 4. 最終合成
 *
 * @note
 * ・ピンポンバッファ構造（[N][2]）でブラーを効率化
 * ・downsample段数を固定することで安定したパフォーマンス
 * ・Fullscreen Quad を使ったポストプロセス前提設計
 *
 * @warning
 * ・入力SRV(color_map)はこの処理中にRTVとしてバインドしてはいけない
 * ・同一リソースの SRV/RTV 同時使用は未定義動作（DX11制約）
 * ・解像度が極端に小さいと downsample で破綻する可能性あり
 */
class bloom
{
public:

	/**
	 * @brief コンストラクタ
	 * @param device D3Dデバイス
	 * @param width バッファ幅
	 * @param height バッファ高さ
	 *
	 * @note
	 * 内部で各FramebufferとShaderを生成する
	 */
	bloom(ID3D11Device* device, uint32_t width, uint32_t height);

	~bloom() = default;

	bloom(const bloom&) = delete;
	bloom& operator =(const bloom&) = delete;
	bloom(bloom&&) noexcept = delete;
	bloom& operator =(bloom&&) noexcept = delete;

	/**
	 * @brief Bloom処理実行
	 * @param immediate_context デバイスコンテキスト
	 * @param color_map 入力カラーマップ（HDR想定）
	 *
	 * @details
	 * color_map から輝度抽出し、ブラー処理を経て最終結果を生成する
	 *
	 * @code
	 * bloom bloomFx(device, width, height);
	 *
	 * // 描画後
	 * bloomFx.make(context, hdrColorSRV);
	 *
	 * // 結果を次パスへ
	 * context->PSSetShaderResources(0, 1, bloomFx.GetColorMapAddress());
	 * @endcode
	 *
	 * @warning
	 * ・color_map は nullptr であってはならない
	 * ・呼び出し前に適切なビューポート設定が必要
	 */
	void make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map);

	/**
	 * @brief 最終Bloom結果のSRV取得
	 */
	ID3D11ShaderResourceView* getColorMap() const
	{
		return bloom_final->GetColorMap();
	}

	/**
	 * @brief SRVポインタのアドレス取得（PSSetShaderResources用）
	 *
	 * @note
	 * Direct3D API呼び出しの利便性のために提供
	 */
	ID3D11ShaderResourceView** GetColorMapAddress() const
	{
		return bloom_final->GetColorMapAddress();
	}

	/**
	 * @brief 最終合成用ピクセルシェーダ取得
	 */
	ID3D11PixelShader* get_pss() { return bloom_final_ps.Get(); };

public:

	/** @brief Bloom有効フラグ */
	int is_bloom = 1;

	/** @brief 輝度抽出しきい値 */
	float bloom_extraction_threshold = 1.f;

	/** @brief Bloom強度 */
	float bloom_intensity = 0.673f;
	// Bloom.h の class bloom に追加
public:
	std::vector<CustomUI::ImageAsset> GetDebugAssets() {
		std::vector<CustomUI::ImageAsset> assets;

		// ImTextureID への明示的なキャストを追加
		auto to_tex = [](ID3D11ShaderResourceView* srv) -> ImTextureID {
			return (ImTextureID)srv;
			};

		// 1. 輝度抽出バッファ
		assets.push_back(CustomUI::ImageAsset(
			to_tex(glow_extraction->GetColorMap()),
			ImVec2(1280.0f, 720.0f),
			"Glow Extraction"
		));

		// 2. ダウンサンプリング・ブラー結果
		for (size_t i = 0; i < downsampled_count; ++i) {
			std::string name = "Blur Level " + std::to_string(i);

			assets.push_back(CustomUI::ImageAsset(
				to_tex(gaussian_blur[i][0]->GetColorMap()),
				ImVec2((float)(1280 >> (i + 1)), (float)(720 >> (i + 1))),
				name
			));
		}

		// 3. 最終出力バッファ
		assets.push_back(CustomUI::ImageAsset(
			to_tex(bloom_final->GetColorMap()),
			ImVec2(1280.0f, 720.0f),
			"Bloom Final"
		));

		return assets;
	}
private:

	///< フルスクリーン描画用
	std::unique_ptr<FullscreenQuad> bit_block_transfer;

	///< 輝度抽出用バッファ
	std::unique_ptr<Framebuffer> glow_extraction;

	/**
	 * @brief ダウンサンプル段数
	 *
	 * @note
	 * 多いほど品質向上だがコスト増加
	 */
	static const size_t downsampled_count = 6;

	/**
	 * @brief ガウシアンブラー用ピンポンバッファ
	 * [level][0:水平 or 1:垂直]
	 */
	std::unique_ptr<Framebuffer> gaussian_blur[downsampled_count][2];

	///< 最終出力
	std::unique_ptr<Framebuffer> bloom_final;

	///< 各種シェーダ
	Microsoft::WRL::ComPtr<ID3D11PixelShader> glow_extraction_ps;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> gaussian_blur_downsampling_ps;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> gaussian_blur_horizontal_ps;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> gaussian_blur_vertical_ps;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> gaussian_blur_upsampling_ps;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> bloom_final_ps;

	/**
	 * @brief 定数バッファ構造体
	 *
	 * @warning
	 * HLSL側とメモリレイアウトを完全一致させる必要あり（16byteアラインメント）
	 */
	struct alignas(16) bloom_constants
	{
		int is_bloom;
		float bloom_extraction_threshold;
		float bloom_intensity;
		float something; ///< パディング or 拡張用
	};
	static_assert(sizeof(bloom_constants) % 16 == 0, "bloom_constants must be 16-byte aligned");
	///< 定数バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
};