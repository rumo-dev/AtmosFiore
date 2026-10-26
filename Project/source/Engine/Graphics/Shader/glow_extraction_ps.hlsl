// BLOOM
#include "bloom.hlsli"

#include "samplers.hlsli"

Texture2D hdr_color_buffer_texture : register(t0);
float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
    float4 sampled_color = hdr_color_buffer_texture.Sample(sampler_states[POINT_CLAMP], texcoord);
    float3 hdr = max(sampled_color.rgb, 0.0f);
    float brightness = max(hdr.r, max(hdr.g, hdr.b));
    float threshold = max(bloom_extraction_threshold, 0.0f);
    // Extract only energy above the threshold so broad sky regions do not
    // contribute a low-level glow around every foreground silhouette.
    float contribution = saturate(brightness - threshold) / max(brightness, 1.0e-4f);
    return float4(hdr * contribution, sampled_color.a);
}
