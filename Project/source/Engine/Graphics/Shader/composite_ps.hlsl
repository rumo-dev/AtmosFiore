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
// ── デバッグ用 ────────────────────────────────────────────────
// 0 = 通常合成
// 1 = near_blurred.rgb をそのまま表示（黒ならresolve以前の問題）
// 2 = near_blurred.a   をそのまま表示（グレースケール。0=黒=カバレッジ無し）
// 3 = far_blurred.rgb  をそのまま表示
// 4 = far_blurred.a    をそのまま表示
// 5 = coc_map.r (near_coc) をそのまま表示
// 6 = coc_map.g (far_coc)  をそのまま表示
#define DOF_DEBUG_VIEW 0
float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;

    float4 original = color_map.Sample(sampler_states[LINEAR_CLAMP], uv);

    if (!is_dof)
        return original;

#if DOF_DEBUG_VIEW != 0
    {
        float4 near_blur_dbg = near_blurred.Sample(sampler_states[LINEAR_CLAMP], uv);
        float4 far_blur_dbg = far_blurred.Sample(sampler_states[LINEAR_CLAMP], uv);
        float4 coc_dbg = coc_map.Sample(sampler_states[LINEAR_CLAMP], uv);

#if DOF_DEBUG_VIEW == 1
        return float4(near_blur_dbg.rgb, 1.0f);
#elif DOF_DEBUG_VIEW == 2
        return float4(near_blur_dbg.aaa, 1.0f);
#elif DOF_DEBUG_VIEW == 3
            return float4(far_blur_dbg.rgb, 1.0f);
#elif DOF_DEBUG_VIEW == 4
            return float4(far_blur_dbg.aaa, 1.0f);
#elif DOF_DEBUG_VIEW == 5
            return float4(coc_dbg.rrr, 1.0f);
#elif DOF_DEBUG_VIEW == 6
            return float4(coc_dbg.ggg, 1.0f);
#endif
    }
#endif
    // ---- Step 1: Far ボケ合成 ----
    // far_blurred.a = resolve後のCoC加重平均 [0,1] → blend率
    float4 far_blur = far_blurred.Sample(sampler_states[LINEAR_CLAMP], uv);
    float3 result = lerp(original.rgb, far_blur.rgb, far_blur.a);

    // ---- Step 2: Near ボケ合成（最前面）----
    // near_blurred.rgb = resolve済み正規化カラー
    // near_blurred.a   = CoC加重平均 [0,1] → blend率
    // coc_map は LINEAR で読んでブレンドを滑らかに
    float4 near_blur = near_blurred.Sample(sampler_states[LINEAR_CLAMP], uv);

    result = lerp(result, near_blur.rgb, near_blur.a);

    return float4(result, 1.0f);
}
