// =============================================================
// field_separation_far_ps.hlsl
// Pass 2b: Far フィールド分離
//
// Input:
//   t0 = color_map  （HDR カラー）
//   t1 = coc_map    （R=NearCoC, G=FarCoC [0,1]）
// Output:
//   RGB = color.rgb
//   A   = far_coc
//
// A（far_coc）は composite_ps で blend 比率として使用する。
// =============================================================
#include "dof_common.hlsli"

Texture2D color_map : register(t0);
Texture2D coc_map : register(t1);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = color_map.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord);
    float far_coc = coc_map.Sample(sampler_states[POINT_CLAMP], pin.texcoord).g;

    // far_coc が 0 より大きければ 1.0、そうでなければ 0.0
    float alpha = (far_coc > 0.0f) ? 1.0f : 0.0f;

    return float4(color.rgb, 1);
}