// =============================================================
// coc_generation_ps.hlsl
// Pass 1: 深度バッファ → CoC マップ生成
//
// Input:
//   t0 = color_map  （このパスでは未使用）
//   t1 = depth_map  （非線形深度 [0,1]）
// Output:
//   R = Near CoC [0,1]  (0=フォーカス内, 1=最大ボケ)
//   G = Far  CoC [0,1]
//
// CoC 計算は dof_common.hlsli の ComputeCoC() を使用。
// 薄レンズモデルのパラメータは CAMERA_CONSTANT_BUFFER (b1) から参照:
//   FNumber, FocalLength, SensorSize, FocusDist, MaxBlurRadius
//   inv_projection（深度の線形化に使用）
// =============================================================
#include "dof_common.hlsli"

Texture2D depth_map : register(t1);

float4 main(VS_OUT pin) : SV_TARGET
{
//   if (pin.texcoord.x < 1.0f / 3.0f)
//   {
//       // 【左側 1/3】 R（coc.r）のみを赤色として出力
//       return float4(1, 0.0f, 0.0f, 1.0f);
//   }
//   else if (pin.texcoord.x < 2.0f / 3.0f)
//   {
//       // 【中央 1/3】 G（coc.g）のみを緑色として出力
//       return float4(0.0f, 1, 0.0f, 1.0f);
//   }
//   else
//   {
//       // 【右側 1/3】 黒色を出力
//       return float4(0.0f, 0.0f, 0.0f, 1.0f);
//   }
    float raw_depth = depth_map.Sample(sampler_states[POINT_CLAMP], pin.texcoord).r;
    
    // 1. 深度の線形化
    float lin_depth = LinearizeDepth(raw_depth);
    
    // 2. CoCの計算
   
    float2 coc = ComputeCoC(lin_depth);

    // ========================================================
    // 🔍 デバッグ表示切り替え（検証したいモードのコメントアウトを解除）
    // ========================================================
    
    // モードA: 画面全体にCoCをそのまま出力 (理想：手前が赤、ピントが黒、奥が緑)
    return float4(coc.r, coc.g, 0.0f, 1.0f);

    // モードB: 3分割で「線形深度」「Near CoC」「Far CoC」を同時に確認
    ///*
    if (pin.texcoord.x < 1.0f / 3.0f)
    {
        // 【左側 1/3】線形深度（50mで白になるようにスケーリング）
        float v = saturate(lin_depth / 50.0f);
        return float4(v, v, v, 1.0f);
    }
    else if (pin.texcoord.x < 2.0f / 3.0f)
    {
        // 【中央 1/3】Near CoC（手前のボケマスク：赤）
        return float4(coc.r, 0.0f, 0.0f, 1.0f);
    }
    else
    {
        // 【右側 1/3】Far CoC（奥のボケマスク：緑）
        return float4(0.0f, coc.g, 0.0f, 1.0f);
    }
    //*/

    
}
