#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"

Texture2D<float4> scene_color : register(t0);
Texture2D<float4> half_resolution_reflection_delta : register(t1);

float4 main(VS_OUT pin) : SV_TARGET
{
    const float4 base_color = scene_color.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    const float3 reflection_delta = half_resolution_reflection_delta.SampleLevel(
        sampler_states[LINEAR_CLAMP], pin.texcoord, 0).rgb;
    return float4(base_color.rgb + reflection_delta, base_color.a);
}
