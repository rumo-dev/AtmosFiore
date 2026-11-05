#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

class Gltf_Model;

// Single-volume, one-bounce diffuse voxel GI for the static Library scene.
class VoxelGI
{
public:
    static constexpr UINT Resolution = 64;

    bool initialize(ID3D11Device* device, Gltf_Model* model,
        const DirectX::XMFLOAT4X4& world);
    void render(ID3D11DeviceContext* context, Gltf_Model* model,
        const DirectX::XMFLOAT4X4& world);
    void bind_lighting(ID3D11DeviceContext* context) const;
    void render_debug_3d(ID3D11DeviceContext* context) const;
    void draw_gui();
    void set_emissive_multiplier(float value) { constants_.emissive_multiplier = value; baked_ = false; }

private:
    struct Constants
    {
        DirectX::XMFLOAT4 volume_min{};
        DirectX::XMFLOAT4 inv_extent{};
        UINT resolution = Resolution;
        UINT slice = Resolution / 2;
        float strength = 0.45f;
        float emissive_multiplier = 1.0f;
        float environment_fill = 0.08f;
        float padding[3] = {};
    } constants_;

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11Texture3D> volume_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> volume_uav_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> volume_srv_;
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
};
