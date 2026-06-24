// Vignetting.h
#pragma once

#include <memory>
#include <d3d11.h>
#include <wrl.h>
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/shader/shader.h"
#include "Engine/Graphics/UI/DebugMenu/CustomWidgets.h"

/**
 * @brief 周辺減光（Vignetting）ポストエフェクトクラス
 *
 * @details
 * 現実のレンズでは、斜め方向から入射する光がレンズの縁に遮られ、
 * あるいは cos^4 則による光量減衰によって画面四隅が暗くなる。
 *
 * 本実装は以下の 2 モードを切り替えられる：
 *   1. 解析モデル : cos^4(θ) 近似
 *        物理的に正確。FNumber / FocalLength との連動で減光量が変わる。
 *   2. アーティスト カーブ : smooth-step ベースのグラデーション
 *        直感的に調整しやすい。
 *
 * 物理ベース連動：
 *   - FNumber      : 開放絞り（小さい F値）ほど周辺減光が強い
 *   - FocalLength  : 広角（短焦点）ほど周辺減光が強い
 *
 * 処理フロー：
 *   HDR Input → [vignetting_ps] → HDR Output
 *
 * @note
 * ・出力は R16G16B16A16_FLOAT の線形 HDR
 * ・Camera_Constants (b1) を参照して物理値を自動反映
 * ・b9 に VignettingConstants をバインド
 */
class Vignetting
{
public:
    /**
     * @brief コンストラクタ
     * @param device  D3Dデバイス
     * @param width   バッファ幅
     * @param height  バッファ高さ
     */
    Vignetting(ID3D11Device* device, uint32_t width, uint32_t height);
    ~Vignetting() = default;

    Vignetting(const Vignetting&) = delete;
    Vignetting& operator=(const Vignetting&) = delete;
    Vignetting(Vignetting&&) = delete;
    Vignetting& operator=(Vignetting&&) = delete;

    /**
     * @brief 周辺減光処理実行
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
        return vignette_buffer->GetColorMap();
    }

    /** @brief PSSetShaderResources 用アドレス取得 */
    ID3D11ShaderResourceView** GetColorMapAddress() const
    {
        return vignette_buffer->GetColorMapAddress();
    }

    /** @brief ImGui デバッグ UI 描画 */
    void DrawDebugUI();

public:
    bool  is_enabled       = true;    ///< 無効時はパススルー
    bool  physical_link    = true;    ///< FNumber / FocalLength から自動算出するか
    float intensity        = 1.0f;    ///< アーティスト強度スケール [0..3]
    float inner_radius     = 0.35f;   ///< 減光が始まる半径（正規化、0..1）
    float outer_radius     = 0.75f;   ///< 完全減光の半径（正規化、0..1）
    float smoothness       = 2.0f;    ///< smooth-step 指数（大きいほど急峻）

private:
    std::unique_ptr<FullscreenQuad> bit_block_transfer;
    std::unique_ptr<Framebuffer>    vignette_buffer;

    Microsoft::WRL::ComPtr<ID3D11PixelShader> vignette_ps;

    /**
     * @brief 定数バッファ構造体（16byte アライン）
     */
    struct alignas(16) VignettingConstants
    {
        float intensity;        ///< 全体強度 [0..3]
        float inner_radius;     ///< 減光開始半径（正規化 UV 空間）
        float outer_radius;     ///< 最大減光半径（正規化 UV 空間）
        float smoothness;       ///< 指数

        int   is_enabled;       ///< 有効フラグ
        int   physical_link;    ///< 物理連動フラグ（1: FNumber/FocalLength を b1 から参照）
        float pad[2];
    };
    static_assert(sizeof(VignettingConstants) % 16 == 0,
        "VignettingConstants must be 16-byte aligned");

    Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;
};
