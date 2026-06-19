// =============================================================
// dof_common.hlsli
// DoF 全シェーダ共通インクルード
// プロジェクトの constants.hlsli を使用する
// =============================================================
#ifndef DOF_COMMON_HLSLI
#define DOF_COMMON_HLSLI

#include "constants.hlsli"
#include "samplers.hlsli"

// ---- VS 出力（FullscreenQuad 共通） ----
struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

// ---- 定数 ----
static const int SAMPLE_COUNT = 7; // Gather 近似パスのサンプル数（未使用化）

// ---- ユーティリティ ----

// 非線形深度 → ビュー空間線形深度
float LinearizeDepth(float raw_depth)
{
    float depth = isReversed ? (1.0f - raw_depth) : raw_depth;
    
    float n = near_z; // カメラの Near プレーン設定値 (メートル)
    float f = far_z; // カメラの Far プレーン設定値 (メートル)
    
    // 標準的な遠近投影の線形化式
    return (n * f) / (f - depth * (f - n));
}

// 薄レンズモデルによる CoC 計算
// 戻り値: float2(NearCoC [0,1], FarCoC [0,1])
float2 ComputeCoC(float linear_depth_m)
{
    // 1. 符号の安全化: 深度の絶対値をとる（ビュー空間が負のZ軸の場合の対策）
    float depth_abs = abs(linear_depth_m);

    float d = max(depth_abs, 0.0001f) * 1000.0f; // mm
    float S = max(FocusDist, 0.0001f) * 1000.0f; // mm
    float f = FocalLength; // mm
    float A = f / max(FNumber, 0.001f); // mm

    // 2. 分母の安全化
    float denominator = d * max(abs(S - f), 0.001f);
    
    // 3. ピクセルサイズ計算の修正
    // ※ MaxBlurRadius基準から、画面解像度（横幅）基準に変更
    // ※ もし ScreenWidth が定数バッファにない場合は、暫定で 1920.0f などの固定値でテストしてください
    float mm_per_pixel = SensorSize / max(1280, 1.0f);
    float coc_px = (A * f * abs(d - S)) / denominator / max(mm_per_pixel, 0.0001f);

    // 4. 手前と奥の判定を「絶対値の比較」に明示変更
    // 深度がピント位置より手前（カメラに近い）ならNear、奥ならFar
    float signed_coc = (depth_abs < FocusDist) ? -coc_px : coc_px;
    
    // クランプ処理
    signed_coc = clamp(signed_coc, -MaxBlurRadius, MaxBlurRadius);

    // [0, 1] に正規化
    float near_coc = saturate(-signed_coc / max(MaxBlurRadius, 1.0f));
    float far_coc = saturate(signed_coc / max(MaxBlurRadius, 1.0f));

    return float2(near_coc, far_coc);
}

#endif // DOF_COMMON_HLSLI
