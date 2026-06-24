// vignetting_ps.hlsl
//
// 周辺減光（Vignetting）ポストエフェクト
//
// レンズの cos^4 則 および 入射角制限による四隅の光量減衰を再現する。
// 2 モードを組み合わせて使用：
//   1. 物理モデル（physical_link == 1）
//      Camera_Constants (b1) の FNumber / FocalLength から減光量を算出。
//      開放絞り（小さい F値）・広角（短焦点）ほど強い減光。
//
//   2. アーティストカーブ（physical_link == 0）
//      inner_radius / outer_radius / smoothness を使ったグラデーション。
//
// 両モードで intensity が最終乗数として掛かる。
//
// レジスタ:
//   b1  : CAMERA_CONSTANT_BUFFER  (constants.hlsli)
//   b9  : VignettingConstants
//   t0  : 入力 HDR カラーテクスチャ
//   s4  : LINEAR_CLAMP サンプラー

#include "constants.hlsli"
#include "samplers.hlsli"

// ---- 定数バッファ ----
cbuffer VignettingConstants : register(b9)
{
    float intensity;        // 全体強度 [0..3]
    float inner_radius;     // 減光開始半径（正規化 UV 空間）
    float outer_radius;     // 最大減光半径（正規化 UV 空間）
    float smoothness;       // 指数

    int   is_enabled;       // 有効フラグ
    int   physical_link;    // 物理連動フラグ
    float vig_pad0;
    float vig_pad1;
};

// ---- テクスチャ ----
Texture2D color_map : register(t0);

// ---- VS 出力 / PS 入力 ----
struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

// ---------------------------------------------------------------------------
// 物理モデル: FNumber / FocalLength → 減光係数
//
// cos^4 則: E(θ) ∝ cos^4(θ)
//   θ は画素の入射角。画面端ほど θ が大きく、光量が減る。
//   有効画角（FoV）と F 値で減光の強度が変わる。
//
// 近似式:
//   vignette_factor = lerp(1.0, cos4(r * half_fov), phys_strength)
//   phys_strength は FNumber（開放ほど大）と focal_length（広角ほど大）で決まる。
// ---------------------------------------------------------------------------
float compute_physical_vignette(float r_normalized)
{
    // r_normalized : 中心からの正規化距離 [0..1]（角で 1.0 程度）

    // 画角の半分 [rad]。fov は Camera_Constants から
    float half_fov = fov * 0.5f;

    // ピクセルの入射角 θ ≈ r_normalized * half_fov（線形近似）
    float theta = r_normalized * half_fov;

    // cos^4(θ) による減光
    float cos_theta = cos(theta);
    float cos4 = cos_theta * cos_theta * cos_theta * cos_theta;

    // 物理強度スケール
    //   FNumber が小さい（開放）ほど強い、基準 f/2.8 → scale 1.0
    //   FocalLength が小さい（広角）ほど強い、基準 50mm → scale 1.0
    float fnum_scale    = clamp(2.8f  / max(FNumber,    0.1f), 0.1f, 2.5f);
    float focal_scale   = clamp(log(50.0f) / log(max(FocalLength, 1.0f)), 0.1f, 2.5f);
    float phys_strength = saturate(fnum_scale * focal_scale * 0.5f);

    // phys_strength = 0 なら減光なし（1.0），= 1 なら cos^4 をフル適用
    return lerp(1.0f, cos4, phys_strength);
}

// ---------------------------------------------------------------------------
// アーティストカーブモデル
// smooth-step を指数で整形したグラデーション
// ---------------------------------------------------------------------------
float compute_artist_vignette(float r_normalized)
{
    // [inner_radius .. outer_radius] の範囲で 0→1 に smooth
    float t = saturate((r_normalized - inner_radius) / max(outer_radius - inner_radius, 0.0001f));

    // smoothstep をさらに指数でシャープ化
    float smooth = t * t * (3.0f - 2.0f * t); // cubic smoothstep
    return 1.0f - pow(smooth, smoothness * 0.5f);
}

// ---------------------------------------------------------------------------
// メイン
// ---------------------------------------------------------------------------
float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;
    float4 color = color_map.Sample(sampler_states[LINEAR_CLAMP], uv);

    // パススルー
    if (!is_enabled)
    {
        return color;
    }

    // 中心からの正規化距離（アスペクト比補正）
    // 画面が横長でも円形の減光になるよう Y 成分を補正
    float2 p = uv - float2(0.5f, 0.5f);
    // 16:9 前提の簡易 aspect 補正（実際の aspect は b1 に入っていないため近似）
    // より正確にしたい場合は aspect をシェーダに渡すこと
    p.x *= (16.0f / 9.0f);
    float r = length(p) * 1.41421f; // 最大距離（角）で 1.0 になるよう正規化

    // 減光係数を計算
    float vignette;
    if (physical_link)
    {
        vignette = compute_physical_vignette(r);
    }
    else
    {
        vignette = compute_artist_vignette(r);
    }

    // intensity でスケール（1.0 が通常、0.0 で効果なし）
    // intensity > 1.0 で過剰な減光（シネマティック表現）
    vignette = lerp(1.0f, vignette, intensity);

    return float4(color.rgb * vignette, color.a);
}
