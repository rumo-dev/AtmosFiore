// =============================================================
// dof_common.hlsli
// DoF 全シェーダ共通インクルード
// =============================================================
#ifndef DOF_COMMON_HLSLI
#define DOF_COMMON_HLSLI

#include "constants.hlsli"
#include "samplers.hlsli"

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

static const int SAMPLE_COUNT = 7;

float LinearizeDepth(float raw_depth)
{
    float depth = isReversed ? (1.0f - raw_depth) : raw_depth;
    float n = near_z;
    float f = far_z;
    return (n * f) / (f - depth * (f - n));
}

// 薄レンズモデルによる CoC 計算
// 戻り値: float2(NearCoC [0,1], FarCoC [0,1])
float2 ComputeCoC(float linear_depth_m)
{
    float depth_abs = abs(linear_depth_m);

    float d = max(depth_abs, 0.0001f) * 1000.0f; // mm
    float S = max(FocusDist, 0.0001f) * 1000.0f; // mm
    float f = FocalLength; // mm
    float A = f / max(FNumber, 0.001f); // mm

    float denominator = d * max(abs(S - f), 0.001f);
    float mm_per_pixel = SensorSize / max(1280, 1.0f);
    float coc_px = (A * f * abs(d - S)) / denominator / max(mm_per_pixel, 0.0001f);

    // ── ここが修正箇所 ──────────────────────────────────────────
    //
    // 旧コード:
    //   float signed_coc = (depth_abs < FocusDist) ? -coc_px : coc_px;
    //
    // 問題: ピント面を境にバイナリで near/far を切り替えていた。
    //   coc_px 自体はピント面で 0 になるが、符号判定が 2 値なので
    //   ピント面ぎりぎりのピクセルが near か far か飛び飛びに決まり
    //   「ぱきっと切り替わる」原因になっていた。
    //
    // 修正: near_coc / far_coc を独立して計算し、
    //   ピント面付近（フォーカスゾーン幅 = FocusDist * focus_zone_ratio）
    //   で smoothstep によりゼロに向けてフェードさせる。
    //   これにより near↔sharp↔far の遷移が滑らかになる。

    // ピント面からの距離（メートル）
    float dist_from_focus = depth_abs - FocusDist; // 負=手前, 正=奥

    // フォーカスゾーン幅: ピント距離の ±5% を「ボケ 0」の安全帯とする
    // 値を大きくするほどピント面周辺のボケ立ち上がりが緩やかになる
    float focus_zone = max(FocusDist * 0.05f, 0.05f); // m

    // Near: ピント面より手前 (dist_from_focus < 0)
    //   フォーカスゾーン内は smoothstep で 0 にフェード
    float raw_near = saturate(coc_px / max(MaxBlurRadius, 1.0f));
    float near_fade = smoothstep(0.0f, focus_zone, -dist_from_focus);
    float near_coc = raw_near * near_fade;

    // Far: ピント面より奥 (dist_from_focus > 0)
    float raw_far = saturate(coc_px / max(MaxBlurRadius, 1.0f));
    float far_fade = smoothstep(0.0f, focus_zone, dist_from_focus);
    float far_coc = raw_far * far_fade;

    return float2(near_coc, far_coc);
}

#endif // DOF_COMMON_HLSLI
