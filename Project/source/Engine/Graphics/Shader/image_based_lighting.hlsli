
#ifndef __IMAGE_BASED_LIGHTING__
#define __IMAGE_BASED_LIGHTING__
#include"samplers.hlsli"
Texture2D skybox : register(t32);
TextureCube diffuse_iem : register(t33);
TextureCube specular_pmrem : register(t34);
Texture2D lut_ggx : register(t35);
TextureCube local_reflection_probe0 : register(t36);
TextureCube local_reflection_probe1 : register(t37);
TextureCube local_reflection_probe2 : register(t38);
cbuffer REFLECTION_PROBE_CONSTANTS : register(b10)
{
    float4 reflection_probe_positions[3];
    float4 reflection_probe_min[3];
    float4 reflection_probe_max[3];
    float4 reflection_probe_settings; // x: count, y: edge fade in metres, z: strength
};

float4 sample_lut_ggx(float2 brdf_sample_point)
{
    return lut_ggx.Sample(sampler_states[LINEAR_WRAP], brdf_sample_point);
}
float4 sample_skybox(float3 v, float roughness)
{
    const float PI = 3.14159265358979;
    uint width, height, number_of_levels;
    skybox.GetDimensions(0, width, height, number_of_levels);

    float lod = roughness * float(number_of_levels - 1);

    v = normalize(v);
    // Blinn/Newell Latitude Mapping
    float2 sample_point;
    sample_point.x = (atan2(v.z, v.x) + PI) / (PI * 2.0);
    sample_point.y = 1.0 - ((asin(v.y) + PI * 0.5) / PI);
    return skybox.SampleLevel(sampler_states[LINEAR_WRAP], sample_point, lod);
}
float4 sample_diffuse_iem(float3 v)
{
    return diffuse_iem.Sample(sampler_states[LINEAR_WRAP], v);
}
float4 sample_specular_pmrem(float3 v, float roughness)
{
    uint width, height, number_of_levels;
    specular_pmrem.GetDimensions(0, width, height, number_of_levels);

    float lod = roughness * float(number_of_levels - 1);

    return specular_pmrem.SampleLevel(sampler_states[LINEAR_WRAP], v, lod);
}

float3 sample_specular_environment(float3 position, float3 direction, float roughness)
{
    float3 global_radiance = sample_specular_pmrem(direction, roughness).rgb;
    float3 local_radiance = 0;
    float total_weight = 0;
    [unroll] for (uint i = 0; i < 3; ++i)
    {
        if (i >= (uint)reflection_probe_settings.x) break;
        float3 box_min = reflection_probe_min[i].xyz;
        float3 box_max = reflection_probe_max[i].xyz;
        float3 edge = min(position - box_min, box_max - position);
        if (any(edge < 0)) continue;
        float weight = saturate(min(edge.x, min(edge.y, edge.z)) /
            max(reflection_probe_settings.y, 0.01));
        float3 safe_direction = (step(0.0, direction) * 2.0 - 1.0) *
            max(abs(direction), 1e-5);
        float3 exit_plane = lerp(box_min, box_max, step(0.0, direction));
        float3 exit_distance = (exit_plane - position) / safe_direction;
        float hit_distance = max(0.0, min(exit_distance.x, min(exit_distance.y, exit_distance.z)));
        float3 corrected = normalize(position + direction * hit_distance -
            reflection_probe_positions[i].xyz);
        float3 radiance = 0;
        if (i == 0) radiance = local_reflection_probe0.SampleLevel(
            sampler_states[LINEAR_CLAMP], corrected, roughness * 7.0).rgb;
        else if (i == 1) radiance = local_reflection_probe1.SampleLevel(
            sampler_states[LINEAR_CLAMP], corrected, roughness * 7.0).rgb;
        else radiance = local_reflection_probe2.SampleLevel(
            sampler_states[LINEAR_CLAMP], corrected, roughness * 7.0).rgb;
        local_radiance += radiance * weight;
        total_weight += weight;
    }
    return lerp(global_radiance, local_radiance / max(total_weight, 1e-5),
        saturate(total_weight) * saturate(reflection_probe_settings.z));
}

#endif
