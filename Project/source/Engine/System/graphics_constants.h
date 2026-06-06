#pragma once
#include <cstdint>
#include <dxgi.h>
#include <dxgi1_2.h>

/**
 * @brief グラフィックス関連の定数定義
 *
 * @details
 * Direct3D11 / DXGI における各種設定値を一元管理する名前空間。
 * 主に以下を定義する：
 *
 * - SwapChain / BackBuffer 設定
 * - MSAA 設定
 * - Viewport 設定
 * - フォーマット定義（RTV / DSV）
 *
 * @note
 * - constexpr を使用しコンパイル時定数として扱う
 * - エンジン全体で統一された設定を使うためのもの
 *
 * - 将来的な拡張例：
 *   - HDR フォーマット（R10G10B10A2 / FP16）
 *   - 可変 MSAA 設定
 *   - 可変バックバッファ数（トリプルバッファ）
 *
 * @warning
 * - これらの値は SwapChain / RenderTarget 作成時に使用されるため、
 *   変更すると描画結果やパフォーマンスに大きく影響します
 *
 * - SampleCount > 1 にする場合：
 *   - デバイスが対応している必要があります
 *   - Resolve 処理が必要になる場合があります
 *
 * - SRGB フォーマットを使用する場合：
 *   - シェーダー側でリニア空間を前提にする必要があります
 *
 * @code
 * DXGI_SWAP_CHAIN_DESC desc{};
 * desc.BufferCount = graphics_constants::BackBufferCount;
 * desc.BufferDesc.Format =
 *     static_cast<DXGI_FORMAT>(graphics_constants::BackBufferFormat::DEFAULT);
 *
 * desc.SampleDesc.Count = graphics_constants::SampleCount;
 * desc.SampleDesc.Quality = graphics_constants::SampleQuality;
 * @endcode
 */
namespace graphics_constants {

    // =========================
    // 基本設定
    // =========================

    /// バックバッファ数（通常はダブルバッファ）
    constexpr UINT BackBufferCount = 2;

    /**
     * @brief MSAA サンプル数
     * @note 1 = 無効
     */
    constexpr UINT SampleCount = 1;

    /// MSAA 品質レベル（通常 0）
    constexpr UINT SampleQuality = 0;

    /// Viewport 最小深度
    constexpr float ViewportMinDepth = 0.0f;

    /// Viewport 最大深度
    constexpr float ViewportMaxDepth = 1.0f;

    /// ミップレベル数（通常 1）
    constexpr UINT MipLevels = 1;

    /// テクスチャ配列サイズ
    constexpr UINT ArraySize = 1;

    /// CPU アクセスフラグ
    constexpr UINT CpuAccessFlags = 0;

    /// Misc フラグ
    constexpr UINT MiscFlags = 0;

    // =========================
    // Present フラグ
    // =========================

    /**
     * @brief DXGI Present フラグラッパー
     *
     * @note
     * DXGI_PRESENT_* を型安全に扱うための enum class
     */
    enum class PresentFlags : UINT {
        None = 0,
        Test = DXGI_PRESENT_TEST,                  ///< テスト表示（実際には表示しない）
        DoNotSequence = DXGI_PRESENT_DO_NOT_SEQUENCE,
        Restart = DXGI_PRESENT_RESTART,
        AllowTearing = DXGI_PRESENT_ALLOW_TEARING  ///< ティアリング許可（VSync無効時）
    };

    // =========================
    // フォーマット
    // =========================

    /**
     * @brief バックバッファフォーマット
     *
     * @note
     * - DEFAULT : Windows標準（非SRGB）
     * - SRGB    : ガンマ補正付き
     */
    enum class BackBufferFormat : UINT {
        DEFAULT = DXGI_FORMAT_B8G8R8A8_UNORM,
        SRGB = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
    };

    /**
     * @brief 深度ステンシルフォーマット
     *
     * @note
     * 用途別に選択：
     * - DEFAULT               : 一般用途
     * - HIGH_PRECISION_DEPTH  : 高精度（シャドウや遠距離描画）
     * - LIGHT_WEIGHT          : 軽量（モバイル/軽量描画）
     */
    enum class DepthStencilFormat : UINT {
        DEFAULT = DXGI_FORMAT_D24_UNORM_S8_UINT,
        HIGH_PRECISION_DEPTH_ONLY = DXGI_FORMAT_D32_FLOAT,
        LIGHT_WEIGHT = DXGI_FORMAT_D16_UNORM
    };

}