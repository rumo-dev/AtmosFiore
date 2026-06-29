// height_fog_ps.hlsl (時間連動・動的3Dノイズ版)

#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"

// ── テクスチャ ─────────────────────────────────────────────────────
Texture2D<float4> gbuffer0 : register(t0);
Texture2D<float4> gbuffer1 : register(t1);
Texture2D<float4> gbuffer2 : register(t2);
Texture2D<float4> gbuffer3 : register(t3);
Texture2D<float4> scene_color : register(t4);

// ── 定数バッファ (b8) ──────────────────────────────────────────────
cbuffer HeightFogCB : register(b8)
{
    float base_height;
    float falloff;
    float density_max;
    float intensity;

    float3 fog_color;
    int is_enabled;

    // ── ノイズ＆アニメーション用パラメータ ──
    float noise_scale; // ノイズの細かさ (例: 0.1)
    float noise_strength; // ノイズの影響度 (例: 0.5)
    float time; // C++から毎フレーム送られる時間（秒）
    float padding1;

    float3 wind_velocity; // 風の方向と速さ (例: 1.0, 0.0, 0.5)
    float padding2;
};

// ── ノイズ / FBM ───────────────────────────────────────────────────
float hash(float3 p)
{
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise3D(float3 x)
{
    float3 i = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(
        lerp(lerp(hash(i + float3(0, 0, 0)), hash(i + float3(1, 0, 0)), f.x),
             lerp(hash(i + float3(0, 1, 0)), hash(i + float3(1, 1, 0)), f.x), f.y),
        lerp(lerp(hash(i + float3(0, 0, 1)), hash(i + float3(1, 0, 1)), f.x),
             lerp(hash(i + float3(0, 1, 1)), hash(i + float3(1, 1, 1)), f.x), f.y),
        f.z);
}

float fbm(float3 p)
{
    float f = 0.0;
    float amp = 0.5;
    [unroll(3)]
    for (int i = 0; i < 3; i++)
    {
        f += amp * noise3D(p);
        p *= 2.02;
        amp *= 0.5;
    }
    return f;
}

// ── メイン ────────────────────────────────────────────────────────
float4 main(VS_OUT pin) : SV_TARGET
{
    float4 scene = scene_color.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    if (!is_enabled)
        return scene;

    float3 pos_ws = gbuffer2.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz;

    // 1. 基本の高さ方向の指数密度を計算
    float height_above = max(pos_ws.y - base_height, 0.0);
    float base_fog_amount = density_max * exp(-falloff * height_above);

    // 2. 動的3Dノイズの計算
    // 風の速度と時間を掛けてスクロールオフセットを作り、ワールド座標から引くことで空間全体を移動させます
    float3 scroll_offset = wind_velocity * time;
    float3 noise_coord = (pos_ws - scroll_offset) * noise_scale;
    
    float noise_val = fbm(noise_coord);

    // 3. ノイズをフォグ密度に適用
    // noise_strength を使って、モヤの濃淡のコントラストを調整します
    float noise_modifier = lerp(1.0, noise_val * 2.0, noise_strength);
   

    float3 view_dir = pos_ws - camera_position.xyz;
    float dist = length(view_dir);

// 距離に基づいたフォグの減衰 (例: 指数的な距離フォグ)
    float dist_fog_amount = 1.0 - exp(-0.01 * dist); // 0.01 はフォグの伸び具合

// 最終的なフォグ濃度 = (高さの密度) * (距離の密度)
    float final_fog_amount = saturate(base_fog_amount * noise_modifier * dist_fog_amount);

// 合成
    float3 color = lerp(scene.rgb, fog_color * intensity, final_fog_amount);
    return float4(color, scene.a);
}