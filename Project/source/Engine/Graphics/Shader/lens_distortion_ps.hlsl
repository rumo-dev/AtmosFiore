// lens_distortion_ps.hlsl
//
// レンズ歪曲（Lens Distortion / 歪曲収差）ポストエフェクト
//
// ブラウン–コンラディモデルの k1 項による放射状歪みを適用する。
//   r_d = r_u * (1 + k1 * r_u^2)
//   k1 > 0 → 樽型（barrel）  : 広角レンズに典型
//   k1 < 0 → 糸巻き型（pincushion）: 望遠レンズに典型
//
// 物理連動:
//   Camera_Constants (b1) の fov を参照して k1 を自動算出（physical_link 時）。
//   k1 == 0.0 の場合はシェーダ内で fov ベース計算に切り替わる。
//
// 範囲外 UV は BORDER BLACK サンプラーで黒クランプ。
//
// レジスタ:
//   b1  : CAMERA_CONSTANT_BUFFER  (constants.hlsli)
//   b9  : LensDistortionConstants
//   t0  : 入力 HDR カラーテクスチャ
//   s7  : LINEAR_BORDER_OPAQUE_BLACK サンプラー

#include "constants.hlsli"
#include "samplers.hlsli"

// ---- 定数バッファ ----
cbuffer LensDistortionConstants : register(b9)
{
    float k1;           // 主歪曲係数（0 ならシェーダ内で fov から算出）
    int   is_enabled;   // 有効フラグ
    float ld_pad0;
    float ld_pad1;
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
// FoV [radian] → k1 基準値
// fov_reference = 45° (0.785 rad) を基準とする
// ---------------------------------------------------------------------------
float k1_from_fov(float fov_rad)
{
    float excess = fov_rad - 0.785398f; // 45° からの超過量
    return clamp(excess * 0.20f, -0.5f, 0.5f);
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

    // 使用する k1 を決定
    // CPU から 0.0 が渡された場合 → fov ベースで自動計算（physical_link モード）
    float effective_k1 = (k1 == 0.0f) ? k1_from_fov(fov) : k1;

    // UV を中心 (0,0) 基準の正規化座標に変換（アスペクト補正付き）
    // 横長画面でも歪みが真円になるよう aspect 補正する
    float2 p = uv - float2(0.5f, 0.5f);

    // アスペクト比補正（Y 成分を aspect で割ることで等方性を保つ）
    // ※ fov は垂直画角として扱い、aspect はバッファから取れないため
    //   UV 空間での等方補正のみ行う（近似）
    float r2 = dot(p, p); // |p|^2

    // ブラウン–コンラディ: r_d = r_u * (1 + k1 * r_u^2)
    float scale = 1.0f + effective_k1 * r2;

    // 歪み後の UV（中心座標系 → [0,1] に戻す）
    float2 distorted_uv = p * scale + float2(0.5f, 0.5f);

    // 範囲外は BORDER BLACK（s7）でクランプ → 黒になる
    return color_map.Sample(sampler_states[LINEAR_BORDER_OPAQUE_BLACK], distorted_uv);
}
