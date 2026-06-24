// Vignetting.cpp
#include "Vignetting.h"
#include "Engine/system/render_state.h"

// ---------------------------------------------------------------------------
// コンストラクタ
// ---------------------------------------------------------------------------

Vignetting::Vignetting(ID3D11Device* device, uint32_t width, uint32_t height)
{
    bit_block_transfer = std::make_unique<FullscreenQuad>(device);

    vignette_buffer = std::make_unique<Framebuffer>(
        device, width, height,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        false
    );

    create_ps_from_cso(device, "vignetting_ps.cso", vignette_ps.GetAddressOf());

    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth      = sizeof(VignettingConstants);
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

void Vignetting::make(ID3D11DeviceContext* immediate_context,
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
    VignettingConstants data{};
    data.intensity      = intensity;
    data.inner_radius   = inner_radius;
    data.outer_radius   = outer_radius;
    data.smoothness     = smoothness;
    data.is_enabled     = is_enabled    ? 1 : 0;
    data.physical_link  = physical_link ? 1 : 0;
    data.pad[0]         = 0.0f;
    data.pad[1]         = 0.0f;

    immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &data, 0, 0);
    immediate_context->PSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());

    // ---- 周辺減光パス ----
    vignette_buffer->Clear(immediate_context, 0, 0, 0, 1);
    vignette_buffer->Activate(immediate_context);
    bit_block_transfer->Blit(immediate_context, &color_map, 0, 1, vignette_ps.Get());
    vignette_buffer->Deactivate(immediate_context);
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

void Vignetting::DrawDebugUI()
{
    if (ImGui::CollapsingHeader("Vignetting"))
    {
        CustomUI::Checkbox("Enable", &is_enabled);
        if (is_enabled)
        {
            CustomUI::Checkbox("Physical Link (FNumber / FocalLength)", &physical_link);
            CustomUI::SliderFloat("Intensity",     &intensity,    0.0f, 3.0f,  "%.2f");
            if (!physical_link)
            {
                CustomUI::SliderFloat("Inner Radius", &inner_radius, 0.0f, 1.0f, "%.2f");
                CustomUI::SliderFloat("Outer Radius", &outer_radius, 0.0f, 1.5f, "%.2f");
                CustomUI::SliderFloat("Smoothness",   &smoothness,   0.5f, 6.0f, "%.1f");
            }
        }
    }
}
