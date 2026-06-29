#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include <cstdint>
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/shader/shader.h"

/**
 * @brief フォグエフェクト基底クラス
 *
 * 全フォグ実装はこのクラスを継承する。
 * - make()     : フォグ効果を適用して内部バッファに書き込む
 * - get_color_map() : 結果 SRV を返す（次パスへの接続用）
 * - DrawDebugUI()   : ImGui パネルを描画（派生クラスでオーバーライド）
 */
class FogBase
{
public:
    FogBase() = default;
    virtual ~FogBase() = default;

    // 非コピー
    FogBase(const FogBase&) = delete;
    FogBase& operator=(const FogBase&) = delete;

    virtual void make(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src_srv) = 0;

    virtual ID3D11ShaderResourceView*  get_color_map()         const = 0;
    virtual ID3D11ShaderResourceView** get_color_map_address()  const = 0;

    virtual void DrawDebugUI() {}

    bool is_enabled = true;
};
