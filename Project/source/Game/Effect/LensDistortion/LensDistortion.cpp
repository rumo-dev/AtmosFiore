// LensDistortion.cpp
#include "LensDistortion.h"
#include "Engine/system/render_state.h"
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// 物理ベース計算（static）
// ---------------------------------------------------------------------------

// FoV [radian] → k1（主歪曲係数）
// 広角（大きな FoV）ほど正の k1（樽型）が大きくなる。
// 基準: 45° (0.785 rad) → k1 ≈ 0.0 (ほぼ歪みなし)
//        90° (1.571 rad) → k1 ≈ 0.25 (顕著な樽型)
float LensDistortion::distortion_k1_from_fov(float fov_rad)
{
    constexpr float fov_reference = 0.785398f; // 45° [rad]
    // FoV が基準より大きい（広角）ほど k1 を増やす
    float excess = fov_rad - fov_reference;    // 0° 基準からの超過量
    float k1 = excess * 0.20f;                // スケーリング係数（調整可能）
    return std::clamp(k1, -0.5f, 0.5f);
}

// ---------------------------------------------------------------------------
// コンストラクタ
// ---------------------------------------------------------------------------

LensDistortion::LensDistortion(ID3D11Device* device,
                               uint32_t width, uint32_t height)
{
    bit_block_transfer = std::make_unique<FullscreenQuad>(device);

    distortion_buffer = std::make_unique<Framebuffer>(
        device, width, height,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        false
    );

    create_ps_from_cso(device, "lens_distortion_ps.cso", distortion_ps.GetAddressOf());

    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth      = sizeof(LensDistortionConstants);
    buffer_desc.Usage          = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags      = 0;
    buffer_desc.StructureByteStride = 0;
    device->CreateBuffer(&buffer_desc, nullptr, constant_buffer.GetAddressOf());
}

// ---------------------------------------------------------------------------
// make
// ---------------------------------------------------------------------------

void LensDistortion::make(ID3D11DeviceContext* immediate_context,
                           ID3D11ShaderResourceView* color_map)
{
    // ---- ステート退避 ----
    ID3D11ShaderResourceView* null_srv{};
    ID3D11ShaderResourceView* cached_srvs[1]{};
    immediate_context->PSGetShaderResources(0, 1, cached_srvs);

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> cached_dss;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   cached_rs;
    Microsoft::WRL::ComPtr<ID3D11BlendState>        cached_bs;
    FLOAT blend_factor[4]; UINT sample_mask;
    immediate_context->OMGetDepthStencilState(cached_dss.GetAddressOf(), 0);
    immediate_context->RSGetState(cached_rs.GetAddressOf());
    immediate_context->OMGetBlendState(cached_bs.GetAddressOf(), blend_factor, &sample_mask);

    Microsoft::WRL::ComPtr<ID3D11Buffer> cached_cb;
    immediate_context->PSGetConstantBuffers(9, 1, cached_cb.GetAddressOf());

    // ---- ステート設定 ----
    Render_State::instance().set_2d_render_states(immediate_context);

    // ---- 定数バッファ更新 ----
    // physical_link が有効な場合: シェーダが b1 の fov を参照して k1 を自動スケール。
    // CPU 側では k1 の基準値（アーティストオフセット込み）だけ渡す。
    // 実際の物理スケーリングは HLSL の distortion_k1_from_fov() で行う。
    LensDistortionConstants data{};
    // physical_link 無効時は distortion_scale のみ k1 として使う
    // 有効時は 0 を渡してシェーダ側で fov から計算させる
    data.k1         = physical_link ? 0.0f : (0.1f * distortion_scale);
    data.is_enabled = is_enabled ? 1 : 0;
    data.pad[0]     = 0.0f;
    data.pad[1]     = 0.0f;

    immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &data, 0, 0);
    immediate_context->PSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());

    // ---- 歪曲パス ----
    distortion_buffer->Clear(immediate_context, 0, 0, 0, 1);
    distortion_buffer->Activate(immediate_context);
    bit_block_transfer->Blit(immediate_context, &color_map, 0, 1, distortion_ps.Get());
    distortion_buffer->Deactivate(immediate_context);
    immediate_context->PSSetShaderResources(0, 1, &null_srv);

    // ---- ステート復元 ----
    immediate_context->PSSetConstantBuffers(9, 1, cached_cb.GetAddressOf());
    immediate_context->OMSetDepthStencilState(cached_dss.Get(), 0);
    immediate_context->RSSetState(cached_rs.Get());
    immediate_context->OMSetBlendState(cached_bs.Get(), blend_factor, sample_mask);
    immediate_context->PSSetShaderResources(0, 1, cached_srvs);
    if (cached_srvs[0]) cached_srvs[0]->Release();
}

// ---------------------------------------------------------------------------
// DrawDebugUI
// ---------------------------------------------------------------------------

void LensDistortion::DrawDebugUI()
{
    if (ImGui::CollapsingHeader("Lens Distortion"))
    {
        CustomUI::Checkbox("Enable", &is_enabled);
        if (is_enabled)
        {
            CustomUI::Checkbox("Physical Link (FoV / FocalLength)", &physical_link);
            if (!physical_link)
            {
                // 正 → 樽型（barrel），負 → 糸巻き型（pincushion）
                CustomUI::SliderFloat("Distortion Scale", &distortion_scale, -3.0f, 3.0f, "%.2f");
            }
        }
    }
}
