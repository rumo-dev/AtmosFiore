// BLOOM
#include "bloom.hlsli"

#include "samplers.hlsli"

Texture2D hdr_color_buffer_texture : register(t0);
float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    float4 sampled_color = hdr_color_buffer_texture.Sample(sampler_states[POINT_CLAMP], texcoord);
    sampled_color = clamp(sampled_color, 0.0, 1.0);

#if 0
    return float4(step(bloom_extraction_threshold, dot(sampled_color.rgb, float3(0.299, 0.587, 0.114))) * sampled_color.rgb * effect_data.bloom_intensity, sampled_color.a);
#else	
    return float4(step(bloom_extraction_threshold, max(sampled_color.r, max(sampled_color.g, sampled_color.b))) * sampled_color.rgb, sampled_color.a);
#endif	
}
