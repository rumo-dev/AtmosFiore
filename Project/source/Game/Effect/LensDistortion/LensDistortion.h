// LensDistortion.h
#pragma once

#include <memory>
#include <d3d11.h>
#include <wrl.h>
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/shader/shader.h"
#include "Engine/Graphics/UI/DebugMenu/CustomWidgets.h"

/**
 * @brief レンズ歪曲（Lens Distortion / 歪曲収差）ポストエフェクトクラス
 *
 * @details
 * ブラウン–コンラディモデルの k1 項（主歪曲係数）による
 * 樽型（barrel）／糸巻き型（pincushion）歪みを再現する。
 *
 * 物理ベース連動：
 *   - FocalLength (fov) : 広角（短焦点）ほど樽型歪みが大きい
 *     → distortion_k1_from_fov() で FoV から k1 基準値を算出
 *   - distortion_scale   : アーティスト手動オフセット（正→樽型, 負→糸巻き型）
 *
 * 処理フロー：
 *   HDR Input → [lens_distortion_ps] → HDR Output
 *
 * @note
 * ・出力は R16G16B16A16_FLOAT の線形 HDR
 * ・歪みでサンプル範囲外となる画素は黒でクランプ（BORDER BLACK サンプラー）
 * ・Camera_Constants (b1) を参照して物理値を自動反映
 * ・b9 に LensDistortionConstants をバインド
 */
class LensDistortion
{
public:
    /**
     * @brief コンストラクタ
     * @param device  D3Dデバイス
     * @param width   バッファ幅
     * @param height  バッファ高さ
     */
    LensDistortion(ID3D11Device* device, uint32_t width, uint32_t height);
    ~LensDistortion() = default;

    LensDistortion(const LensDistortion&) = delete;
    LensDistortion& operator=(const LensDistortion&) = delete;
    LensDistortion(LensDistortion&&) = delete;
    LensDistortion& operator=(LensDistortion&&) = delete;

    /**
     * @brief レンズ歪曲処理実行
     * @param immediate_context デバイスコンテキスト
     * @param color_map         入力 HDR カラー SRV
     *
     * @note Camera_Constants (b1) が事前にバインドされている前提。
     *       結果は GetColorMap() で取得。
     */
    void make(ID3D11DeviceContext* immediate_context,
              ID3D11ShaderResourceView* color_map);

    /** @brief 処理済み HDR バッファの SRV 取得 */
    ID3D11ShaderResourceView* GetColorMap() const
    {
        return distortion_buffer->GetColorMap();
    }

    /** @brief PSSetShaderResources 用アドレス取得 */
    ID3D11ShaderResourceView** GetColorMapAddress() const
    {
        return distortion_buffer->GetColorMapAddress();
    }

    /** @brief ImGui デバッグ UI 描画 */
    void DrawDebugUI();

public:
    bool  is_enabled       = true;   ///< 無効時はパススルー
    float distortion_scale = 1.0f;   ///< アーティスト強度スケール（正→樽型, 負→糸巻き型）
    bool  physical_link    = true;   ///< FoV（FocalLength）から自動算出するか

private:
    std::unique_ptr<FullscreenQuad> bit_block_transfer;
    std::unique_ptr<Framebuffer>    distortion_buffer;

    Microsoft::WRL::ComPtr<ID3D11PixelShader> distortion_ps;

    /**
     * @brief 定数バッファ構造体（16byte アライン）
     *
     * k1 : ブラウン–コンラディ モデルの主歪曲係数
     *   正値 → 樽型（barrel）歪み   : 広角レンズに典型
     *   負値 → 糸巻き型（pincushion）歪み : 望遠レンズに典型
     */
    struct alignas(16) LensDistortionConstants
    {
        float k1;           ///< 主歪曲係数
        int   is_enabled;   ///< 有効フラグ
        float pad[2];
    };
    static_assert(sizeof(LensDistortionConstants) % 16 == 0,
        "LensDistortionConstants must be 16-byte aligned");

    Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;

    /**
     * @brief FoV [radian] から k1 基準値を算出
     * @param fov_rad 水平画角 [radian]
     * @return k1 係数 (0.0 ～ 0.5 程度)
     */
    static float distortion_k1_from_fov(float fov_rad);
};
