// ChromaticAberration.h
#pragma once

#include <memory>
#include <d3d11.h>
#include <wrl.h>
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/shader/shader.h"
#include "Engine/Graphics/UI/DebugMenu/CustomWidgets.h"

/**
 * @brief 色収差（Chromatic Aberration）ポストエフェクトクラス
 *
 * @details
 * 現実のレンズでは、波長によって屈折率が異なるため RGB チャンネルが
 * 画面端に向かって放射状にズレる（横方向色収差）。
 *
 * 物理ベース連動：
 *   - FocalLength  : 広角（短焦点）ほど収差が大きい → strength_from_focal_length()
 *   - FNumber      : 開放絞りほど収差が大きい       → strength_from_fnumber()
 *   - intensity    : アーティスト手動オフセット
 *
 * 処理フロー：
 *   HDR Input → [chromatic_aberration_ps] → HDR Output
 *
 * @note
 * ・出力は R16G16B16A16_FLOAT の線形 HDR
 * ・Camera_Constants (b1) を参照して物理値を自動反映
 * ・b9 に ChromaticAberrationConstants をバインド
 */
class ChromaticAberration
{
public:
    /**
     * @brief コンストラクタ
     * @param device   D3Dデバイス
     * @param width    バッファ幅
     * @param height   バッファ高さ
     */
    ChromaticAberration(ID3D11Device* device, uint32_t width, uint32_t height);
    ~ChromaticAberration() = default;

    ChromaticAberration(const ChromaticAberration&) = delete;
    ChromaticAberration& operator=(const ChromaticAberration&) = delete;
    ChromaticAberration(ChromaticAberration&&) = delete;
    ChromaticAberration& operator=(ChromaticAberration&&) = delete;

    /**
     * @brief 色収差処理実行
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
        return ca_buffer->GetColorMap();
    }

    /** @brief PSSetShaderResources 用アドレス取得 */
    ID3D11ShaderResourceView** GetColorMapAddress() const
    {
        return ca_buffer->GetColorMapAddress();
    }

    /** @brief ImGui デバッグ UI 描画 */
    void DrawDebugUI();

public:
    bool  is_enabled       = true;   ///< 無効時はパススルー
    float intensity        = 1.0f;   ///< アーティスト強度オフセット [0..3]
    bool  physical_link    = true;   ///< FocalLength / FNumber から自動算出するか

private:
    std::unique_ptr<FullscreenQuad> bit_block_transfer;
    std::unique_ptr<Framebuffer>    ca_buffer;

    Microsoft::WRL::ComPtr<ID3D11PixelShader> ca_ps;

    /**
     * @brief 定数バッファ構造体（16byte アライン）
     *
     * shift_r / shift_b : UV 空間でのチャンネルごとのオフセット倍率
     *   正値で外側へ、負値で内側へシフト。
     *   R を外、B を内（または逆）にすることで色フチを表現。
     */
    struct alignas(16) ChromaticAberrationConstants
    {
        float shift_r;      ///< R チャンネル UV オフセット倍率
        float shift_b;      ///< B チャンネル UV オフセット倍率
        int   is_enabled;   ///< 有効フラグ
        float pad;
    };
    static_assert(sizeof(ChromaticAberrationConstants) % 16 == 0,
        "ChromaticAberrationConstants must be 16-byte aligned");

    Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;

    // ---- 物理ベース強度計算 ----

    /**
     * @brief 焦点距離から収差強度を算出
     * @param focal_length_mm 焦点距離 [mm]
     * @return 強度係数 (12mm→2.0, 50mm→0.5, 200mm→0.1 程度)
     */
    static float strength_from_focal_length(float focal_length_mm);

    /**
     * @brief F値から収差強度を算出
     * @param f_number F値 (例: 1.4, 2.8, 5.6)
     * @return 強度係数 (f/1.4→1.5, f/5.6→0.5, f/16→0.2 程度)
     */
    static float strength_from_fnumber(float f_number);
};
