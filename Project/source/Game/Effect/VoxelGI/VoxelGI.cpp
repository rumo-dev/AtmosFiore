#include "VoxelGI.h"

#include "Engine/Graphics/Model/gltf_model.h"
#include "Engine/System/Manager/resource_manager.h"
#include "Engine/System/render_state.h"
#include "Engine/System/graphics_core.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"

#include <algorithm>
#include <cmath>

using Microsoft::WRL::ComPtr;

bool VoxelGI::initialize(ID3D11Device* device, Gltf_Model* model,
    const DirectX::XMFLOAT4X4& world)
{
    if (!device || !model || !model->has_bbox) return false;
    baked_ = false;
    bake_settings_dirty_ = false;
    slice_dirty_ = true;
    device_ = device;

    DirectX::XMFLOAT3 lower, upper;
    model->compute_instance_aabb(world, false, 0.0f,
        Gltf_Model::animation_mode::single, 0, lower, upper);
    const float x = upper.x - lower.x;
    const float y = upper.y - lower.y;
    const float z = upper.z - lower.z;
    const float extent = (std::max)({ x, y, z, 1.0f }) * 1.05f;
    const float half = extent * 0.5f;
    const float cx = (lower.x + upper.x) * 0.5f;
    const float cy = (lower.y + upper.y) * 0.5f;
    const float cz = (lower.z + upper.z) * 0.5f;
    constants_.volume_min = { cx - half, cy - half, cz - half, 0 };
    constants_.inv_extent = { 1 / extent, 1 / extent, 1 / extent, extent / Resolution };

    D3D11_TEXTURE3D_DESC desc{};
    desc.Width = desc.Height = desc.Depth = Resolution;
    desc.MipLevels = 0;
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE |
        D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    if (FAILED(device->CreateTexture3D(&desc, nullptr, volume_.GetAddressOf()))) return false;
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format = desc.Format;
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
    uav_desc.Texture3D.MipSlice = 0;
    uav_desc.Texture3D.WSize = Resolution;
    if (FAILED(device->CreateUnorderedAccessView(volume_.Get(), &uav_desc,
        volume_uav_.GetAddressOf()))) return false;
    if (FAILED(device->CreateShaderResourceView(volume_.Get(), nullptr,
        volume_srv_.GetAddressOf()))) return false;

    D3D11_BUFFER_DESC cb_desc{};
    cb_desc.ByteWidth = sizeof(Constants);
    cb_desc.Usage = D3D11_USAGE_DEFAULT;
    cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(device->CreateBuffer(&cb_desc, nullptr, constants_buffer_.GetAddressOf()))) return false;

    D3D11_TEXTURE2D_DESC slice_desc{};
    slice_desc.Width = slice_desc.Height = Resolution;
    slice_desc.MipLevels = slice_desc.ArraySize = 1;
    slice_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    slice_desc.SampleDesc.Count = 1;
    slice_desc.Usage = D3D11_USAGE_DEFAULT;
    slice_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device->CreateTexture2D(&slice_desc, nullptr, slice_texture_.GetAddressOf()))) return false;
    if (FAILED(device->CreateUnorderedAccessView(slice_texture_.Get(), nullptr,
        slice_uav_.GetAddressOf()))) return false;
    if (FAILED(device->CreateShaderResourceView(slice_texture_.Get(), nullptr,
        slice_srv_.GetAddressOf()))) return false;

    return true;
}

void VoxelGI::render(ID3D11DeviceContext* ctx, Gltf_Model* model,
    const DirectX::XMFLOAT4X4& world)
{
    if (!volume_uav_ || !constants_buffer_) return;
    auto& resources = Resource_Manager::instance();
    ctx->UpdateSubresource(constants_buffer_.Get(), 0, nullptr, &constants_, 0, 0);
    ID3D11Buffer* cb = constants_buffer_.Get();
    ID3D11ShaderResourceView* null_srv = nullptr;

    if (!baked_ && model)
    {
    auto* vs = resources.get_native_shader<Vertex_Shader>("VOXEL_GI_VS");
    auto* gs = resources.get_native_shader<Geometry_Shader>("VOXEL_GI_GS");
    auto* ps = resources.get_native_shader<Pixel_Shader>("VOXEL_GI_PS");
    if (!vs || !gs || !ps) return;

    ctx->GSSetConstantBuffers(8, 1, &cb);
    ctx->PSSetConstantBuffers(8, 1, &cb);

    ComPtr<ID3D11RenderTargetView> old_rtv;
    ComPtr<ID3D11DepthStencilView> old_dsv;
    ctx->OMGetRenderTargets(1, old_rtv.GetAddressOf(), old_dsv.GetAddressOf());
    UINT viewport_count = 1;
    D3D11_VIEWPORT old_viewport{};
    ctx->RSGetViewports(&viewport_count, &old_viewport);

    // The previous lighting pass may still have sampled the volume.
    ctx->PSSetShaderResources(13, 1, &null_srv);
    const float empty[4] = { 0, 0, 0, 0 };
    ctx->ClearUnorderedAccessViewFloat(volume_uav_.Get(), empty);
    ID3D11UnorderedAccessView* uav = volume_uav_.Get();
    ctx->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 1, &uav, nullptr);
    D3D11_VIEWPORT viewport{ 0, 0, float(Resolution), float(Resolution), 0, 1 };
    ctx->RSSetViewports(1, &viewport);
    Render_State::instance().set_rasterizer_state(ctx, Rasterizer_State::Cull_None_CCW);
    Render_State::instance().set_depth_stencil_state(ctx, Depth_State::Test_Disable_Write_Disable);
    Render_State::instance().set_blend_state(ctx, Blend_State::Opaque);
    ctx->GSSetShader(gs, nullptr, 0);

    model->render(ctx, world, pass_mode::voxel_gi, false, 0.0f,
        Gltf_Model::animation_mode::single, 0);

    ctx->GSSetShader(nullptr, nullptr, 0);
    ID3D11UnorderedAccessView* null_volume_uav = nullptr;
    ctx->OMSetRenderTargetsAndUnorderedAccessViews(
        0, nullptr, nullptr, 0, 1, &null_volume_uav, nullptr);
    ctx->OMSetRenderTargets(1, old_rtv.GetAddressOf(), old_dsv.Get());
    if (viewport_count) ctx->RSSetViewports(1, &old_viewport);
    ctx->GenerateMips(volume_srv_.Get());
    baked_ = true;
    bake_settings_dirty_ = false;
    slice_dirty_ = true;
    }

    if (show_slice_ && baked_ && slice_dirty_)
    {
        auto* cs = resources.get_native_shader<Compute_Shader>("VOXEL_GI_SLICE_CS");
        if (cs)
        {
            ID3D11ShaderResourceView* srv = volume_srv_.Get();
            ID3D11UnorderedAccessView* slice_uav = slice_uav_.Get();
            ctx->CSSetShader(cs, nullptr, 0);
            ctx->CSSetConstantBuffers(0, 1, &cb);
            ctx->CSSetShaderResources(0, 1, &srv);
            ctx->CSSetUnorderedAccessViews(0, 1, &slice_uav, nullptr);
            ctx->Dispatch(Resolution / 8, Resolution / 8, 1);
            ID3D11UnorderedAccessView* null_uav = nullptr;
            ctx->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
            ctx->CSSetShaderResources(0, 1, &null_srv);
            ctx->CSSetShader(nullptr, nullptr, 0);
            slice_dirty_ = false;
        }
    }
}

void VoxelGI::bind_lighting(ID3D11DeviceContext* ctx) const
{
    ID3D11ShaderResourceView* srv = enabled_ && baked_ ? volume_srv_.Get() : nullptr;
    ctx->PSSetShaderResources(13, 1, &srv);
    ID3D11Buffer* cb = constants_buffer_.Get();
    ctx->PSSetConstantBuffers(8, 1, &cb);
}

void VoxelGI::render_debug_3d(ID3D11DeviceContext* ctx) const
{
    if (!show_3d_ || !baked_ || !volume_srv_) return;
    auto* shader = Resource_Manager::instance().get_native_shader<Pixel_Shader>(
        "VOXEL_GI_DEBUG_PS");
    if (!shader) return;

    auto& graphics = Graphics_Core::instance();
    ID3D11RenderTargetView* back_buffer = graphics.get_render_target_view().Get();
    ctx->OMSetRenderTargets(1, &back_buffer, nullptr);
    D3D11_VIEWPORT viewport{};
    graphics.get_viewport(viewport);
    ctx->RSSetViewports(1, &viewport);
    // Use the same rasterizer state as the working fullscreen post-process passes.
    Render_State::instance().set_2d_render_states(ctx);
    Render_State::instance().set_blend_state(ctx, Blend_State::Opaque);

    ID3D11Buffer* camera_cb = graphics.get_constant_buffer(
        Graphics_Core::Conastant_Buffer_Type::Camera);
    ID3D11Buffer* voxel_cb = constants_buffer_.Get();
    ctx->PSSetConstantBuffers(1, 1, &camera_cb);
    ctx->PSSetConstantBuffers(8, 1, &voxel_cb);
    ID3D11ShaderResourceView* srv = volume_srv_.Get();
    graphics.get_fullscreen_quad()->Blit(ctx, &srv, 13, 1, shader);
    ID3D11ShaderResourceView* null_srv = nullptr;
    ctx->PSSetShaderResources(13, 1, &null_srv);
}

void VoxelGI::draw_gui()
{
    if (!volume_srv_) return;
    if (!ImGui::CollapsingHeader("Voxel GI")) return;
    ImGui::Checkbox("Enabled##VoxelGI", &enabled_);
    ImGui::SliderFloat("Strength##VoxelGI", &constants_.strength, 0.0f, 2.0f);
    bake_settings_dirty_ |= ImGui::SliderFloat("Emissive multiplier##VoxelGI",
        &constants_.emissive_multiplier, 0.0f, 20.0f);
    bake_settings_dirty_ |= ImGui::SliderFloat("Environment fill##VoxelGI",
        &constants_.environment_fill, 0.0f, 1.0f);
    if (ImGui::Button("Rebake static volume##VoxelGI")) baked_ = false;
    ImGui::TextDisabled(!baked_ ? "Static volume pending" :
        (bake_settings_dirty_ ? "Rebake to apply settings" : "Static volume baked"));
    if (ImGui::Checkbox("Show volume slice", &show_slice_)) slice_dirty_ = true;
    ImGui::Checkbox("Show 3D volume", &show_3d_);
    if (show_3d_)
        ImGui::TextDisabled("Shown on the main screen; follows the active camera");
    int selected_slice = static_cast<int>(constants_.slice);
    if (ImGui::SliderInt("Y slice", &selected_slice, 0, Resolution - 1))
    {
        constants_.slice = static_cast<UINT>(selected_slice);
        slice_dirty_ = true;
    }
    if (show_slice_)
        ImGui::Image(reinterpret_cast<ImTextureID>(slice_srv_.Get()), ImVec2(256, 256));
}
