// volumetric_fog_ps.hlsl
#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"

// ── テクスチャ ────────────────────────────────────────────────────
Texture2D<float4> gbuffer2 : register(t2);
Texture2D<float4> scene_color : register(t4);

// 3D累積フォグテクスチャ
Texture3D<float4> accumulated_volume_texture : register(t10);

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

// ── メイン ────────────────────────────────────────────────────────
float4 main(VS_OUT pin) : SV_TARGET
{
    float4 scene = scene_color.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);

    if (!is_enabled)
        return scene;

    // GBuffer からワールド座標を復元
    float4 gb2 = gbuffer2.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float3 pos_ws = gb2.xyz;

    // 背景（空など、深度バッファに書き込まれていないピクセル）の場合
    if (gb2.w == 0.0)
    {
        float3 ray_dir = normalize(pos_ws - camera_position.xyz);
        pos_ws = camera_position.xyz + ray_dir * fog_far;
    }

    // ビュー空間に変換してデプスを取得
    float4 pos_vs = mul(float4(pos_ws, 1.0), view);
    float z_view = pos_vs.z;

    // ビュー空間デプスから3DテクスチャのZスライス（W座標）へのマッピングを逆算 (二次関数の逆算)
    float w = saturate(sqrt(max(z_view - fog_near, 0.0) / max(fog_far - fog_near, 0.0001)));

    // 階層感（バンディング）を完全に消し去るため、ディザリングの強度を 1.0 に引き上げ
    float dither = frac(52.9829189 * frac(dot(pin.position.xy, float2(0.06711056, 0.00583715))));
    w = saturate(w + (dither - 0.5) * 1.0 / (float)grid_depth);

    // 3D累積ボリュームフォグテクスチャをサンプリング
    float4 fog_sample = accumulated_volume_texture.SampleLevel(sampler_states[LINEAR_CLAMP], float3(pin.texcoord, w), 0);

    // ブレンド処理
    float3 Lo = scene.rgb * fog_sample.a + fog_sample.rgb * intensity;

    return float4(Lo, scene.a);
}