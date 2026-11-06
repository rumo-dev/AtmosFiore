#include "DDGI.h"
#include "Game/Effect/VoxelGI/VoxelGI.h"
#include "Engine/System/Manager/resource_manager.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"
#include "Engine/System/graphics_core.h"
#include "Engine/System/render_state.h"
#include "Engine/Utilities/math_util.h"
#include <algorithm>

void DDGI::set_probe_bounds(DirectX::XMFLOAT3 minimum, DirectX::XMFLOAT3 maximum)
{
    minimum.x = (std::min)(minimum.x, maximum.x - 0.1f);
    minimum.y = (std::min)(minimum.y, maximum.y - 0.1f);
    minimum.z = (std::min)(minimum.z, maximum.z - 0.1f);
    constants_.volume_min = { minimum.x, minimum.y, minimum.z, 0.0f };
    constants_.inv_extent = {
        1.0f / (maximum.x - minimum.x),
        1.0f / (maximum.y - minimum.y),
        1.0f / (maximum.z - minimum.z), 0.0f };
    requested_min_[0] = minimum.x; requested_min_[1] = minimum.y; requested_min_[2] = minimum.z;
    requested_max_[0] = maximum.x; requested_max_[1] = maximum.y; requested_max_[2] = maximum.z;
    custom_bounds_ = true;
    frame_ = 0;
    has_data_ = false;
}

bool DDGI::initialize(ID3D11Device* device)
{
    if (!device) return false;
    device_ = device;
    debug_sphere_ = std::make_unique<Debug_Sphere>(device, 8, 6);
    if (!create_probe_buffers()) return false;
    D3D11_BUFFER_DESC cb_desc{};
    cb_desc.ByteWidth = sizeof(Constants);
    cb_desc.Usage = D3D11_USAGE_DEFAULT;
    cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    return SUCCEEDED(device->CreateBuffer(&cb_desc, nullptr,
        constants_buffer_.GetAddressOf()));
}

bool DDGI::create_probe_buffers()
{
    grid_valid_ = false;
    const unsigned int probe_count = constants_.count_x * constants_.count_y * constants_.count_z;
    constexpr unsigned int probe_stride = 12 * sizeof(DirectX::XMFLOAT4);
    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = probe_count * probe_stride;
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    buffer_desc.StructureByteStride = probe_stride;
    for (int i = 0; i < 2; ++i)
    {
        probes_[i].Reset();
        probe_srv_[i].Reset();
        probe_uav_[i].Reset();
        if (FAILED(device_->CreateBuffer(&buffer_desc, nullptr, probes_[i].GetAddressOf()))) return false;
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Format = DXGI_FORMAT_UNKNOWN;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srv_desc.Buffer.NumElements = probe_count;
        if (FAILED(device_->CreateShaderResourceView(probes_[i].Get(), &srv_desc,
            probe_srv_[i].GetAddressOf()))) return false;
        D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
        uav_desc.Format = DXGI_FORMAT_UNKNOWN;
        uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uav_desc.Buffer.NumElements = probe_count;
        if (FAILED(device_->CreateUnorderedAccessView(probes_[i].Get(), &uav_desc,
            probe_uav_[i].GetAddressOf()))) return false;
    }
    filtered_probes_.Reset();
    filtered_probe_srv_.Reset();
    filtered_probe_uav_.Reset();
    if (FAILED(device_->CreateBuffer(&buffer_desc, nullptr,
        filtered_probes_.GetAddressOf()))) return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC filtered_srv_desc{};
    filtered_srv_desc.Format = DXGI_FORMAT_UNKNOWN;
    filtered_srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    filtered_srv_desc.Buffer.NumElements = probe_count;
    if (FAILED(device_->CreateShaderResourceView(filtered_probes_.Get(),
        &filtered_srv_desc, filtered_probe_srv_.GetAddressOf()))) return false;
    D3D11_UNORDERED_ACCESS_VIEW_DESC filtered_uav_desc{};
    filtered_uav_desc.Format = DXGI_FORMAT_UNKNOWN;
    filtered_uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    filtered_uav_desc.Buffer.NumElements = probe_count;
    if (FAILED(device_->CreateUnorderedAccessView(filtered_probes_.Get(),
        &filtered_uav_desc, filtered_probe_uav_.GetAddressOf()))) return false;
    frame_ = 0;
    source_revision_ = 0;
    current_ = 0;
    has_data_ = false;
    grid_valid_ = true;
    return true;
}

void DDGI::update(ID3D11DeviceContext* ctx, const VoxelGI& source)
{
    if (!enabled_ || !grid_valid_ || !constants_buffer_ ||
        !source.volume_srv() || !source.albedo_srv() || !source.normal_srv()) return;
    auto* shader = Resource_Manager::instance().get_native_shader<Compute_Shader>("DDGI_UPDATE_CS");
    auto* filter_shader = Resource_Manager::instance().get_native_shader<Compute_Shader>("DDGI_FILTER_CS");
    if (!shader || !filter_shader) return;
    const bool reset = source_revision_ != source.bake_revision();
    if (reset)
    {
        source_revision_ = source.bake_revision();
        frame_ = 0;
    }
    constants_.voxel_min = source.volume_min();
    constants_.voxel_inv_extent = source.inverse_extent();
    constants_.voxel_resolution = source.resolution();
    if (!custom_bounds_)
    {
        constants_.volume_min = constants_.voxel_min;
        constants_.inv_extent = constants_.voxel_inv_extent;
        requested_min_[0] = constants_.volume_min.x;
        requested_min_[1] = constants_.volume_min.y;
        requested_min_[2] = constants_.volume_min.z;
        requested_max_[0] = constants_.volume_min.x + 1.0f / constants_.inv_extent.x;
        requested_max_[1] = constants_.volume_min.y + 1.0f / constants_.inv_extent.y;
        requested_max_[2] = constants_.volume_min.z + 1.0f / constants_.inv_extent.z;
    }
    const float sx = 1.0f / (constants_.inv_extent.x * constants_.count_x);
    const float sy = 1.0f / (constants_.inv_extent.y * constants_.count_y);
    const float sz = 1.0f / (constants_.inv_extent.z * constants_.count_z);
    constants_.inv_extent.w = (std::max)({ sx, sy, sz });
    constants_.frame = frame_++;
    constants_.inject_lights = inject_lights_ ? 1u : 0u;
    ctx->UpdateSubresource(constants_buffer_.Get(), 0, nullptr, &constants_, 0, 0);
    ID3D11Buffer* cb = constants_buffer_.Get();
    auto& graphics = Graphics_Core::instance();
    if (inject_lights_)
    {
        graphics.get_point_light_manager().upload_to_gpu(ctx, false);
        graphics.get_spot_light_manager().upload_to_gpu(ctx, false);
        graphics.get_area_light_manager().upload_to_gpu(ctx, false);
    }
    ID3D11ShaderResourceView* lights[3] = {
        graphics.get_point_light_manager().get_srv(),
        graphics.get_spot_light_manager().get_srv(),
        graphics.get_area_light_manager().get_srv()
    };
    ID3D11ShaderResourceView* directional_shadow =
        graphics.post_procss.GetShadow().get_directional_shadow_map();
    ID3D11Buffer* light_cb = graphics.get_constant_buffer(
        Graphics_Core::Conastant_Buffer_Type::Light);
    ID3D11Buffer* point_cb = graphics.get_point_light_manager().get_cb();
    ID3D11Buffer* spot_cb = graphics.get_spot_light_manager().get_cb();
    ID3D11Buffer* area_cb = graphics.get_area_light_manager().get_cb();
    ID3D11ShaderResourceView* inputs[4] = {
        source.volume_srv(), probe_srv_[current_].Get(),
        source.albedo_srv(), source.normal_srv()
    };
    const int next = 1 - current_;
    ID3D11UnorderedAccessView* output = probe_uav_[next].Get();
    Render_State::instance().set_cs_sampler_state(ctx);
    ctx->CSSetShader(shader, nullptr, 0);
    ctx->CSSetConstantBuffers(2, 1, &light_cb);
    ctx->CSSetConstantBuffers(4, 1, &point_cb);
    ctx->CSSetConstantBuffers(6, 1, &spot_cb);
    ctx->CSSetConstantBuffers(7, 1, &area_cb);
    ctx->CSSetConstantBuffers(9, 1, &cb);
    ctx->CSSetShaderResources(8, 3, lights);
    ctx->CSSetShaderResources(7, 1, &directional_shadow);
    ctx->CSSetShaderResources(13, 4, inputs);
    ctx->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
    ctx->Dispatch(constants_.count_x, constants_.count_y, constants_.count_z);
    ID3D11UnorderedAccessView* null_uav = nullptr;
    ID3D11ShaderResourceView* null_inputs[4]{};
    ID3D11Buffer* null_cb = nullptr;
    ctx->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
    ctx->CSSetShaderResources(8, 3, null_inputs);
    ctx->CSSetShaderResources(7, 1, null_inputs);
    ctx->CSSetShaderResources(13, 4, null_inputs);
    // Filter the finished raw field for lighting. The next update still reads
    // the unfiltered field, avoiding repeated blur through temporal history.
    ID3D11ShaderResourceView* filter_inputs[2] = {
        source.volume_srv(), probe_srv_[next].Get()
    };
    ID3D11UnorderedAccessView* filtered_output = filtered_probe_uav_.Get();
    ID3D11ShaderResourceView* null_srv = nullptr;
    ctx->PSSetShaderResources(14, 1, &null_srv);
    ctx->CSSetShader(filter_shader, nullptr, 0);
    ctx->CSSetShaderResources(13, 2, filter_inputs);
    ctx->CSSetUnorderedAccessViews(0, 1, &filtered_output, nullptr);
    ctx->Dispatch(constants_.count_x, constants_.count_y, constants_.count_z);
    ctx->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
    ctx->CSSetShaderResources(13, 2, null_inputs);
    ctx->CSSetConstantBuffers(2, 1, &null_cb);
    ctx->CSSetConstantBuffers(4, 1, &null_cb);
    ctx->CSSetConstantBuffers(6, 1, &null_cb);
    ctx->CSSetConstantBuffers(7, 1, &null_cb);
    ctx->CSSetConstantBuffers(9, 1, &null_cb);
    ctx->CSSetShader(nullptr, nullptr, 0);
    if (inject_lights_)
    {
        graphics.get_point_light_manager().upload_to_gpu(ctx);
        graphics.get_spot_light_manager().upload_to_gpu(ctx);
        graphics.get_area_light_manager().upload_to_gpu(ctx);
    }
    current_ = next;
    has_data_ = true;
}

void DDGI::render_debug(ID3D11DeviceContext* ctx) const
{
    if (!show_probes_ || !debug_sphere_ || !grid_valid_ || !has_data_) return;
    const auto camera_base = CameraManager::instance().get_active_camera();
    if (!camera_base) return;
    const Camera& camera = camera_base->get_camera();
    // Draw after the post-process chain. A debug marker must not alter the
    // scene depth or the source color that volumetric fog integrates.
    Render_State::instance().set_render_states(ctx,
        Depth_State::Test_Disable_Write_Disable,
        Blend_State::Alpha, Rasterizer_State::Wireframe_CCW);
    const float spacing = constants_.inv_extent.w;
    const float radius = (std::max)(0.025f, spacing * 0.12f);
    const Frustum frustum = ExtractFrustum(camera.view, camera.projection);
    for (unsigned int z = 0; z < constants_.count_z; ++z)
    for (unsigned int y = 0; y < constants_.count_y; ++y)
    for (unsigned int x = 0; x < constants_.count_x; ++x)
    {
        const unsigned int index = x + constants_.count_x * (y + constants_.count_y * z);
        const float px = constants_.volume_min.x + (x + 0.5f) / (constants_.count_x * constants_.inv_extent.x);
        const float py = constants_.volume_min.y + (y + 0.5f) / (constants_.count_y * constants_.inv_extent.y);
        const float pz = constants_.volume_min.z + (z + 0.5f) / (constants_.count_z * constants_.inv_extent.z);
        if (!SphereIntersectsFrustum(frustum,
            DirectX::XMVectorSet(px, py, pz, 1), radius)) continue;
        DirectX::XMFLOAT4X4 world{};
        DirectX::XMStoreFloat4x4(&world,
            DirectX::XMMatrixScaling(radius, radius, radius) *
            DirectX::XMMatrixTranslation(px, py, pz));
        const bool ready = frame_ >= 8 || index % 8 < frame_;
        const DirectX::XMFLOAT4 color = ready ?
            DirectX::XMFLOAT4{ 0.1f, 0.9f, 0.9f, 0.35f } :
            DirectX::XMFLOAT4{ 1.0f, 0.65f, 0.1f, 0.35f };
        debug_sphere_->Render(ctx, world, color);
    }
}

void DDGI::bind_lighting(ID3D11DeviceContext* ctx) const
{
    ID3D11ShaderResourceView* srv = enabled_ && grid_valid_ && has_data_ ? filtered_probe_srv_.Get() : nullptr;
    ID3D11Buffer* cb = enabled_ && grid_valid_ && has_data_ ? constants_buffer_.Get() : nullptr;
    ctx->PSSetShaderResources(14, 1, &srv);
    ctx->PSSetConstantBuffers(9, 1, &cb);
}

bool DDGI::bind_fog(ID3D11DeviceContext* ctx) const
{
    if (!enabled_ || !grid_valid_ || !has_data_) return false;
    ID3D11ShaderResourceView* srv = filtered_probe_srv_.Get();
    ID3D11Buffer* cb = constants_buffer_.Get();
    ctx->CSSetShaderResources(14, 1, &srv);
    ctx->CSSetConstantBuffers(9, 1, &cb);
    return true;
}

void DDGI::draw_gui()
{
    if (!ImGui::CollapsingHeader("DDGI")) return;
    ImGui::Checkbox("Enabled##DDGI", &enabled_);
    ImGui::SliderFloat("Probe strength##DDGI", &constants_.strength, 0.0f, 2.0f);
    ImGui::SliderFloat("Voxel / probe blend##DDGI", &constants_.blend, 0.0f, 1.0f);
    ImGui::SliderFloat("Probe hysteresis##DDGI", &constants_.hysteresis, 0.0f, 0.98f);
    ImGui::Checkbox("Show probes in 3D##DDGI", &show_probes_);
    ImGui::DragFloat3("Probe min XYZ##DDGI", requested_min_, 0.1f);
    ImGui::DragFloat3("Probe max XYZ##DDGI", requested_max_, 0.1f);
    if (ImGui::Button("Apply probe bounds##DDGI"))
        set_probe_bounds({ requested_min_[0], requested_min_[1], requested_min_[2] },
            { requested_max_[0], requested_max_[1], requested_max_[2] });
    ImGui::TextDisabled("Probe spacing: %.2f m", constants_.inv_extent.w);
    ImGui::SliderInt3("Probe grid XYZ##DDGI", requested_counts_, 2, 12);
    if (ImGui::Button("Apply probe grid##DDGI"))
    {
        const unsigned int x = static_cast<unsigned int>(requested_counts_[0]);
        const unsigned int y = static_cast<unsigned int>(requested_counts_[1]);
        const unsigned int z = static_cast<unsigned int>(requested_counts_[2]);
        if (x != constants_.count_x || y != constants_.count_y || z != constants_.count_z)
        {
            constants_.count_x = x;
            constants_.count_y = y;
            constants_.count_z = z;
            create_probe_buffers();
        }
    }
    ImGui::TextDisabled("%u x %u x %u probes; %u / 8 update groups ready",
        constants_.count_x, constants_.count_y, constants_.count_z,
        frame_ < 8 ? frame_ : 8);
    if (!grid_valid_) ImGui::TextDisabled("Probe grid allocation failed");
    if (ImGui::Checkbox("Inject scene lights##DDGI", &inject_lights_))
    {
        frame_ = 0;
        has_data_ = false;
    }
    ImGui::SliderFloat("Light multiplier##DDGI", &constants_.light_multiplier, 0.0f, 2.0f);
}
