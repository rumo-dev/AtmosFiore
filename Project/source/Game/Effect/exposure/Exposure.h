// Exposure.h
#pragma once

#include <memory>
#include <d3d11.h>
#include <wrl.h>
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/shader/shader.h"
#include "Engine/Graphics/UI/DebugMenu/CustomWidgets.h"

/**
 * @brief T値ベース露出ポストエフェクトクラス
 *
 * @details
 * T値 / シャッタースピード / ISO から EV100 を計算し、
 * HDRバッファに線形露出スケールを適用する単独ポストエフェクト。
 * トーンマップ・sRGB変換は行わない（後段に委譲）。
 *
 * 処理フロー：
 *   HDR Input → [exposure_ps] → HDR Output (露出適用済み)
 *
 * @note
 * ・出力は依然 R16G16B16A16_FLOAT の線形HDR
 * ・Bloom の前段・後段どちらにも挿入可能
 * ・b1 の Camera_Constants を参照するため追加の定数バッファ不要
 */
class Exposure
{
public:
	/**
	 * @brief コンストラクタ
	 * @param device   D3Dデバイス
	 * @param width    バッファ幅
	 * @param height   バッファ高さ
	 */
	Exposure(ID3D11Device* device, uint32_t width, uint32_t height);
	~Exposure() = default;

	Exposure(const Exposure&) = delete;
	Exposure& operator=(const Exposure&) = delete;
	Exposure(Exposure&&) = delete;
	Exposure& operator=(Exposure&&) = delete;

	/**
	 * @brief 露出処理実行
	 * @param immediate_context デバイスコンテキスト
	 * @param color_map         入力 HDR カラー SRV
	 *
	 * @note
	 * Camera_Constants (b1) が事前にバインドされている前提。
	 * 結果は GetColorMap() / GetColorMapAddress() で取得。
	 */
	void make(ID3D11DeviceContext* immediate_context,
		ID3D11ShaderResourceView* color_map);

	/** @brief 露出適用済み HDR バッファの SRV 取得 */
	ID3D11ShaderResourceView* GetColorMap() const
	{
		return exposure_buffer->GetColorMap();
	}

	/** @brief PSSetShaderResources 用アドレス取得 */
	ID3D11ShaderResourceView** GetColorMapAddress() const
	{
		return exposure_buffer->GetColorMapAddress();
	}

	/** @brief ImGui デバッグ UI 描画 */
	void DrawDebugUI();

public:


	bool  is_enabled = true;          ///< 無効時はパススルー

private:
	std::unique_ptr<FullscreenQuad> bit_block_transfer;
	std::unique_ptr<Framebuffer>    exposure_buffer;

	Microsoft::WRL::ComPtr<ID3D11PixelShader> exposure_ps;

	/** @brief 定数バッファ構造体（16byte アライン） */
	struct alignas(16) ExposureConstants
	{
		int   is_enabled;       // 有効フラグ
		float pad[3];
	};
	static_assert(sizeof(ExposureConstants) % 16 == 0,
		"ExposureConstants must be 16-byte aligned");

	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
};