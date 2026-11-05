#include "gltf_model.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"

struct texture_info { int index; int texcoord; };
struct normal_texture_info { int index; int texcoord; float scale; };
struct occlusion_texture_info { int index; int texcoord; float strength; };
struct pbr_metallic_roughness
{
    float4 basecolor_factor;
    texture_info basecolor_texture;
    float metallic_factor;
    float roughness_factor;
    texture_info metallic_roughness_texture;
};
struct material_constants
{
    float3 emissive_factor;
    int alpha_mode;
    float alpha_cutoff;
    int double_sided;
    pbr_metallic_roughness pbr_metallic_roughness;
    normal_texture_info normal_texture;
    occlusion_texture_info occlusion_texture;
    texture_info emissive_texture;
};
StructuredBuffer<material_constants> materials : register(t0);
Texture2D<float4> material_textures[5] : register(t1);
#define BASECOLOR_TEXTURE 0

cbuffer VOXEL_GI_CONSTANTS : register(b8)
{
    float4 volume_min;
    float4 inv_extent;
    uint resolution;
    uint slice;
    float strength;
    float padding;
};

RWTexture3D<float4> voxel_radiance : register(u0);

void main(VS_OUT input)
{
    float3 p = (input.w_position.xyz - volume_min.xyz) * inv_extent.xyz;
    if (any(p < 0) || any(p >= 1)) return;

    material_constants m = materials[material];
    float4 albedo = m.pbr_metallic_roughness.basecolor_factor;
    if (m.pbr_metallic_roughness.basecolor_texture.index >= 0)
    {
        float4 texel = material_textures[BASECOLOR_TEXTURE].Sample(sampler_states[ANISOTROPIC_WRAP], input.texcoord);
        albedo *= float4(pow(saturate(texel.rgb), 2.2), texel.a);
    }
    if (m.alpha_mode == 1) clip(albedo.a - m.alpha_cutoff);

    float3 N = normalize(input.w_normal.xyz);
    float3 L = normalize(-directional_light_direction.xyz);
    float irradiance = directional_light_color.w / 683.0;
    // A small sky term keeps unlit surfaces from making the first bounce black.
    float3 radiance = albedo.rgb * (0.08 +
        directional_light_color.rgb * irradiance * saturate(dot(N, L))) / 3.14159265;
    radiance += m.emissive_factor;
    uint3 cell = min((uint3)(p * resolution), resolution - 1);
    voxel_radiance[cell] = float4(radiance, 1);
}
