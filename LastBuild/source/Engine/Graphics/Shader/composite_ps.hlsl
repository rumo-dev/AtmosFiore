// =============================================================
// composite_ps.hlsl — Pass 5: 最終合成
//
// blurred[near/far] は resolve で rgb=正規化済みカラー、a=blend率
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

    if (!is_dof)
        return original;

    // ---- Step 1: Far ボケ合成 ----
    // far_blurred.a = resolve後のCoC加重平均 [0,1] → blend率
    float4 far_blur = far_blurred.Sample(sampler_states[LINEAR_CLAMP], uv);
    float3 result = lerp(original.rgb, far_blur.rgb, far_blur.a);

    // ---- Step 2: Near ボケ合成（最前面）----
    // near_blurred.rgb = resolve済み正規化カラー
    // near_blurred.a   = CoC加重平均 [0,1] → blend率
    // coc_map は LINEAR で読んでブレンドを滑らかに
    float4 near_blur = near_blurred.Sample(sampler_states[LINEAR_CLAMP], uv);
    float near_coc = coc_map.Sample(sampler_states[LINEAR_CLAMP], uv).r;

    // near_blur.a（scatter由来の累積）と near_coc（CoC map由来）の
    // 大きい方を blend 率にする。
    // near_blur.a だけだとCoC境界でぱきっとする場合があるため
    // smoothstep で soften した near_coc を補助として使う。
    float near_blend = max(near_blur.a, smoothstep(0.0f, 0.3f, near_coc));

    result = lerp(result, near_blur.rgb, near_blend);

    return float4(result, 1.0f);
}
