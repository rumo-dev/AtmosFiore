#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"

Texture2D<float4> scene_color : register(t0);
Texture2D<float4> lens_flare : register(t1);

float4 main(VS_OUT pin) : SV_TARGET
{
    float3 scene = scene_color.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0).rgb;
    float3 flare = lens_flare.SampleLevel(sampler_states[LINEAR_CLAMP], pin.texcoord, 0).rgb;
    return float4(scene + flare, 1.0f);
}
