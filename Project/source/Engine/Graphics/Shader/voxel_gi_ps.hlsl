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
#define EMISSIVE_TEXTURE 3

cbuffer VOXEL_GI_CONSTANTS : register(b8)
{
    float4 volume_min;
    float4 inv_extent;
    uint resolution;
    uint slice;
    float strength;
    float emissive_multiplier;
    float environment_fill;
    uint emissive_only;
    float2 padding;
};

RWTexture3D<float4> voxel_radiance : register(u0);
RWTexture3D<float4> voxel_albedo : register(u1);
RWTexture3D<float4> voxel_normal : register(u2);

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
    // Translucent geometry must not become a fully opaque light blocker.
    if (m.alpha_mode == 2) return;

    // Static material-only bake: diffuse environment fill and self-emission.
    // Dynamic scene lights are handled by the regular deferred lighting pass.
    float3 radiance = albedo.rgb * (environment_fill / 3.14159265);
    // Match the glTF G-buffer: emissiveFactor * linearized emissive texture.
    // Emission is independent of surface normal.
    float3 emissive = m.emissive_factor;
    if (m.emissive_texture.index >= 0)
    {
        float3 texel = material_textures[EMISSIVE_TEXTURE].Sample(
            sampler_states[ANISOTROPIC_WRAP], input.texcoord).rgb;
        emissive *= pow(saturate(texel), 2.2);
    }
    radiance += emissive * emissive_multiplier;
    uint3 cell = min((uint3)(p * resolution), resolution - 1);
    // A later wall or trim triangle can share this voxel with a thin light
    // strip. The second pass writes emitters last so they remain visible.
    if (emissive_only != 0)
    {
        if (!any(emissive > 0.0f)) return;
        voxel_radiance[cell] = float4(radiance, 1);
        return;
    }
    voxel_radiance[cell] = float4(radiance, 1);
    voxel_albedo[cell] = float4(albedo.rgb, 1);
    voxel_normal[cell] = float4(normalize(input.w_normal.xyz), 1);
}
