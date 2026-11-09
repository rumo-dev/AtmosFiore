#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"

Texture2D<float> shadow_mask : register(t0);
Texture2D<float4> gbuffer_normal : register(t1);
Texture2D<float4> gbuffer_position : register(t2);

cbuffer DIRECTIONAL_SHADOW_BLUR_CONSTANTS : register(b12)
{
    float2 blur_direction;
    float blur_radius_pixels;
    float depth_sigma;
    float normal_power;
    float spatial_sigma;
    float2 padding;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    float3 center_normal = gbuffer_normal.SampleLevel(
        sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz;
    if (dot(center_normal, center_normal) < 1.0e-4f)
        return 1.0f.xxxx;
    center_normal = normalize(center_normal);
    float3 center_position = gbuffer_position.SampleLevel(
        sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz;

    uint width, height;
    shadow_mask.GetDimensions(width, height);
    float2 texel_size = 1.0f / float2(width, height);
    float weighted_visibility = 0.0f;
    float weight_sum = 0.0f;
    [unroll]
    for (int tap = -4; tap <= 4; ++tap)
    {
        float2 sample_uv = pin.texcoord + blur_direction * texel_size *
            (tap * blur_radius_pixels * 0.25f);
        if (any(sample_uv < 0.0f) || any(sample_uv > 1.0f))
            continue;

        float3 sample_normal = gbuffer_normal.SampleLevel(
            sampler_states[POINT_CLAMP], sample_uv, 0).xyz;
        if (dot(sample_normal, sample_normal) < 1.0e-4f)
            continue;
        sample_normal = normalize(sample_normal);
        float3 sample_position = gbuffer_position.SampleLevel(
            sampler_states[POINT_CLAMP], sample_uv, 0).xyz;
        // View depth changes across a tilted floor even on one continuous
        // surface. Measure separation from the receiver's tangent plane.
        float plane_distance = abs(dot(sample_position - center_position, center_normal));
        float spatial_weight = exp(-0.5f * tap * tap /
            max(spatial_sigma * spatial_sigma, 1.0e-4f));
        float depth_weight = exp(-plane_distance * depth_sigma);
        float normal_weight = pow(saturate(dot(center_normal, sample_normal)), normal_power);
        float weight = spatial_weight * depth_weight * normal_weight;
        float visibility = shadow_mask.SampleLevel(
            sampler_states[LINEAR_CLAMP], sample_uv, 0);
        weighted_visibility += visibility * weight;
        weight_sum += weight;
    }
    float visibility = weighted_visibility / max(weight_sum, 1.0e-5f);
    return visibility.xxxx;
}
