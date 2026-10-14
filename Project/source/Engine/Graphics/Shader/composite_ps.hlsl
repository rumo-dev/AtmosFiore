#include "dof_common.hlsli"

Texture2D color_map : register(t0);
Texture2D coc_map : register(t1);
Texture2D ffs_result : register(t2);

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;
    float4 original = color_map.Sample(sampler_states[LINEAR_CLAMP], uv);
    if (!is_dof)
        return original;

    if (debug_view == 1)
    {
        float3 result = ffs_result.Sample(sampler_states[LINEAR_CLAMP], uv).rgb;
        return float4(result, 1.0f);
    }
    if (debug_view == 2)
    {
        float coc = coc_map.Sample(sampler_states[LINEAR_CLAMP], uv).r;
        return float4(coc, 0.0f, 0.0f, 1.0f);
    }
    if (debug_view == 3)
    {
        float coc = coc_map.Sample(sampler_states[LINEAR_CLAMP], uv).g;
        return float4(0.0f, coc, 0.0f, 1.0f);
    }

    // The FFS resolve is already the complete image. No CoC-based lerp with
    // the original is needed after the filter weights have been normalized.
    float3 result = ffs_result.Sample(sampler_states[LINEAR_CLAMP], uv).rgb;
    return float4(result, original.a);
}
