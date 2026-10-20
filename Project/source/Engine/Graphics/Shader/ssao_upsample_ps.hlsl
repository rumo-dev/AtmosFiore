// Edge-aware reconstruction from half-resolution obscurance to full resolution.
#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"

Texture2D<float4> low_resolution_obscurance : register(t0);
Texture2D<float4> gbuffer_position : register(t1);
cbuffer SSAO_UPSAMPLE_CONSTANT_BUFFER : register(b12)
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
    uint width, height;
    low_resolution_obscurance.GetDimensions(width, height);
    float2 dimensions = float2(width, height);
    float2 low_position = pin.texcoord * dimensions - 0.5f;
    int2 base = int2(floor(low_position));
    float3 center_world = gbuffer_position.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz;
    float3 center_view = mul(float4(center_world, 1.0f), view).xyz;

    float total = 0.0f;
    float total_weight = 0.0f;
    [unroll]
    for (int y = 0; y < 2; ++y)
    {
        [unroll]
        for (int x = 0; x < 2; ++x)
        {
            int2 texel = clamp(base + int2(x, y), int2(0, 0), int2(width - 1, height - 1));
            float2 sample_uv = (float2(texel) + 0.5f) / dimensions;
            float3 sample_world = gbuffer_position.SampleLevel(sampler_states[POINT_CLAMP], sample_uv, 0).xyz;
            float3 sample_view = mul(float4(sample_world, 1.0f), view).xyz;
            float2 delta = abs(low_position - float2(texel));
            float spatial_weight = (1.0f - delta.x) * (1.0f - delta.y);
            float surface_weight = exp(-length(sample_view - center_view) * position_sigma);
            float weight = max(spatial_weight, 0.0f) * surface_weight;
            float ao = low_resolution_obscurance.Load(int3(texel, 0)).r;
            total += ao * weight;
            total_weight += weight;
        }
    }

    float result = total / max(total_weight, 1.0e-5f);
    return float4(result, result, result, 1.0f);
}
