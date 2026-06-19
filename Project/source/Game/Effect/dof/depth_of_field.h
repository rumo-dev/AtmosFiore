// DEPTH OF FIELD - Fast Filter Spreading (True Scatter / Compute Shader)
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
 * Fast Filter Spreading（散乱方式）を Compute Shader で忠実に実装。
 *
 * ─── 処理フロー ───────────────────────────────────────────────
 *  color_map + depth_map (GBuffer)
 *      │
 *  [Pass 1] coc_generation_ps        → coc_map  (R=NearCoC G=FarCoC)
 *      │
 *  [Pass 2] field_separation_*_ps    → near_field (premult α), far_field
 *      │
 *  [Pass 3a] ffs_scatter_cs          → delta_buffer[near], delta_buffer[far]
 *             各ピクセルが CoC 矩形四隅に Bartlett デルタを InterlockedAdd
 *      │
 *  [Pass 3b] ffs_prefix_h_cs         → delta_buffer （水平プレフィックスサム）
 *      │
 *  [Pass 3c] ffs_prefix_v_cs         → delta_buffer （垂直プレフィックスサム）
 *      │
 *  [Pass 3d] ffs_resolve_cs          → blurred_near, blurred_far (float16 tex)
 *             固定小数点 → float 変換 + 正規化
 *      │
 *  [Pass 4] composite_ps             → dof_final
 *
 * ─── パラメータ ─────────────────────────────────────────────
 *  CAMERA_CONSTANT_BUFFER (b1) から参照:
 *    FNumber, FocalLength, SensorSize, FocusDist, MaxBlurRadius
 *    inv_projection（深度の線形化）
 *
 * ─── Compute Shader の利点 ───────────────────────────────────
 *  ・真の Scatter 実装 → 各ピクセルが自身の CoC を周囲に「撒く」
 *  ・O(1) in filter size（巨大ボケでもコスト一定）
 *  ・PS 版 Gather 近似より正確な Bartlett カーネル
 *
 * @note
 *  ・delta_buffer のサイズ:
 *    (work_width + 2*MAX_R+2) * (work_height + 2*MAX_R+2) * 4 * sizeof(int)
 *    MaxBlurRadius を変更した場合はバッファを再生成すること
 *
 * @warning
 *  ・color_map / depth_map は nullptr 禁止
 *  ・入力 SRV と UAV の同時バインド禁止（DX11 制約）
 *  ・Compute Shader は D3D_FEATURE_LEVEL_11_0 以上が必要
 */
class DepthOfField
{
public:
	/**
	 * @brief コンストラクタ
	 * @param device  D3D11 デバイス（Feature Level 11.0+ 必須）
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
	 *
	 * @note
	 * DoF パラメータ（FNumber, FocalLength 等）は
	 * CAMERA_CONSTANT_BUFFER (b1) から自動参照されるため、
	 * 呼び出し前にカメラ CB が正しくバインドされていること。
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
	int  is_dof = 1;

	/**
	 * @brief ハーフ解像度処理フラグ
	 * @note  true にすると内部バッファを 1/2 解像度にして高速化。
	 *        最終合成（composite_ps）のみフル解像度で実行される。
	 *        コンストラクタ呼び出し前に設定すること。
	 */
	bool half_resolution = true;

private:
	// ---- ヘルパー ----
	void CreateDeltaBuffer(ID3D11Device* device);

	// ---- フルスクリーン描画 ----
	std::unique_ptr<FullscreenQuad> bit_block_transfer;

	// ---- フレームバッファ ----
	std::unique_ptr<Framebuffer> coc_map;      ///< R=NearCoC, G=FarCoC（フル解像度）
	std::unique_ptr<Framebuffer> near_field;   ///< premultiplied α（ワーク解像度）
	std::unique_ptr<Framebuffer> far_field;    ///< A=FarCoC（ワーク解像度）

	/**
	 * @brief FFS 最終ブラー結果テクスチャ [0]=near, [1]=far
	 *
	 * RWTexture2D として ffs_resolve_cs が書き込み、
	 * composite_ps が SRV として読み込む。
	 * D3D11 では RTV/SRV 両用テクスチャを手動で管理する必要があるため、
	 * Framebuffer ではなく生リソースで保持する。
	 */
	struct  alignas(16) BlurredBuffer
	{
		Microsoft::WRL::ComPtr<ID3D11Texture2D>          texture;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
	};
	BlurredBuffer blurred[2]; ///< [0]=near, [1]=far

	/** @brief 最終出力（フル解像度） */
	std::unique_ptr<Framebuffer> dof_final;

	// ---- デルタバッファ（固定小数点 Scatter 用） [0]=near, [1]=far ----
	struct DeltaBuffer
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer>             buffer;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
		uint32_t element_count = 0; ///< 要素数（RGBA × ピクセル数）
	};
	DeltaBuffer delta_buf[2];

	// ---- ピクセルシェーダ ----
	Microsoft::WRL::ComPtr<ID3D11PixelShader> coc_generation_ps;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> field_separation_near_ps;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> field_separation_far_ps;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> composite_ps;

	// ---- コンピュートシェーダ ----
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> ffs_scatter_cs;     ///< Pass 3a
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> ffs_prefix_h_cs;    ///< Pass 3b
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> ffs_prefix_v_cs;    ///< Pass 3c
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> ffs_resolve_cs;     ///< Pass 3d

	// ---- 定数バッファ ----

	/**
	 * @brief DoF 固有の軽量定数バッファ（b9）
	 * @details is_dof フラグと inv_buffer サイズのみ保持。
	 *          FNumber / FocalLength 等は b1 から直接参照。
	 */
	struct alignas(16) dof_local_constants
	{
		int   is_dof;
		float inv_buffer_width;
		float inv_buffer_height;
		float padding;
	};
	static_assert(sizeof(dof_local_constants) % 16 == 0,
		"dof_local_constants must be 16-byte aligned");

	/**
	 * @brief FFS パス専用定数バッファ（b10）
	 * @details デルタバッファのサイズ・パディング情報を格納。
	 */
	struct alignas(16) ffs_constants
	{
		int   buf_width;         ///< パディング込みバッファ幅
		int   buf_height;        ///< パディング込みバッファ高さ
		int   padding_x;         ///< MAX_RADIUS + 1
		int   padding_y;         ///< MAX_RADIUS + 1
		int   work_width_cb;     ///< ワーク解像度幅
		int   work_height_cb;    ///< ワーク解像度高さ
		float ffs_pad0;
		float ffs_pad1;
	};
	static_assert(sizeof(ffs_constants) % 16 == 0,
		"ffs_constants must be 16-byte aligned");

	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;     ///< b9
	Microsoft::WRL::ComPtr<ID3D11Buffer> ffs_constant_buffer; ///< b10

	// ---- 解像度 ----
	uint32_t full_width;
	uint32_t full_height;
	uint32_t work_width;
	uint32_t work_height;

	// ---- FFS バッファサイズキャッシュ ----
	int ffs_buf_width = 0;
	int ffs_buf_height = 0;
	int ffs_padding_x = 0;
	int ffs_padding_y = 0;
};