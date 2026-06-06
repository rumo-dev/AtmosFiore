// BLOOM

#include "samplers.hlsli"

Texture2D hdr_color_buffer_texture : register(t0);

float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    return hdr_color_buffer_texture.Sample(sampler_states[LINEAR_CLAMP], texcoord, 0.0);
}
