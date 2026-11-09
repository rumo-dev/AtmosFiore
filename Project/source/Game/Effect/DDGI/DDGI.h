#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>
#include "Engine/Graphics/model/debug_primitive.h"

class VoxelGI;

// Diffuse probe field updated from the static voxel radiance volume.
class DDGI
{
public:
    static constexpr unsigned int DefaultGridSize = 8;
    bool initialize(ID3D11Device* device);
    void set_probe_bounds(DirectX::XMFLOAT3 minimum, DirectX::XMFLOAT3 maximum);
    void update(ID3D11DeviceContext* context, const VoxelGI& source);
    void bind_lighting(ID3D11DeviceContext* context) const;
    bool bind_fog(ID3D11DeviceContext* context) const;
    void render_debug(ID3D11DeviceContext* context) const;
    void draw_gui();

private:
    struct Constants
    {
        DirectX::XMFLOAT4 volume_min{};
        DirectX::XMFLOAT4 inv_extent{};
        unsigned int count_x = 12;
        unsigned int count_y = DefaultGridSize;
        unsigned int count_z = 12;
        float strength = 1.0f;
        unsigned int frame = 0;
        float hysteresis = 0.85f;
        float blend = 0.85f;
        float visibility_bias = 0.08f;
        unsigned int inject_lights = 1;
        float light_multiplier = 1.0f;
        float padding[2]{};
        DirectX::XMFLOAT4 voxel_min{};
        DirectX::XMFLOAT4 voxel_inv_extent{};
        unsigned int voxel_resolution = 64;
        float voxel_padding[3]{};
    } constants_;
    static_assert(sizeof(Constants) == 128, "DDGI shader constant layout changed");

    bool create_probe_buffers();

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> probes_[2];
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> probe_srv_[2];
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> probe_uav_[2];
    Microsoft::WRL::ComPtr<ID3D11Buffer> filtered_probes_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> filtered_probe_srv_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> filtered_probe_uav_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constants_buffer_;
    unsigned int source_revision_ = 0;
    unsigned int frame_ = 0;
    int current_ = 0;
    bool has_data_ = false;
    bool enabled_ = true;
    bool show_probes_ = false;
    bool inject_lights_ = true;
    bool grid_valid_ = false;
    int requested_counts_[3] = { 12, 8, 12 };
    float requested_min_[3] = {};
    float requested_max_[3] = {};
    bool custom_bounds_ = false;
    std::unique_ptr<Debug_Sphere> debug_sphere_;
};
