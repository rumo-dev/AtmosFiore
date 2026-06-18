// =============================================================
// dof_common.hlsli
// DoF 全シェーダ共通インクルード
// プロジェクトの constants.hlsli を使用する
// =============================================================
#ifndef DOF_COMMON_HLSLI
#define DOF_COMMON_HLSLI

#include "constants.hlsli"
#include "samplers.hlsli"

// ---- サンプラー ----

// ---- VS 出力（FullscreenQuad 共通） ----
struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

// ---- 定数 ----
static const int SAMPLE_COUNT = 7; // 水平/垂直パスのサンプル数

// ---- ユーティリティ ----

// 非線形深度 → ビュー空間線形深度
// inv_projection を使って正確に逆変換する
float LinearizeDepth(float raw_depth)
{
    // Reversed-Z の場合は深度を反転
    float depth = isReversed ? (1.0f - raw_depth) : raw_depth;

    // クリップ空間の座標を復元
    float4 clip = float4(0.0f, 0.0f, depth, 1.0f);

    // ビュー空間へ逆変換
    float4 view_pos = mul(clip, inv_projection);

    return view_pos.z / view_pos.w; // 線形深度（正値）
}


// 薄レンズモデルによる CoC 計算
// 参考: GDC 2017「Cinematic Depth of Field」- Hillesland & Skelton (AMD)
//
//   CoC = (A * f * |d - S|) / (d * (S - f))
//   A = f / FNumber  （絞り径、mm）
//   f = FocalLength  （焦点距離、mm）
//   S = FocusDist    （フォーカス距離、m → mm 換算）
//   d = 線形深度     （m → mm 換算）
//
// 戻り値: float2(NearCoC [0,1], FarCoC [0,1])
float2 ComputeCoC(float linear_depth_m)
{
    float d = linear_depth_m * 1000.0f;
    float S = FocusDist * 1000.0f;
    float f = FocalLength;
    float A = f / max(FNumber, 0.001f);

    float denominator = d * max(S - f, 0.001f);

    // 【修正箇所】MaxBlurRadius ではなく、画面全体の横幅ピクセル数（例: 1920）で割る
    float screen_width = 1280.0f; // ★環境に合わせて定数やCBの値に差し替えてください
    float mm_per_pixel = SensorSize / screen_width;

    // 物理的なボケ幅（ピクセル数）を計算
    float coc_px = (A * f * abs(d - S)) / denominator / mm_per_pixel;

    // 符号で Near / Far を判定
    float signed_coc = (d < S) ? -coc_px : coc_px;
    
    // 最大ボケ幅（ピクセル数）でクランプする
    signed_coc = clamp(signed_coc, -MaxBlurRadius, MaxBlurRadius);

    // [0, 1] の範囲に正規化して出力
    float near_coc = saturate(-signed_coc / MaxBlurRadius);
    float far_coc = saturate(signed_coc / MaxBlurRadius);

    return float2(near_coc, far_coc);
}

#endif // DOF_COMMON_HLSLI
