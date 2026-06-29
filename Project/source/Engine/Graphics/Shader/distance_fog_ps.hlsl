// distance_fog_ps.hlsl (時間連動・動的3Dノイズ版)
// カメラからの距離に基づきリニアにフォグを適用する。

#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"

// ── テクスチャ ─────────────────────────────────────────────────────
Texture2D<float4> gbuffer0 : register(t0);
Texture2D<float4> gbuffer1 : register(t1);
Texture2D<float4> gbuffer2 : register(t2);
Texture2D<float4> gbuffer3 : register(t3);
Texture2D<float4> scene_color : register(t4);

// ── 定数バッファ (b0) ──────────────────────────────────────────────
cbuffer DistanceFogCB : register(b8)
{
    float fog_start; // フォグ開始距離
    float fog_end; // フォグ最大密度距離
    float density; // 最大時の密度 [0..1]
    float intensity; // 輝度乗数

    float3 fog_color; // フォグ色 (linear)
    int is_enabled;

    float padding[3];
};

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 scene = scene_color.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    if (!is_enabled)
        return scene;

    float3 pos_ws = gbuffer2.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz;
    float dist = length(pos_ws - camera_position.xyz);

    // リニア補間 [0..1]
    float t = saturate((dist - fog_start) / max(fog_end - fog_start, 0.0001));
    float fog_amount = t * density;

    float3 color = lerp(scene.rgb, fog_color * intensity, fog_amount);
    return float4(color, scene.a);
}
