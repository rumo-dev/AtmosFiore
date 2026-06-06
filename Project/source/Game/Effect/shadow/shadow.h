#pragma once

#include <memory>
#include <d3d11.h>
#include <wrl.h>
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/shader/shader.h"
#include "Engine/utilities/misc.h"

#include "Engine/Graphics/Shadow/shadow_map.h"
#include "Engine/Graphics/UI/imGui/imgui.h"

/**
 * @brief シャドウマッピング管理クラス
 *
 * @details
 * ライト視点から深度マップ（シャドウマップ）を生成し、
 * それを用いてシーンに影を適用する。
 *
 * 処理フロー：
 * 1. make_shadow_begin() でライト視点レンダリング開始
 * 2. シーンをライト視点で描画（深度のみ）
 * 3. make_shadow_end() で通常描画へ復帰
 * 4. make() でシャドウ適用（ポスト処理）
 *
 * @note
 * ・shadow_map クラスに深度バッファ生成を委譲
 * ・FullscreenQuad によるポストエフェクト型シャドウ合成
 * ・ライトパラメータは ImGui でリアルタイム調整可能
 *
 * @warning
 * ・シャドウマップ生成中は通常のレンダーターゲットを使用しないこと
 * ・深度バッファとカラーバッファのバインド状態に注意
 * ・SRVとDSVの同時バインドは禁止（DX11制約）
 */
class shadow
{
public:
    enum class PointShadowFace
    {
        Front,
        Back
    };

    struct Point_Shadow_Constants
    {
        DirectX::XMFLOAT4 position_radius;
        DirectX::XMFLOAT4 params;
        DirectX::XMFLOAT4 options;
    };


    /**
     * @brief コンストラクタ
     * @param device D3Dデバイス
     * @param width 出力バッファ幅
     * @param height 出力バッファ高さ
     */
    shadow(ID3D11Device* device, uint32_t width, uint32_t height);

    ~shadow() = default;

    shadow(const shadow&) = delete;
    shadow& operator =(const shadow&) = delete;
    shadow(shadow&&) noexcept = delete;
    shadow& operator =(shadow&&) noexcept = delete;

    /**
     * @brief シャドウ適用処理
     * @param immediate_context デバイスコンテキスト
     * @param color_map 入力カラー（通常描画結果）
     *
     * @details
     * シャドウマップを参照し、影を合成した最終画像を生成する。
     *
     * @code
     * shadow.make(context, sceneColorSRV);
     * context->PSSetShaderResources(0, 1, shadow.GetColorMapAddress());
     * @endcode
     *
     * @warning
     * color_map はシャドウ生成時のレンダーターゲットと同一であってはならない
     */
    void make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map);

    /**
     * @brief 最終カラー取得
     */
    ID3D11ShaderResourceView* getColorMap() const
    {
        return shaded->GetColorMap();
    }

    /**
     * @brief SRVアドレス取得（API用）
     */
    ID3D11ShaderResourceView** GetColorMapAddress() const
    {
        return shaded->GetColorMapAddress();
    }

    /**
     * @brief シャドウマップ取得
     */
    shadow_map* get_shadow_map() const
    {
        return point_shadow_front.get();
    }

    ID3D11ShaderResourceView* get_point_shadow_front_map() const
    {
        return point_shadow_front->get_depth_map();
    }

    ID3D11ShaderResourceView* get_point_shadow_back_map() const
    {
        return point_shadow_back->get_depth_map();
    }

    ID3D11ShaderResourceView* get_directional_shadow_map() const
    {
        return directional_shadow_map->get_depth_map();
    }

public:

    /**
     * @brief シャドウマップ生成開始
     *
     * @details
     * ライト視点に切り替え、深度書き込み用状態をセットする。
     *
     * @note
     * この後にシーンを描画することでシャドウマップが生成される
     */
    void make_shadow_begin(PointShadowFace face = PointShadowFace::Front);
    void make_directional_shadow_begin();

    /**
     * @brief シャドウマップ生成終了
     *
     * @details
     * 通常のカメラ視点・レンダリング状態に戻す。
     */
    void make_shadow_end();

private:

    ///< フルスクリーン描画
    std::unique_ptr<FullscreenQuad> bit_block_transfer;

    ///< シャドウ適用後の出力
    std::unique_ptr<Framebuffer> shaded;

public: // shadow settings

    /**
     * @brief シャドウマップ解像度
     *
     * @note
     * 高いほど品質向上だがメモリと描画コスト増加
     */
    const uint32_t shadowmap_width = 2048;
    const uint32_t shadowmap_height = 2048;

    ///< ポイントライト用双放物面シャドウマップ（前後）
    std::unique_ptr<shadow_map> point_shadow_front;
    std::unique_ptr<shadow_map> point_shadow_back;
    std::unique_ptr<shadow_map> directional_shadow_map;

    Microsoft::WRL::ComPtr<ID3D11Buffer> point_shadow_constant_buffer;
    shadow_map* active_shadow_map{ nullptr };

    ///< ライト注視点
    DirectX::XMFLOAT4 light_view_focus{ 0, 0, 0, 1 };

    ///< ライト距離
    float light_view_distance{ 35.0f };

    ///< 投影サイズ（直交投影範囲）
    float light_view_size{ 100.0f };

    ///< ニアクリップ
    float light_view_near_z{ 0.1f };

    ///< ファークリップ
    float light_view_far_z{ 100.0f };

    float point_shadow_bias{ 0.003f };
    float point_shadow_strength{ 0.65f };
    int point_shadow_enabled{ 1 };

    // last computed light view-projection (stored for culling)
    DirectX::XMFLOAT4X4 last_light_view_projection{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    // per-face view-projection matrices for point shadow (cube faces)
    DirectX::XMFLOAT4X4 point_face_viewproj[6];
    // current point shadow face group being rendered (Front/Back)
    PointShadowFace current_point_face_group{ PointShadowFace::Front };

    // getters
    DirectX::XMFLOAT4X4 get_point_face_viewproj(int idx) const { return point_face_viewproj[idx]; }
    PointShadowFace get_current_point_face_group() const { return current_point_face_group; }

    /**
     * @brief ImGuiによる調整UI
     *
     * @note
     * ランタイムでシャドウ品質・範囲を調整可能
     *
     * @warning
     * near > far にならないよう制御しているが、
     * 極端な値ではシャドウが破綻する可能性あり
     */
    void shadow_gui()
    {
        ImGui::SliderFloat("light_view_distance", &light_view_distance, 1.0f, +100.0f);
        ImGui::SliderFloat("light_view_size", &light_view_size, 1.0f, +100.0f);
        ImGui::SliderFloat("light_view_near_z", &light_view_near_z, 0.01f, light_view_far_z - 1.0f);
        ImGui::SliderFloat("light_view_far_z", &light_view_far_z, light_view_near_z + 1.0f, +100.0f);
        bool enabled = point_shadow_enabled != 0;
        if (ImGui::Checkbox("point_shadow_enabled", &enabled))
        {
            point_shadow_enabled = enabled ? 1 : 0;
        }
        ImGui::SliderFloat("point_shadow_bias", &point_shadow_bias, 0.0f, 0.05f);
        ImGui::SliderFloat("point_shadow_strength", &point_shadow_strength, 0.0f, 1.0f);
    }

public:

    /// @name ライトパラメータ取得
    /// @{

    DirectX::XMFLOAT4 get_light_view_focus() const { return light_view_focus; }
    float get_light_view_distance() const { return light_view_distance; }
    float get_light_view_size() const { return light_view_size; }
    float get_light_view_near_z() const { return light_view_near_z; }
    float get_light_view_far_z() const { return light_view_far_z; }

    // Get last computed light view-projection (for culling)
    DirectX::XMFLOAT4X4 get_light_view_projection() const { return last_light_view_projection; }

    /// @}

};
