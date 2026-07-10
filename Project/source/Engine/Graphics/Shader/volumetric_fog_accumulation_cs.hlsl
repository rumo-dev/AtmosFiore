// volumetric_fog_accumulation_cs.hlsl
#include "constants.hlsli"

// ── SRV / UAV ─────────────────────────────────────────────────────
Texture3D<float4>   g_volume_light : register(t0);
RWTexture3D<float4> g_volume_accumulated_uav : register(u0);

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

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= (uint)grid_width || DTid.y >= (uint)grid_height)
        return;

    // UVWのXY座標を正規化 (-1 ~ 1)
    float ndc_x = ((float)DTid.x + 0.5) / (float)grid_width * 2.0 - 1.0;
    float ndc_y = 1.0 - ((float)DTid.y + 0.5) / (float)grid_height * 2.0;

    // 視錐台の列に対応するビュー空間レイ方向の比率を計算 (Z=1 と仮定)
    float3 view_ray = float3(ndc_x / projection[0][0], ndc_y / projection[1][1], 1.0);
    float ray_length_scale = length(view_ray);

    float3 accumulated_light = 0.0;
    float accumulated_transmittance = 1.0;

    // 手前から奥（Zスライス）に向かって積分処理を実行
    for (int z = 0; z < grid_depth; ++z)
    {
        uint3 pos = uint3(DTid.x, DTid.y, z);

        // 二次関数マッピングにおけるスライスの前後境界デプスを計算
        float z_prev = (z == 0) ? fog_near : fog_near + (fog_far - fog_near) * pow((float)z / (float)grid_depth, 2.0);
        float z_next = fog_near + (fog_far - fog_near) * pow((float)(z + 1) / (float)grid_depth, 2.0);

        // 実際のレイに沿ったステップサイズ
        float step_length = (z_next - z_prev) * ray_length_scale;

        // 注入されたメディア情報を取得
        float4 injected = g_volume_light[pos];
        float3 scattering_coef = injected.rgb;
        float extinction = injected.a;

        // 透過率を計算
        float step_trans = exp(-extinction * step_length);

        // 散乱光の積分
        float3 step_scat = scattering_coef * ((extinction > 0.0001) ? (1.0 - step_trans) / extinction : step_length);

        // 累積
        accumulated_light += step_scat * accumulated_transmittance;
        accumulated_transmittance *= step_trans;

        // 累積結果を書き込み (RGB: 散乱光, A: 透過率)
        g_volume_accumulated_uav[pos] = float4(accumulated_light, accumulated_transmittance);
    }
}
