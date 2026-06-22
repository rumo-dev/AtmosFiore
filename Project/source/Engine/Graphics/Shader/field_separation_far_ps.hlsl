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

Texture2D color_map : register(t0); //DXGI_FORMAT_R16G16_FLOAT
Texture2D coc_map : register(t1); //DXGI_FORMAT_R16G16B16A16_FLOAT
//書き出し先ー＞DXGI_FORMAT_R16G16B16A16_FLOAT

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = color_map.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord);
    // LINEAR_CLAMP: フル解像度 coc_map をワーク解像度にダウンサンプルする際に補間する
    float far_coc = coc_map.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord).g;
    // far も premult に変える
    return float4(color.rgb * far_coc, far_coc);
    
}

