// chromatic_aberration_ps.hlsl
//
// 色収差（Chromatic Aberration）ポストエフェクト
//
// RGB チャンネルを UV 中心から放射状に異なる量だけオフセットすることで、
// レンズ端での波長分散（横方向色収差）を再現する。
//
// 物理連動:
//   Camera_Constants (b1) の FocalLength / FNumber を参照して
//   シフト量を自動スケーリング。physical_link == 0 の場合は定数のみ使用。
//
// レジスタ:
//   b1  : CAMERA_CONSTANT_BUFFER  (constants.hlsli)
//   b9  : ChromaticAberrationConstants
//   t0  : 入力 HDR カラーテクスチャ
//   s4  : LINEAR_CLAMP サンプラー

#include "constants.hlsli"
#include "samplers.hlsli"

// ---- 定数バッファ ----
cbuffer ChromaticAberrationConstants : register(b9)
{
    float shift_r;      // R チャンネル UV オフセット倍率
    float shift_b;      // B チャンネル UV オフセット倍率
    int   is_enabled;   // 有効フラグ
    float ca_pad;
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
// 物理ベース スケーリング
//
//   focal_scale : 広角（短焦点）ほど大きい。基準 50mm → 1.0
//   fnum_scale  : 開放（小さい F値）ほど大きい。基準 f/2.8 → 1.0
// ---------------------------------------------------------------------------
float focal_scale(float focal_length_mm)
{
    // log(50) / log(focal_length) を [0.1, 2.5] でクランプ
    float s = log(50.0f) / max(log(max(focal_length_mm, 1.0f)), 0.001f);
    return clamp(s, 0.1f, 2.5f);
}

float fnum_scale(float f_number)
{
    // 基準 f/2.8 → 1.0 ; 開放 f/1.0 → 2.8
    return clamp(2.8f / max(f_number, 0.1f), 0.1f, 2.0f);
}

// ---------------------------------------------------------------------------
// メイン
// ---------------------------------------------------------------------------
float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;

    // パススルー
    if (!is_enabled)
    {
        return color_map.Sample(sampler_states[LINEAR_CLAMP], uv);
    }

    // UV 中心 (0.5, 0.5) からの方向ベクトル
    float2 dir = uv - float2(0.5f, 0.5f);

    // 中心からの距離（最大は角で約 0.707）を 0..1 に正規化して非線形に強調
    // 実際のレンズ収差は半径の 2~3 乗に比例する
    float  dist = length(dir);
    float  radial = dist * dist; // 半径の二乗で端に集中

    // 物理スケール（Camera_Constants b1 から FocalLength / FNumber を参照）
    float phys_scale = focal_scale(FocalLength) * fnum_scale(FNumber);

    // チャンネルごとの UV ずれ量
    // shift_r / shift_b は CPU 側で intensity 込みの倍率として渡される
    float2 offset_r = dir * (shift_r * radial * phys_scale);
    float2 offset_b = dir * (shift_b * radial * phys_scale);
    // G チャンネルはオフセットなし（基準波長）

    // サンプリング
    float r = color_map.Sample(sampler_states[LINEAR_CLAMP], uv + offset_r).r;
    float g = color_map.Sample(sampler_states[LINEAR_CLAMP], uv             ).g;
    float b = color_map.Sample(sampler_states[LINEAR_CLAMP], uv + offset_b  ).b;
    float a = color_map.Sample(sampler_states[LINEAR_CLAMP], uv             ).a;

    return float4(r, g, b, a);
}
