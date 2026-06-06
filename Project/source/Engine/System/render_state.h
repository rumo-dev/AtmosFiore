#pragma once
#include <d3d11.h>
#include <Windows.h> // GetAsyncKeyState用
#include <algorithm>
#include <string>
#include "Engine/utilities/color_util.h"
#include "Engine/utilities/misc.h"
#include "Engine/utilities/dx_common.h"
#include "Engine/System/samplers.h"

/**
 * @brief 深度ステートの種類
 * @details 深度テストと書き込みの有効/無効の組み合わせを定義
 */
enum class Depth_State
{
    Test_Enable_Write_Enable,     ///< 深度テスト有効 & 書き込み有効
    Test_Enable_Write_Disable,    ///< 深度テスト有効 & 書き込み無効
    Test_Disable_Write_Enable,    ///< 深度テスト無効 & 書き込み有効
    Test_Disable_Write_Disable,   ///< 深度テスト無効 & 書き込み無効
    Num,
};

/**
 * @brief ステンシル参照値の意味
 * @details ステンシルテスト時に使用する参照値の用途を明示
 */
enum class Stencil_Ref : UINT
{
    None = 0,    ///< ステンシル未使用またはリセット
    Default = 1, ///< 通常描画用
};

/**
 * @brief ブレンドステートの種類
 */
enum class Blend_State {
    Alpha,              ///< αブレンド
    Add,                ///< 加算合成
    Subtract,           ///< 減算合成
    Replace,            ///< 上書き
    Multiply_Alpha,     ///< α乗算
    Multiply_Color,     ///< 色乗算
    Lighten,            ///< 明るい方を採用
    Darken,             ///< 暗い方を採用
    Screen,             ///< スクリーン合成
    Opaque,             ///< 不透明
    Num,
};

/**
 * @brief ラスタライザステートの種類
 */
enum class Rasterizer_State
{
    Cull_None_CW,
    Cull_None_CCW,
    Cull_Back_CW,
    Cull_Back_CCW,
    Cull_Front_CW,
    Cull_Front_CCW,
    Wireframe_CW,
    Wireframe_CCW,
    Wireframe_No_Depth_CW,
    Wireframe_No_Depth_CCW,
    Num,
};

/**
 * @brief Direct3D11 レンダーステート管理クラス
 *
 * @details
 * 各種レンダリングステート（サンプラ・深度・ブレンド・ラスタライザ）を
 * 一元管理し、描画パイプラインへの設定を簡略化する。
 *
 * @note
 * ・シングルトンで管理することで、状態の一貫性を維持
 * ・状態生成は create_* 系関数で初期化時に一度だけ行う想定
 * ・描画ごとのステート切り替えコストを最小限にする設計
 */
class Render_State
{
public:

    /**
     * @brief インスタンス取得（シングルトン）
     */
    static Render_State& instance()
    {
        static Render_State instance;
        return instance;
    };

    /**
     * @brief コンストラクタ
     */
    Render_State();

    ~Render_State() = default;

    /**
     * @brief サンプラステート設定
     * @param immediate_context デバイスコンテキスト
     */
    void set_sampler_state(ID3D11DeviceContext* immediate_context);

    /**
     * @brief 深度・ステンシルステート設定
     * @param immediate_context デバイスコンテキスト
     * @param state 深度ステート
     * @param stencil_ref ステンシル参照値
     *
     * @warning
     * stencil_ref は UINT に暗黙変換されるため、
     * enum値とシェーダー側の期待値が一致していることを確認すること。
     */
    void set_depth_stencil_state(
        ID3D11DeviceContext* immediate_context,
        Depth_State state,
        Stencil_Ref stencil_ref = Stencil_Ref::Default);

    /**
     * @brief ブレンドステート設定
     * @param immediate_context デバイスコンテキスト
     * @param state ブレンド状態
     * @param blend_factor ブレンド係数
     * @param sample_mask サンプルマスク
     *
     * @warning
     * sample_mask は通常 0xFFFFFFFF を指定するが、
     * マルチサンプル環境では意図しない描画欠けが起こる可能性がある。
     */
    void set_blend_state(
        ID3D11DeviceContext* immediate_context,
        Blend_State state,
        Color_Utils::Color blend_factor = Color_Utils::Colors::transparent,
        UINT sample_mask = 0xFFFFFFFF);

    /**
     * @brief ラスタライザステート設定
     */
    void set_rasterizer_state(
        ID3D11DeviceContext* immediate_context,
        Rasterizer_State state);

    /**
     * @brief 各種レンダーステート一括設定
     *
     * @note
     * 描画パス単位での状態切り替えを簡潔にするためのユーティリティ関数。
     *
     * @code
     * // 例: 通常3D描画
     * Render_State::instance().set_render_states(
     *     context,
     *     Depth_State::Test_Enable_Write_Enable,
     *     Blend_State::Alpha,
     *     Rasterizer_State::Cull_Back_CW
     * );
     * @endcode
     */
    void set_render_states(
        ID3D11DeviceContext* immediate_context,
        Depth_State depth_state = Depth_State::Test_Disable_Write_Disable,
        Blend_State blend_state = Blend_State::Alpha,
        Rasterizer_State rasterizer_state = Rasterizer_State::Cull_Back_CW,
        Color_Utils::Color blend_factor = Color_Utils::Colors::transparent,
        UINT sample_mask = 0xFFFFFFFF,
        Stencil_Ref stencil_ref = Stencil_Ref::Default);

    /**
     * @brief 3D描画用の標準ステート設定
     */
    void set_3d_render_states(
        ID3D11DeviceContext* immediate_context,
        Rasterizer_State rs = Rasterizer_State::Cull_Back_CW);

    /**
     * @brief ディファードレンダリング用ステート
     */
    void set_deferred_render_states(
        ID3D11DeviceContext* immediate_context,
        Rasterizer_State rs = Rasterizer_State::Cull_None_CW);

    /**
     * @brief 2D描画用ステート設定
     *
     * @note
     * UIやスプライト描画を想定（深度無効 + αブレンド）
     */
    void set_2d_render_states(ID3D11DeviceContext* immediate_context);

    /**
     * @brief サンプラステート生成
     */
    bool create_sampler_state(ID3D11Device* device);

    /**
     * @brief 深度ステート生成
     */
    bool create_depth_stencil_state(ID3D11Device* device);

    /**
     * @brief ブレンドステート生成
     */
    bool create_blend_state(ID3D11Device* device);

    /**
     * @brief ラスタライザステート生成
     */
    bool create_rasterizer_state(ID3D11Device* device);

private:

    ///< サンプラステート配列
    Microsoft::WRL::ComPtr<ID3D11SamplerState> _sampler_states[14];

    ///< 深度ステート配列
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>
        _depth_stencil_states[static_cast<int>(Depth_State::Num)];

    ///< ブレンドステート配列
    Microsoft::WRL::ComPtr<ID3D11BlendState>
        _blend_states[static_cast<int>(Blend_State::Num)];

    ///< ラスタライザステート配列
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>
        _rasterizer_states[static_cast<int>(Rasterizer_State::Num)];
};