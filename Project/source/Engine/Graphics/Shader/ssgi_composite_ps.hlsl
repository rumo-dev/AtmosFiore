#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"

Texture2D<float4> scene_color : register(t0);
Texture2D<float4> indirect_light : register(t1);
Texture2D<float4> gbuffer_normal : register(t2);
Texture2D<float4> gbuffer_position : register(t3);

cbuffer SSGI_CONSTANT_BUFFER : register(b12)
{
    float ssgi_radius;
    float ssgi_intensity;
    float ssgi_thickness;
    float ssgi_debug_view;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    float3 scene = scene_color.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0).rgb;
    float4 center_position = gbuffer_position.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float3 center_normal = normalize(gbuffer_normal.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz);
    uint width, height;
    indirect_light.GetDimensions(width, height);
    float2 texel = 1.0f / float2(width, height);
    float3 gi = 0.0f;
    float total_weight = 0.0f;
    [unroll]
    for (int y = 0; y < 2; ++y)
    {
        [unroll]
        for (int x = 0; x < 2; ++x)
        {
            float2 offset = (float2(x, y) - 0.5f) * texel;
            float2 sample_uv = saturate(pin.texcoord + offset);
            float4 sample_position = gbuffer_position.SampleLevel(sampler_states[POINT_CLAMP], sample_uv, 0);
            float3 sample_normal = normalize(gbuffer_normal.SampleLevel(sampler_states[POINT_CLAMP], sample_uv, 0).xyz);
            float distance_weight = exp(-length(sample_position.xyz - center_position.xyz) * 3.0f);
            float normal_weight = pow(saturate(dot(sample_normal, center_normal)), 8.0f);
            float spatial_weight = (x == 0 ? 0.75f : 0.25f) * (y == 0 ? 0.75f : 0.25f);
            float weight = distance_weight * normal_weight * spatial_weight;
            gi += indirect_light.SampleLevel(sampler_states[POINT_CLAMP], sample_uv, 0).rgb * weight;
            total_weight += weight;
        }
    }
    gi /= max(total_weight, 1.0e-5f);
    if (ssgi_debug_view > 1.5f)
        return float4(scene, 1.0f);
    if (ssgi_debug_view > 0.5f)
        return float4(gi * 4.0f, 1.0f);
    return float4(scene + gi, 1.0f);
}
