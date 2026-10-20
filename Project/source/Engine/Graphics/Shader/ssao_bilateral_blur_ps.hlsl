// 13-tap cross-bilateral obscurance filter from the Alchemy AO pipeline.
#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"

Texture2D<float4> input_obscurance : register(t0);
Texture2D<float4> gbuffer_position : register(t1);

cbuffer SSAO_FILTER_CONSTANT_BUFFER : register(b12)
{
    float ssao_radius;
    float ssao_bias;
    float ssao_intensity;
    float ssao_contrast;
    float spatial_sigma;
    float position_sigma;
    float2 blur_direction;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 center_position = gbuffer_position.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float3 center_view_position = mul(float4(center_position.xyz, 1.0f), view).xyz;
    float2 texel_size;
    uint width, height;
    input_obscurance.GetDimensions(width, height);
    texel_size = 1.0f / float2(width, height);

    float weighted_sum = 0.0f;
    float weight_sum = 0.0f;
    [unroll]
    for (int tap = -6; tap <= 6; ++tap)
    {
        float2 uv = pin.texcoord + blur_direction * texel_size * tap;
        if (any(uv < 0.0f) || any(uv > 1.0f))
            continue;

        float4 sample_position = gbuffer_position.SampleLevel(sampler_states[POINT_CLAMP], uv, 0);
        float3 sample_view_position = mul(float4(sample_position.xyz, 1.0f), view).xyz;
        float3 delta = sample_view_position - center_view_position;
        float spatial_weight = exp(-0.5f * (tap * tap) / max(spatial_sigma * spatial_sigma, 1.0e-4f));
        float surface_weight = exp(-length(delta) * position_sigma);
        float weight = spatial_weight * surface_weight;
        float obscurance = input_obscurance.SampleLevel(sampler_states[POINT_CLAMP], uv, 0).r;
        weighted_sum += obscurance * weight;
        weight_sum += weight;
    }

    float filtered = weighted_sum / max(weight_sum, 1.0e-5f);
    return float4(filtered, filtered, filtered, 1.0f);
}
