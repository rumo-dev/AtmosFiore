// // volumetric_fog_ps.hlsl (クリーンアップ版)
#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"
#include "shadow_directional.hlsli"
#include "point_light.hlsli"
#include "shadow_point.hlsli"
#include "spot_light.hlsli"
#include "shadow_spot.hlsli"

// ── テクスチャ ────────────────────────────────────────────────────
Texture2D<float4> gbuffer0 : register(t0);
Texture2D<float4> gbuffer1 : register(t1);
Texture2D<float4> gbuffer2 : register(t2);
Texture2D<float4> gbuffer3 : register(t3);
Texture2D<float4> scene_color : register(t4);
Texture2D<float> point_shadow_front_map : register(t5);
Texture2D<float> point_shadow_back_map : register(t6);
Texture2D<float> directional_shadow_map : register(t7);

// ── 定数バッファ (b8) ─────────────────────────────────────────────
cbuffer VolumetricFogCB : register(b8)
{
    int step_count; // レイマーチングステップ数
    float density_base; // 基底密度
    float scattering; // 散乱係数
    float absorption; // 吸収係数

    float anisotropy; // HG 位相関数 g 値
    float height_falloff; // (未使用にする場合は削除可能)
    float noise_scale; // FBM ノイズスケール
    float intensity; // 全体輝度乗数

    int is_enabled;
    float camera_far_distance;
    float pad[2];
};

// ── ノイズ / FBM ─────────────────────────────────────────────────
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

// ── Henyey-Greenstein 位相関数 ────────────────────────────────────
float HGPhase(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(max(denom, 0.0001), 1.5));
}

// ── メイン ────────────────────────────────────────────────────────
float4 main(VS_OUT pin) : SV_TARGET
{
    float4 scene = scene_color.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);

    if (!is_enabled)
        return scene;

    float4 gb2 = gbuffer2.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float3 pos_ws = gb2.xyz;
    float3 ray_start = camera_position.xyz;
    
    float3 temp_vec = pos_ws - ray_start;
    float3 ray_dir = normalize(temp_vec);

    // 背景（空）の判定：遠クリップ面までレイを延長
    if (gb2.w == 0.0)
    {
        pos_ws = ray_start + ray_dir * camera_far_distance;
    }

    float3 ray_end = pos_ws;
    float3 ray_vec = ray_end - ray_start;
    float ray_len = length(ray_vec);

    if (ray_len < 0.0001)
        return scene;

    float step_size = ray_len / (float) step_count;
    float3 step_vec = ray_dir * step_size;

    // ディザー (Interleaved Gradient Noise)
    float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
    float dither = frac(magic.z * frac(dot(pin.position.xy, magic.xy)));
    float3 cur_pos = ray_start + step_vec * dither;

    float3 L_dir = directional_light_direction.xyz;
    float3 L_col = directional_light_color.rgb * 5.0;

    float3 scattered = 0.0;
    float transmittance = 1.0;

    for (int i = 0; i < step_count; i++)
    {
        // 【変更】均一なライトシャフトにするためノイズを無効化したい場合は 1.0 にしてください
        float noise_val = saturate(lerp(0.4, 1.6, fbm(cur_pos * noise_scale)));
        
        // 【排除】不具合の原因になっていた高さ減衰（height_fog）を完全に削除
        float density = density_base * noise_val;

        float extinction = density * (scattering + absorption);
        float step_trans = exp(-extinction * step_size);

        float3 total_light = 0.0;

        // Directional Light (太陽・月などからのライトシャフト)
        {
            // 【修正】視線方向と「光がやってくる方向（-L_dir）」の内積を正しく計算
            float cos_theta = dot(ray_dir, -L_dir);
            float phase = HGPhase(cos_theta, anisotropy);
            
            float shadow = SampleDirectionalShadow(directional_shadow_map, cur_pos, float3(0, 0, 0), -L_dir);
            total_light += L_col * phase * shadow;
        }

        // Point Lights
        for (int p = 0; p < numPointLights; ++p)
        {
            if (pointLights[p].intensity <= 0 || pointLights[p].radius <= 0)
                continue;
            float3 lv = pointLights[p].position - cur_pos;
            float dist = length(lv);
            if (dist >= pointLights[p].radius)
                continue;

            float3 L = lv / max(dist, 0.0001);
            float att = pow(saturate(1.0 - dist / pointLights[p].radius), 2.0);
            float boost = min(1.0 / (dist + 0.1), 5.0);
            
            float cos_theta = dot(ray_dir, L);
            float phase = HGPhase(cos_theta, anisotropy);
            
            float shadow = SamplePointLightShadow(point_shadow_front_map, point_shadow_back_map, cur_pos, float3(0, 0, 0), L, p);
            total_light += pointLights[p].color * pointLights[p].intensity * (att * boost) * phase * shadow;
        }

        // Spot Lights
        for (int s = 0; s < numSpotLights; ++s)
        {
            if (spotLights[s].intensity <= 0 || spotLights[s].radius <= 0)
                continue;
            float3 lv = spotLights[s].position - cur_pos;
            float dist = length(lv);
            if (dist >= spotLights[s].radius)
                continue;

            float3 L = lv / max(dist, 0.0001);
            float3 spot_dir = normalize(spotLights[s].direction);
            float cos_a = dot(-L, spot_dir);
            float spot = smoothstep(cos(spotLights[s].outerAngle), cos(spotLights[s].innerAngle), cos_a);
            if (spot <= 0)
                continue;

            float dist_att = pow(saturate(1.0 - dist / spotLights[s].radius), 2.0);
            
            float cos_theta = dot(ray_dir, L);
            float phase = HGPhase(cos_theta, anisotropy);
            
            total_light += spotLights[s].color * spotLights[s].intensity * (spot * dist_att * 2.0) * phase;
        }

        // 積分処理
        float3 scat_contrib = total_light * density * scattering;
        float int_weight = (extinction > 0.0001) ? (1.0 - step_trans) / extinction : step_size;
        scattered += scat_contrib * transmittance * int_weight;
        transmittance *= step_trans;

        if (transmittance < 0.01)
            break;

        cur_pos += step_vec;
    }

    float3 Lo = scene.rgb * transmittance + scattered * intensity;
    return float4(Lo, scene.a);
}