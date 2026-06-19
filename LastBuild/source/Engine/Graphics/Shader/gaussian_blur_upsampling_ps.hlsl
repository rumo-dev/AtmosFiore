// BLOOM
#include "bloom.hlsli"

#include "samplers.hlsli"

static const uint downsampled_count = 6;
Texture2D downsampled_textures[downsampled_count] : register(t0);

float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    if (!is_enabled == 1)
    {
        return downsampled_textures[0].Sample(sampler_states[LINEAR_CLAMP], texcoord);
    }
    float3 sampled_color = 0;
	[unroll]
    for (uint downsampled_index = 0; downsampled_index < downsampled_count; ++downsampled_index)
    {
        sampled_color += downsampled_textures[downsampled_index].Sample(sampler_states[LINEAR_CLAMP], texcoord).xyz;
    }
    return float4(sampled_color * bloom_intensity, 1);
}
