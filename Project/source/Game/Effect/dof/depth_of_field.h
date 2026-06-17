// DEPTH OF FIELD - Fast Filter Spreading (Scatter Method)
// Based on: "Cinematic Depth of Field" GDC 2017 - Hillesland & Skelton (AMD)
#pragma once

#include <memory>
#include <d3d11.h>
#include <wrl.h>
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/shader/shader.h"
#include "Engine/utilities/misc.h"

/**
 * @brief Depth of Field（被写界深度）ポストエフェクトクラス
 *
 * @details
 * AMD GDC 2017「Cinematic Depth of Field」で紹介された
 * Fast Filter Spreading（散乱方式）を用いた DoF 実装。
 *
 * DoF パラメータは CAMERA_CONSTANT_BUFFER (b1) から受け取る:
 *   FNumber       絞り値（例: 1.4, 2.8, 5.6）
 *   FocalLength   焦点距離 (mm)
 *   SensorSize    センサーサイズ (mm)
 *   FocusDist     フォーカス距離 (m)
 *   MaxBlurRadius 最大ボケ半径 (px)
 *
 * 処理フロー:
 *   color_map + depth_map
 *       ↓
 *   [Pass 1] coc_generation_ps   → coc_map (R=NearCoC G=FarCoC)
 *       ↓
 *   [Pass 2] field_separation_ps → near_field (premult α), far_field (MRT)
 *       ↓
 *   [Pass 3] ffs_horizontal_ps   → ffs_horizontal[near/far]
 *       ↓
 *   [Pass 4] ffs_vertical_ps     → ffs_vertical[near/far]（ボケ完成）
 *       ↓
 *   [Pass 5] composite_ps        → dof_final
 *
 * @note
 * ・CoC 計算は薄レンズモデル (GDC スライド準拠)
 * ・Near ボケは premultiplied alpha で蓄積 → 最終合成時に正規化
 * ・half_resolution = true で内部バッファを 1/2 解像度にして高速化
 *
 * @warning
 * ・color_map / depth_map は nullptr であってはならない
 * ・入力 SRV を RTV として同時バインドしてはならない（DX11 制約）
 */
class DepthOfField
{
public:
	/**
	 * @brief コンストラクタ
	 * @param device  D3D11 デバイス
	 * @param width   レンダーターゲット幅
	 * @param height  レンダーターゲット高さ
	 */
	DepthOfField(ID3D11Device* device, uint32_t width, uint32_t height);

	~DepthOfField() = default;

	DepthOfField(const DepthOfField&) = delete;
	DepthOfField& operator=(const DepthOfField&) = delete;
	DepthOfField(DepthOfField&&) noexcept = delete;
	DepthOfField& operator=(DepthOfField&&) noexcept = delete;

	/**
	 * @brief DoF 処理の実行
	 * @param immediate_context  D3D11 デバイスコンテキスト
	 * @param color_map          入力カラーマップ SRV（HDR 推奨）
	 * @param depth_map          深度バッファ SRV
	 *
	 * @note
	 * DoF パラメータ（FNumber, FocalLength 等）は
	 * CAMERA_CONSTANT_BUFFER (b1) から自動参照されるため、
	 * 呼び出し前にカメラ CB が正しくバインドされていること。
	 *
	 * @code
	 * DepthOfField dof(device, width, height);
	 *
	 * // 毎フレーム（カメラ CB バインド後に呼ぶ）
	 * dof.make(context, hdrColorSRV, depthSRV);
	 *
	 * // 結果を次パスへ
	 * context->PSSetShaderResources(0, 1, dof.GetColorMapAddress());
	 * @endcode
	 */
	void make(ID3D11DeviceContext* immediate_context,
		ID3D11ShaderResourceView* color_map);

	/** @brief 最終 DoF 結果の SRV 取得 */
	ID3D11ShaderResourceView* GetColorMap() const
	{
		return dof_final->GetColorMap();
	}

	/** @brief SRV ポインタアドレス取得（PSSetShaderResources 用） */
	ID3D11ShaderResourceView** GetColorMapAddress() const
	{
		return dof_final->GetColorMapAddress();
	}

public:
	// ---- 実行時スイッチ ----

	/** @brief DoF 有効フラグ（0 = オフ、オリジナルをそのまま出力） */
	int is_dof = 1;

	/**
	 * @brief ハーフ解像度処理フラグ
	 * @note  true にすると内部バッファを 1/2 解像度にして高速化。
	 *        最終合成のみフル解像度で実行される。
	 *        コンストラクタ呼び出し前に設定すること。
	 */
	bool half_resolution = true;

private:
	// ---- フルスクリーン描画 ----
	std::unique_ptr<FullscreenQuad> bit_block_transfer;

	// ---- フレームバッファ ----

	/** @brief CoC マップ（フル解像度）: R=NearCoC, G=FarCoC */
	std::unique_ptr<Framebuffer> coc_map;

	/** @brief Near フィールド（ワーク解像度, premultiplied α） */
	std::unique_ptr<Framebuffer> near_field;

	/** @brief Far フィールド（ワーク解像度） */
	std::unique_ptr<Framebuffer> far_field;

	/** @brief FFS 水平積分後の中間バッファ: [0]=near, [1]=far */
	std::unique_ptr<Framebuffer> ffs_horizontal[2];

	/** @brief FFS 垂直積分後（ボケ完成）: [0]=near, [1]=far */
	std::unique_ptr<Framebuffer> ffs_vertical[2];

	/** @brief 最終出力（フル解像度） */
	std::unique_ptr<Framebuffer> dof_final;

	// ---- ピクセルシェーダ ----
	Microsoft::WRL::ComPtr<ID3D11PixelShader> coc_generation_ps;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> field_separation_near_ps;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> field_separation_far_ps;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> ffs_horizontal_ps;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> ffs_vertical_ps;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> composite_ps;

	/**
	 * @brief DoF 固有の軽量定数バッファ（b9）
	 * @details
	 * CAMERA_CONSTANT_BUFFER (b1) で賄えない
	 * is_dof フラグと inv_buffer サイズのみを保持する。
	 * FNumber / FocalLength 等は b1 から直接参照する。
	 */
	struct alignas(16) dof_local_constants
	{
		int   is_dof;
		float inv_buffer_width;   // 1.0f / ワーク解像度幅
		float inv_buffer_height;  // 1.0f / ワーク解像度高さ
		float padding;
	};
	static_assert(sizeof(dof_local_constants) % 16 == 0,
		"dof_local_constants must be 16-byte aligned");

	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer; // slot b9

	// ---- 解像度 ----
	uint32_t full_width;
	uint32_t full_height;
	uint32_t work_width;
	uint32_t work_height;
};
