#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <string>
#include <utility>

class Gltf_Model;

// Single-volume, one-bounce diffuse voxel GI for a static scene.
class VoxelGI
{
public:
    static constexpr UINT DefaultResolution = 64;

    bool initialize(ID3D11Device* device, Gltf_Model* model,
        const DirectX::XMFLOAT4X4& world);
    void render(ID3D11DeviceContext* context, Gltf_Model* model,
        const DirectX::XMFLOAT4X4& world);
    void bind_lighting(ID3D11DeviceContext* context) const;
    void render_debug_3d(ID3D11DeviceContext* context) const;
    void draw_gui();
    void set_resolution(UINT resolution) { resolution_ = resolution; }
    void set_emissive_multiplier(float value) { constants_.emissive_multiplier = value; baked_ = false; }
    void set_strength(float value) { constants_.strength = value; }
    void set_bounds_root(std::string name) { bounds_root_name_ = std::move(name); }
    void set_debug_slice_height(float height) { debug_slice_height_ = height; use_debug_slice_height_ = true; }
    ID3D11ShaderResourceView* volume_srv() const { return baked_ ? volume_srv_.Get() : nullptr; }
    ID3D11ShaderResourceView* albedo_srv() const { return baked_ ? albedo_srv_.Get() : nullptr; }
    ID3D11ShaderResourceView* normal_srv() const { return baked_ ? normal_srv_.Get() : nullptr; }
    DirectX::XMFLOAT4 volume_min() const { return constants_.volume_min; }
    DirectX::XMFLOAT4 inverse_extent() const { return constants_.inv_extent; }
    unsigned int resolution() const { return resolution_; }
    unsigned int bake_revision() const { return bake_revision_; }
    bool show_3d_volume() const { return show_3d_; }

private:
    struct Constants
    {
        DirectX::XMFLOAT4 volume_min{};
        DirectX::XMFLOAT4 inv_extent{};
        UINT resolution = DefaultResolution;
        UINT slice = DefaultResolution / 2;
        float strength = 0.45f;
        float emissive_multiplier = 1.0f;
        float environment_fill = 0.08f;
        UINT emissive_only = 0;
        float padding[2] = {};
    } constants_;

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Gltf_Model* model_ = nullptr;
    DirectX::XMFLOAT4X4 world_{};
    UINT resolution_ = DefaultResolution;
    std::string bounds_root_name_;
    float debug_slice_height_ = 0.0f;
    bool use_debug_slice_height_ = false;
    Microsoft::WRL::ComPtr<ID3D11Texture3D> volume_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> volume_uav_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> volume_srv_;
    Microsoft::WRL::ComPtr<ID3D11Texture3D> albedo_volume_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> albedo_uav_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> albedo_srv_;
    Microsoft::WRL::ComPtr<ID3D11Texture3D> normal_volume_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> normal_uav_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normal_srv_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constants_buffer_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> slice_texture_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> slice_uav_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> slice_srv_;
    bool enabled_ = true;
    bool show_slice_ = false;
    bool show_3d_ = false;
    bool baked_ = false;
    bool bake_settings_dirty_ = false;
    bool slice_dirty_ = true;
    unsigned int bake_revision_ = 0;
    unsigned int emissive_voxel_count_ = 0;
    unsigned int bright_emissive_voxel_count_ = 0;
};
