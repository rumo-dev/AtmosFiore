// volumetric_fog_injection_cs.hlsl
#include "samplers.hlsli"
#include "constants.hlsli"
#include "shadow_directional.hlsli"
#include "point_light.hlsli"
#include "shadow_point.hlsli"
#include "spot_light.hlsli"
#include "shadow_spot.hlsli"

// ── テクスチャ ────────────────────────────────────────────────────
Texture2D<float> point_shadow_front_map : register(t5);
Texture2D<float> point_shadow_back_map : register(t6);
Texture2D<float> directional_shadow_map : register(t7);

// ── UAV ───────────────────────────────────────────────────────────
RWTexture3D<float4> g_volume_light_uav : register(u0);

// ── 定数バッファ (b8) ─────────────────────────────────────────────
cbuffer VolumetricFogCB : register(b8)
{
    int grid_width;
    int grid_height;
    int grid_depth;
    int pad_grid;

    float density_base;
    float scattering;
    float absorption;
    float anisotropy;

    float noise_scale;
    float intensity;
    float fog_near;
    float fog_far;

    int is_enabled;
    float time;
    float pad_cb[2];
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

[numthreads(8, 8, 4)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= (uint)grid_width || DTid.y >= (uint)grid_height || DTid.z >= (uint)grid_depth)
        return;

    // 1. 正規化グリッド座標 (0 ~ 1) の算出 (セルの中心点)
    float3 uvw = (float3(DTid) + 0.5) / float3(grid_width, grid_height, grid_depth);

    // 2. 二次関数マッピングによるビュー空間深度の算出 (中距離の解像度を確保)
    float z_view = fog_near + (fog_far - fog_near) * pow(uvw.z, 2.0);

    // 3. ビュー空間の XY 座標を復元
    float ndc_x = uvw.x * 2.0 - 1.0;
    float ndc_y = 1.0 - uvw.y * 2.0;
    
    float3 pos_vs;
    pos_vs.z = z_view;
    pos_vs.x = ndc_x * z_view / projection[0][0];
    pos_vs.y = ndc_y * z_view / projection[1][1];

    // 4. ワールド空間座標を算出
    float4 pos_ws = mul(float4(pos_vs, 1.0), inv_view);
    float3 cur_pos = pos_ws.xyz / pos_ws.w;

    // 5. 密度の評価 (noise_scale > 0.0 の場合のみ FBM ノイズを適用)
    float density = density_base;
    if (noise_scale > 0.0)
    {
        float3 noise_pos = cur_pos * noise_scale + float3(0.0, -time * 0.05, 0.0);
        float noise_val = saturate(lerp(0.4, 1.6, fbm(noise_pos)));
        density *= noise_val;
    }

    // カメラからボクセルへの視線方向
    float3 ray_dir = normalize(cur_pos - camera_position.xyz);

    float3 total_light = 0.0;

    // Directional Light
    {
        float3 L_dir = directional_light_direction.xyz;
        float3 L_col = directional_light_color.rgb * 5.0;
        
        float cos_theta = dot(ray_dir, -L_dir);
        float phase = HGPhase(cos_theta, anisotropy);
        
        float shadow = SampleDirectionalShadow(directional_shadow_map, cur_pos, float3(0, 0, 0), -L_dir);
        total_light += L_col * phase * shadow;
    }

    // Point Lights
    for (uint p = 0; p < numPointLights; ++p)
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
    for (uint s = 0; s < numSpotLights; ++s)
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

    // メディア特性の評価
    float3 scattering_coef = total_light * density * scattering;
    float extinction_coef = density * (scattering + absorption);

    g_volume_light_uav[DTid] = float4(scattering_coef, extinction_coef);
}
