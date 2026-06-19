// =============================================================
// ffs_vertical_ps.hlsl
// Pass 4: Fast Filter Spreading — 垂直方向ブラー（ボケ完成）
//
// Input:
//   t0 = ffs_horizontal 出力（水平積分済み near or far）
//        RGB = 水平ボケ済みカラー（near は premult）,  A = CoC
// Output:
//   縦方向積分を適用した最終ボケ画像
//   A = 重み付き平均 CoC（composite_ps の blend 比率に使用）
//
// 水平パス（ffs_horizontal_ps）と同一アルゴリズムを
// 垂直方向に適用する。
// 水平 × 垂直の 2 パス積分により、
// GDC スライドの Bartlett（テント）カーネルが完成する。
//
// 定数バッファ参照:
//   MaxBlurRadius （b1: CAMERA_CONSTANT_BUFFER）
//   inv_buffer_height, is_dof （b9: dof_local_constants）
// =============================================================
#include "dof_common.hlsli"

cbuffer dof_local_constants : register(b9)
{
    int is_dof;
    float inv_buffer_width;
    float inv_buffer_height;
    float local_padding;
};

Texture2D h_blurred_tex : register(t0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;

    float center_coc = h_blurred_tex.Sample(sampler_states[POINT_CLAMP], uv).a;

    if (center_coc < 0.001f)
        return h_blurred_tex.Sample(sampler_states[POINT_CLAMP], uv);

    float blur_radius_uv = center_coc * MaxBlurRadius * inv_buffer_height;

    float4 color_sum = float4(0, 0, 0, 0);
    float weight_sum = 0.0f;
    float coc_sum = 0.0f;

    [unroll]
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        float t = (float) i / (float) (SAMPLE_COUNT - 1) - 0.5f;
        float2 sample_uv = uv + float2(0.0f, t * blur_radius_uv * 2.0f);

        float4 s = h_blurred_tex.Sample(sampler_states[LINEAR_CLAMP], sample_uv);
        float s_coc = s.a;

        float dist_norm = abs(t) * 2.0f;
        float weight = max(0.0f, (1.0f - dist_norm) * s_coc);

        color_sum += s * weight;
        weight_sum += weight;
        coc_sum += s_coc * weight;
    }

    if (weight_sum > 0.001f)
    {
        color_sum /= weight_sum;
        coc_sum /= weight_sum;
    }

    // A に重み付き平均 CoC を格納（composite_ps の blend 比率に使用）
    color_sum.a = coc_sum;

    return color_sum;
}
