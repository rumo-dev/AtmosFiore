// The Alchemy Screen-Space Ambient Obscurance Algorithm
// https://casual-effects.com/research/McGuire2011AlchemyAO/VV11AlchemyAO.pdf
#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"

Texture2D<float4> gbuffer_normal : register(t0);
Texture2D<float4> gbuffer_position : register(t1);
cbuffer SSAO_CONSTANT_BUFFER : register(b12)
{
    float ssao_radius;
    float ssao_bias;
    float ssao_intensity;
    float ssao_contrast;
};

float hash_pixel(float2 pixel)
{
    float3 p = frac(float3(pixel.xyx) * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 center_position = gbuffer_position.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float3 world_normal = gbuffer_normal.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz;
    if (dot(world_normal, world_normal) < 1.0e-4f)
        return float4(1.0f, 1.0f, 1.0f, 1.0f);

    const int sample_count = 16;
    const float radius = max(ssao_radius, 1.0e-3f);

    float3 position = mul(float4(center_position.xyz, 1.0f), view).xyz;
    float3 normal = normalize(mul(float4(world_normal, 0.0f), view).xyz);
    float rotation = hash_pixel(pin.position.xy) * 6.2831853f;
    // Alchemy samples a disk parallel to the image plane. Project the
    // world-space radius at this pixel's camera-space depth to UV units.
    float2 radius_uv = 0.5f * radius * float2(projection[0][0], projection[1][1]) / max(position.z, 1.0e-4f);
    float occlusion = 0.0f;

    [unroll]
    for (int i = 0; i < sample_count; ++i)
    {
        // Stratified uniform-disk points, rotated per pixel to suppress a
        // visible screen-aligned sampling pattern.
        float t = (i + 0.5f) / sample_count;
        float phi = i * 2.39996323f + rotation;
        float2 disk = sqrt(t) * float2(cos(phi), sin(phi));
        float2 sample_uv = pin.texcoord + disk * radius_uv;
        if (any(sample_uv <= 0.0f) || any(sample_uv >= 1.0f))
            continue;

        float3 sample_normal = gbuffer_normal.SampleLevel(sampler_states[POINT_CLAMP], sample_uv, 0).xyz;
        if (dot(sample_normal, sample_normal) < 1.0e-4f)
            continue;
        float3 hit_world_position = gbuffer_position.SampleLevel(sampler_states[POINT_CLAMP], sample_uv, 0).xyz;
        float3 hit_position = mul(float4(hit_world_position, 1.0f), view).xyz;
        float3 v = hit_position - position;
        float distance_squared = dot(v, v);
        if (distance_squared <= 1.0e-5f || distance_squared >= radius * radius)
            continue;

        // Equation (10): signed normal distance, depth-scaled bias and
        // inverse-square falloff, with the paper's finite-radius cutoff.
        occlusion += max(0.0f, dot(normal, v) - position.z * ssao_bias) / (distance_squared + 1.0e-4f);
    }

    float ao = pow(saturate(1.0f - (2.0f * ssao_intensity * occlusion / sample_count)), ssao_contrast);
    return float4(ao, ao, ao, 1.0f);
}
