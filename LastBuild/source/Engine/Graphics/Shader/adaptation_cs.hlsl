// adaptation_cs.hlsl - 自動露出調整（Eye Adaptation）の計算
#include "samplers.hlsli"

// t0: シーンのHDRテクスチャ（1x1に縮小されたミップマップレベル）
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
    uint2 mip_dimensions;
    current_scene_mip1x1.GetDimensions(mip_dimensions.x, mip_dimensions.y);
    
    // 1x1に縮小されたミップマップから平均カラーを取得
    float3 avg_color = 0;
    for (int x = 0; x < mip_dimensions.x; ++x)
    {
        for (int y = 0; y < mip_dimensions.y; ++y)
        {
            avg_color += current_scene_mip1x1.Load(int3(x, y, 0)).rgb;
        }
    }
    avg_color /= (mip_dimensions.x * mip_dimensions.y); // 平均を取る
        //float3 avg_color = current_scene_mip1x1.Load(int3(0, 0, 0)).rgb;

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
//ターゲットをかえれるように’