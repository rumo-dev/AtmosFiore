// ChromaticAberration.cpp
#include "ChromaticAberration.h"
#include "Engine/system/render_state.h"
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// 物理ベース強度計算（static）
// ---------------------------------------------------------------------------

// 焦点距離 [mm] → 収差強度
// 広角（12mm）ほど大きく、望遠（200mm）ほど小さくなる
// 基準: 50mm → 1.0 として正規化
float ChromaticAberration::strength_from_focal_length(float focal_length_mm)
{
    // 対数スケールで焦点距離に反比例
    // strength = log(50) / log(focal_length) を [0.1, 2.5] でクランプ
    if (focal_length_mm <= 0.0f) return 1.0f;
    constexpr float reference = 50.0f;
    float s = std::log(reference) / std::log(focal_length_mm);
    return std::clamp(s, 0.1f, 2.5f);
}

// F値 → 収差強度
// 開放（小さい F値）ほど収差が大きい
// 基準: f/2.8 → 1.0 として正規化
float ChromaticAberration::strength_from_fnumber(float f_number)
{
    if (f_number <= 0.0f) return 1.0f;
    constexpr float reference = 2.8f;
    float s = reference / f_number; // f/1.4 → 2.0, f/5.6 → 0.5
    return std::clamp(s, 0.1f, 2.0f);
}

// ---------------------------------------------------------------------------
// コンストラクタ
// ---------------------------------------------------------------------------

ChromaticAberration::ChromaticAberration(ID3D11Device* device,
                                         uint32_t width, uint32_t height)
{
    bit_block_transfer = std::make_unique<FullscreenQuad>(device);

    ca_buffer = std::make_unique<Framebuffer>(
        device, width, height,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        false
    );

    create_ps_from_cso(device, "chromatic_aberration_ps.cso", ca_ps.GetAddressOf());

    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth      = sizeof(ChromaticAberrationConstants);
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

void ChromaticAberration::make(ID3D11DeviceContext* immediate_context,
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

    // ---- 物理ベース強度計算 ----
    // Camera_Constants (b1) から FocalLength / FNumber を GPU が直接参照するため、
    // CPU 側では intensity だけを定数バッファに積む。
    // physical_link が有効な場合はシェーダ内で b1 の値を使って自動スケーリングする。
    // CPU 側で shift 量を事前計算して渡すことで、シェーダを単純に保てる。
    // ここでは CPU 計算方式を採用（Camera_Constants を CPU にミラーしている前提がないため
    // intensity のみ渡し、実スケーリングはシェーダに委ねる）。

    // ---- 定数バッファ更新 ----
    // shift_r と shift_b は UV 倍率。
    // R を外側（+ 方向）、B を内側（- 方向）にシフト → 典型的な色フチ
    // intensity を [0..3] でスケール。physical_link 時はシェーダが FocalLength/FNumber を追加適用。
    ChromaticAberrationConstants data{};
    data.shift_r    =  0.002f * intensity;  // R : 外側
    data.shift_b    = -0.002f * intensity;  // B : 内側
    data.is_enabled = is_enabled ? 1 : 0;
    data.pad        = 0.0f;

    immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &data, 0, 0);
    immediate_context->PSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());

    // ---- 色収差パス ----
    ca_buffer->Clear(immediate_context, 0, 0, 0, 1);
    ca_buffer->Activate(immediate_context);
    bit_block_transfer->Blit(immediate_context, &color_map, 0, 1, ca_ps.Get());
    ca_buffer->Deactivate(immediate_context);
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

void ChromaticAberration::DrawDebugUI()
{
    if (ImGui::CollapsingHeader("Chromatic Aberration"))
    {
        CustomUI::Checkbox("Enable", &is_enabled);
        if (is_enabled)
        {
            CustomUI::Checkbox("Physical Link (FocalLength / FNumber)", &physical_link);
            CustomUI::SliderFloat("Intensity", &intensity, 0.0f, 3.0f, "%.2f");
        }
    }
}
