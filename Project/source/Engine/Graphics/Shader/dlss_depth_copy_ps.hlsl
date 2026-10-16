#include "fullscreen_quad.hlsli"

Texture2D<float> depth_map : register(t0);

#define Point_Clamp 0
SamplerState sampler_states[9] : register(s0);

float main(VS_OUT pin) : SV_TARGET
{
    return depth_map.Sample(sampler_states[Point_Clamp], pin.texcoord);
}
