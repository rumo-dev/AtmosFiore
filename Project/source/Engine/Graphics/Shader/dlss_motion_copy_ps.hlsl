#include "fullscreen_quad.hlsli"

Texture2D<float2> motion_vectors : register(t0);

#define Point_Clamp 0
SamplerState sampler_states[9] : register(s0);

float2 main(VS_OUT pin) : SV_TARGET
{
    return motion_vectors.Sample(sampler_states[Point_Clamp], pin.texcoord);
}
