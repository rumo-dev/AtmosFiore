// exponential_fog_ps.hlsl (時間連動・動的3Dノイズ版)
// カメラ距離に対して指数的にフォグを適用する。
// Mode 0: exp  / Mode 1: exp^2

#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"

// ── テクスチャ ─────────────────────────────────────────────────────
Texture2D<float4> gbuffer0 : register(t0);
Texture2D<float4> gbuffer1 : register(t1);
Texture2D<float4> gbuffer2 : register(t2);
Texture2D<float4> gbuffer3 : register(t3);
Texture2D<float4> scene_color : register(t4);

cbuffer ExponentialFogCB : register(b8)
{
    float density; // 密度係数
    float intensity; // 輝度乗数
    int mode; // 0=exp, 1=exp2
    int is_enabled;

    float3 fog_color; // フォグ色 (linear)
    float padding;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 scene = scene_color.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    if (!is_enabled)
        return scene;

    float3 pos_ws = gbuffer2.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz;
    float dist = length(pos_ws - camera_position.xyz);

    float fog_amount;
    if (mode == 0)
    {
        // Exponential: fog = 1 - exp(-density * d)
        fog_amount = 1.0 - exp(-density * dist);
    }
    else
    {
        // Exponential2: fog = 1 - exp(-(density * d)^2)
        float x = density * dist;
        fog_amount = 1.0 - exp(-x * x);
    }
    fog_amount = saturate(fog_amount);

    float3 color = lerp(scene.rgb, fog_color * intensity, fog_amount);
    return float4(color, scene.a);
}
