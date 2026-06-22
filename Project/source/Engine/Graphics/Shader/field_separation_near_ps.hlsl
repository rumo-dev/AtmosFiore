// =============================================================
// field_separation_near_ps.hlsl
// Pass 2a: Near フィールド分離
//
// Input:
//   t0 = color_map  （HDR カラー）
//   t1 = coc_map    （R=NearCoC, G=FarCoC [0,1]）
// Output:
//   RGB = color.rgb * near_coc  （Premultiplied Alpha）
//   A   = near_coc
//
// Premultiplied Alpha で蓄積する理由:
//   前景ボケは背景に「滲み出す」現象を再現するため、
//   ボケ後に A（蓄積 CoC）で除算して正規化することで
//   正しい重み付き平均色が得られる。
// =============================================================
#include "dof_common.hlsli"

Texture2D color_map : register(t0);
Texture2D coc_map : register(t1);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = color_map.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord);
    // LINEAR_CLAMP: coc_map はフル解像度、このパスはワーク解像度(half-res)で動くため
    // POINT_CLAMP だと補間なしでダウンサンプルされ CoC がぱきっと飛ぶ
    float near_coc = coc_map.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord).r;

    // Premultiplied Alpha: rgb * near_coc で蓄積し、resolve で alpha(累積CoC合計)で除算
    // これにより scatter→prefix sum→resolve の後に正しい加重平均色が得られる
    return float4(color.rgb * near_coc, near_coc);
}
