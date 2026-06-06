// adaptation_ps.hlsl
#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"

// t0: シーンのHDRカラー（ブルーム加算済み）
// t1: CSによって更新された、現在の画面の露出値テクスチャ(1x1)
Texture2D<float4> scene_texture : register(t0);
Texture2D<float4> exposure_texture : register(t1);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 raw_color = scene_texture.Sample(sampler_states[POINT_CLAMP], pin.texcoord);
    //return raw_color;
    float3 fragment_color = raw_color.rgb;
    float alpha = raw_color.a;

    // 1x1 のテクスチャ中央から現在の露出値をサンプリング
    float exposure = exposure_texture.Sample(sampler_states[POINT_CLAMP], float2(0.5f, 0.5f)).r;
    
    // 露出を適用して、人間の目が順応した状態の明るさに調整
    fragment_color *= exposure;

    // 露出調整済みの「HDRカラー」として出力
    return float4(fragment_color, alpha);
}