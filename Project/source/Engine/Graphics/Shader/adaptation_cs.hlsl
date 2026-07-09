// adaptation_cs.hlsl - 自動露出調整（Eye Adaptation）の計算
#include "samplers.hlsli"

// t0: シーンのHDRテクスチャ
// ※ bloom.getColorMap() (bloom_final のフルミップチェイン付きSRV) がバインドされる想定。
//    1x1に縮小済みの単体テクスチャではないため、最後のミップレベル（最小サイズ）を
//    GetDimensions で取得してから Load する。
Texture2D<float4> current_scene_mip1x1 : register(t0);

// u0: 読み書き可能な1x1の露出値テクスチャ（R32_FLOAT）
RWTexture2D<float> rw_exposure_texture : register(u0);

cbuffer AdaptationConfig : register(b0)
{
    float delta_time; // フレーム間の経過時間（秒）
    float speed_to_light; // 明るい場所への順応スピード（秒^-1）
    float speed_to_dark; // 暗い場所への順応スピード（秒^-1）
    float target_lum; // 目標輝度（基準値: 0.18 = 中間グレー）
};

// 定数定義
static const float MIN_LUMINANCE = 0.001f; // ゼロ除算防止の最小輝度
static const float MIN_EXPOSURE = 0.05f; // 露出の下限クランプ値
static const float MAX_EXPOSURE = 10.0f; // 露出の上限クランプ値
static const float EPSILON_TIME = 0.0001f; // デルタタイムの下限

// BT.709 標準係数による輝度計算
float GetLuminance(float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

[numthreads(1, 1, 1)]
void main()
{
    // ミップマップの総数（mip_levels）を取得する
    uint width, height, mip_levels;
    current_scene_mip1x1.GetDimensions(0, width, height, mip_levels);

    // 一番最後のミップレベル（1x1サイズ）のインデックスを計算
    // 例: 全9レベルなら 0〜8 なので、一番下は「9 - 1 = 8」
    uint last_mip_level = mip_levels - 1;

    // ループは不要！1x1のミップマップから直接画面全体の平均色（平均輝度）を取得
    // Loadの第3引数(int3のz)にミップレベルを指定する
    float3 avg_color = current_scene_mip1x1.Load(int3(0, 0, last_mip_level)).rgb;

    // 平均輝度を算出（ゼロ除算防止）
    float current_lum = max(GetLuminance(avg_color), MIN_LUMINANCE);

    // 画面の明るさに対する理想的な露出値を計算
    float target_exposure = target_lum / (current_lum * (1.0 - target_lum));

    // 前フレームの露出値を読み込む
    float old_exposure = rw_exposure_texture[uint2(0, 0)];

    // 明暗の変化方向によって順応スピードを切り替える
    // 明るくなる場合は speed_to_light、暗くなる場合は speed_to_dark
    float speed = (target_exposure > old_exposure) ? speed_to_light : speed_to_dark;

    // フレームスキップ対策：delta_time の下限を設定
    float safe_delta_time = max(delta_time, EPSILON_TIME);

    // 指数型の一時遅れフィルタで前フレームからターゲットへ滑らかに補間
    // 式: new = old + (target - old) * (1 - exp(-speed * dt))
    float new_exposure = old_exposure + (target_exposure - old_exposure) * (1.0f - exp(-speed * safe_delta_time));

    // 露出値を安全な範囲にクランプ
    new_exposure = clamp(new_exposure, MIN_EXPOSURE, MAX_EXPOSURE);

    // 新しい露出値を保存
    rw_exposure_texture[uint2(0, 0)] = new_exposure;
}
//ライティングが前提じゃない
//ターゲットをかえれるように