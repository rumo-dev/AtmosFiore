// =============================================================
// composite_ps.hlsl
// Pass 5: 最終合成
//
// Input:
//   t0 = color_map      オリジナル HDR カラー
//   t1 = coc_map        R=NearCoC, G=FarCoC（Pass 1 出力）
//   t2 = near_blurred   Near ボケ済み（Premultiplied Alpha）
//   t3 = far_blurred    Far ボケ済み（A = 累積 CoC）
//
// 合成順序（前面→背面）:
//   Near ボケ > シャープ > Far ボケ
//
//   Step 1. Far 合成:
//     result = lerp(original, far_blurred.rgb, far_blurred.a)
//
//   Step 2. Near 合成（最前面に重ねる）:
//     near_color = near_blurred.rgb / near_blurred.a  （premult 除算）
//     result     = lerp(result, near_color, near_coc)
//
// 定数バッファ参照:
//   is_dof （b9: dof_local_constants）
// =============================================================
#include "dof_common.hlsli"

cbuffer dof_local_constants : register(b9)
{
    int is_dof;
    float inv_buffer_width;
    float inv_buffer_height;
    float local_padding;
};

Texture2D color_map : register(t0);
Texture2D coc_map : register(t1);
Texture2D near_blurred : register(t2);
Texture2D far_blurred : register(t3);

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;

    float4 original = color_map.Sample(sampler_states[LINEAR_CLAMP], uv);

    // DoF 無効時はオリジナルをそのまま返す
    if (!is_dof)
        return original;

    float near_coc = coc_map.Sample(sampler_states[POINT_CLAMP], uv).r;

    // ---- Step 1: Far ボケ合成 ----
    // far_blurred.a = 累積 far CoC → blend 比率として使用
    float4 far_blur = far_blurred.Sample(sampler_states[LINEAR_CLAMP], uv);
    float far_blend = saturate(far_blur.a);
    float3 result = lerp(original.rgb, far_blur.rgb, far_blend);

    // ---- Step 2: Near ボケ合成（最前面）----
    // Premultiplied Alpha を除算して正規化（重み付き平均の復元）
    float4 near_blur = near_blurred.Sample(sampler_states[LINEAR_CLAMP], uv);
    float3 near_color = (near_blur.a > 0.001f)
                      ? near_blur.rgb / near_blur.a
                      : original.rgb;

    result = lerp(result, near_color, saturate(near_coc));

    return float4(result, 1.0f);
}
