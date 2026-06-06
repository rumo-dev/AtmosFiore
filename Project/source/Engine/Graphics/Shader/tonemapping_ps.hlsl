// tonemapping_ps.hlsl
#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "tonemapping_algorithms.hlsli"

// --- メイン処理 ---

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 hdr_color = adjusted_hdr_texture.Sample(sampler_states[POINT_CLAMP], pin.texcoord);
    if (!is_enabled)
        return hdr_color;

    float3 color = hdr_color.rgb;

    color = ApplyToneMapping(color);

    // ガンマ補正
    color = pow(color, 1.0 / gamma);

    return float4(color, hdr_color.a);
}

