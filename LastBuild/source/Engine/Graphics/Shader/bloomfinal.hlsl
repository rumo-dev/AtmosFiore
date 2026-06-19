// BLOOM
#include "fullscreen_quad.hlsli"
#include "bloom.hlsli"
#include "samplers.hlsli"

Texture2D texture_maps[2] : register(t0);


float4 main(VS_OUT pin) : SV_TARGET
{
    if (!is_enabled)
    {
        return texture_maps[0].Sample(sampler_states[POINT_CLAMP], pin.texcoord);
    }
    float4 color = texture_maps[0].Sample(sampler_states[POINT_CLAMP], pin.texcoord);
    float4 bloom = texture_maps[1].Sample(sampler_states[POINT_CLAMP], pin.texcoord);

    float3 fragment_color = color.rgb + bloom.rgb;
    float alpha = color.a;

    return float4(fragment_color, alpha);


}
