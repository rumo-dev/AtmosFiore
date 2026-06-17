// =============================================================
// ffs_horizontal_ps.hlsl
// Pass 3: Fast Filter Spreading — 水平方向ブラー
//
// Input:
//   t0 = near_field または far_field（Pass 2 出力）
//        RGB = カラー（near は premult）,  A = CoC [0,1]
// Output:
//   水平方向にボケを適用した中間バッファ
//   A = 中心ピクセルの CoC（垂直パスのボケ幅計算に引き継ぐ）
//
// ■ 本来の FFS（GDC 2017 スライド準拠）:
//   Compute Shader で各ピクセルが CoC サイズの矩形四隅に
//   デルタ値をアトミック加算（Scatter）し、
//   その後 2D プレフィックスサムで積分する。
//   DX11 PS からは任意位置への書き込みが不可のため、
//   ここでは等価な Gather 近似を採用する。
//
// ■ Gather 近似の原理:
//   出力ピクセル (x,y) の値 =
//     各入力ピクセル i が x を CoC 範囲内に含む場合の
//     input[i] の加重平均
//   → 可変幅の Bartlett（テント）フィルタの水平適用と等価
//
// ■ Bartlett 重み:
//   w(t) = max(0, 1 - |t|)   t ∈ [-1, 1]
//   GDC スライドの 9 点デルタ（2 回積分）をサンプリング重みで近似する。
//
// 定数バッファ参照:
//   MaxBlurRadius （b1: CAMERA_CONSTANT_BUFFER）
//   inv_buffer_width, is_dof （b9: dof_local_constants）
// =============================================================
#include "dof_common.hlsli"

// b9: DoF ローカル定数
cbuffer dof_local_constants : register(b9)
{
    int is_dof;
    float inv_buffer_width;
    float inv_buffer_height;
    float local_padding;
};

Texture2D source_tex : register(t0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;

    float center_coc = source_tex.Sample(sampler_states[POINT_CLAMP], uv).a;

    // CoC がほぼゼロ（フォーカス内）なら早期リターン
    if (center_coc < 0.001f)
        return source_tex.Sample(sampler_states[POINT_CLAMP], uv);

    // CoC [0,1] → UV 空間のボケ幅
    float blur_radius_uv = center_coc * MaxBlurRadius * inv_buffer_width;

    float4 color_sum = float4(0, 0, 0, 0);
    float weight_sum = 0.0f;

    [unroll]
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        // t: [-0.5, 0.5] の均等サンプル位置
        float t = (float) i / (float) (SAMPLE_COUNT - 1) - 0.5f;
        float2 sample_uv = uv + float2(t * blur_radius_uv * 2.0f, 0.0f);

        float4 s = source_tex.Sample(sampler_states[LINEAR_CLAMP], sample_uv);
        float s_coc = s.a;

        // Bartlett（テント）重み: 中心ほど強く、端ほど弱い
        // GDC スライドの 2 回積分によるテントカーネルを近似
        float dist_norm = abs(t) * 2.0f; // [0, 1]
        float weight = max(0.0f, (1.0f - dist_norm) * s_coc);

        color_sum += s * weight;
        weight_sum += weight;
    }

    if (weight_sum > 0.001f)
        color_sum /= weight_sum;

    // A に中心 CoC を引き継ぐ（垂直パスのボケ幅計算に使用）
    color_sum.a = center_coc;

    return color_sum;
}
