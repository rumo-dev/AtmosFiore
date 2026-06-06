#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"

Texture2D texture_maps[6] : register(t0);

#define NORMAL_ROUGHNESS_MAP 0
#define BASE_METALNESS_MAP 1
#define POSITION_OCULUSION_MAP 2
#define EMISSIVE_MAP 3
#define CAMERA_COLOR_MAP 4
#define SHADOW_DEPTH_MAP 5
//PCF
#if 0

float4 main(VS_OUT pin) : SV_TARGET
{
    // 1. 各種データのサンプリング
    float3 N = texture_maps[NORMAL_ROUGHNESS_MAP].SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz;
    float3 L = normalize(-directional_light_direction.xyz);
    float3 wposition = texture_maps[POSITION_OCULUSION_MAP].SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz;

    // 2. ライト空間への変換
    float4 light_view_position = mul(float4(wposition.xyz, 1.0f), light_view_projection);
    light_view_position /= light_view_position.w;

    float2 light_view_texcoord;
    light_view_texcoord.x = light_view_position.x * +0.5 + 0.5;
    light_view_texcoord.y = light_view_position.y * -0.5 + 0.5;

    // 3. バイアス計算と深度の決定
    const float shadow_depth_bias = max(0.005 * (1.0 - dot(N, L)), 0.0005);
    float depth = saturate(light_view_position.z - shadow_depth_bias);

    // --- 影のジャギ対策 (PCF: Percentage Closer Filtering) ---
    float shadow = 0.0;
    float2 shadow_map_size;
    texture_maps[SHADOW_DEPTH_MAP].GetDimensions(shadow_map_size.x, shadow_map_size.y);
    float2 texel_size = 1.0 / shadow_map_size;

    // 5x5の範囲で比較・平均化を行う
    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            float2 offset = float2(x, y) * texel_size;
            shadow += texture_maps[SHADOW_DEPTH_MAP].SampleCmpLevelZero(
                comparison_sampler_state,
                light_view_texcoord + offset,
                depth
            ).r;
        }
    }
    shadow /= 25.0; // 25点サンプルの平均
    // -------------------------------------------------------

    // 4. 色の合成
    float4 color = texture_maps[CAMERA_COLOR_MAP].Sample(sampler_states[POINT_CLAMP], pin.texcoord, 0);

    float shadow_strength = 0.5; // 影の濃さ（0.0で真っ暗、1.0で影響なし）
    float lighting = lerp(shadow_strength, 1.0, shadow);

    return float4(color.rgb * lighting, color.a);
}
#else //SSPCSS

// --- PCSS用パラメータ ---
static const int BLOCKER_SAMPLES = 16;
static const int PCF_SAMPLES = 16;
static const float LIGHT_SIZE = 0.02; // 光源の大きさ（ボケ具合に直結）

// Poisson Disk サンプリング用の点群
static const float2 poisson_disk[16] =
{
    float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725),
    float2(-0.094184101, -0.92938870), float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432), float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845), float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554), float2(0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023), float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507), float2(-0.81409955, 0.91437590),
    float2(0.19984126, 0.78641367), float2(0.14383161, -0.14100790)
};

float4 main(VS_OUT pin) : SV_TARGET
{
    // 1. 各種データのサンプリング
    float3 N = texture_maps[NORMAL_ROUGHNESS_MAP].SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz;
    float3 L = normalize(-directional_light_direction.xyz);
    float3 wposition = texture_maps[POSITION_OCULUSION_MAP].SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz;

    // 2. ライト空間への変換
    float4 light_view_position = mul(float4(wposition, 1.0f), light_view_projection);
    light_view_position /= light_view_position.w;

    float2 uv = light_view_position.xy * float2(0.5, -0.5) + 0.5;

    const float shadow_depth_bias = max(0.005 * (1.0 - dot(N, L)), 0.0005);
    float depth = saturate(light_view_position.z - shadow_depth_bias);

    // --- SSPCSS 処理開始 ---

    float2 shadow_map_size;
    texture_maps[SHADOW_DEPTH_MAP].GetDimensions(shadow_map_size.x, shadow_map_size.y);
    float2 texel_size = 1.0 / shadow_map_size;

    // STEP 1: ブロッカー検索 (平均遮蔽距離の算出)
    float avg_blocker_depth = 0;
    int blocker_count = 0;
    float search_region = LIGHT_SIZE * (depth); // 受影面の距離に応じて検索範囲を調整

    for (int i = 0; i < BLOCKER_SAMPLES; i++)
    {
        float2 offset = poisson_disk[i] * texel_size * search_region * 200.0;
        float b_depth = texture_maps[SHADOW_DEPTH_MAP].SampleLevel(sampler_states[POINT_CLAMP], uv + offset, 0).r;
        if (b_depth < depth)
        {
            avg_blocker_depth += b_depth;
            blocker_count++;
        }
    }

    // 遮蔽物がない場合は影なしとして描画
    if (blocker_count == 0)
        return texture_maps[CAMERA_COLOR_MAP].Sample(sampler_states[POINT_CLAMP], pin.texcoord);

    avg_blocker_depth /= (float) blocker_count;

    // STEP 2: ペナンブラサイズの決定
    // (受影面深度 - 平均遮蔽深度) / 平均遮蔽深度 * 光源サイズ
    float penumbra = (depth - avg_blocker_depth) * LIGHT_SIZE / avg_blocker_depth;
    penumbra = max(penumbra, 0.0);

    // STEP 3: フィルタリング (可変半径PCF)
    float shadow = 0.0;
    [unroll]
    for (int j = 0; j < PCF_SAMPLES; j++)
    {
        float2 offset = poisson_disk[j] * texel_size * penumbra * 500.0;
        shadow += texture_maps[SHADOW_DEPTH_MAP].SampleCmpLevelZero(
            comparison_sampler_state,
            uv + offset,
            depth
        ).r;
    }
    shadow /= (float) PCF_SAMPLES;

    // 4. 色の合成
    float4 color = texture_maps[CAMERA_COLOR_MAP].Sample(sampler_states[POINT_CLAMP], pin.texcoord);
    float shadow_strength = 0.5;
    float result = lerp(shadow_strength, 1.0, shadow);

    return float4(color.rgb * result, color.a);
}
#endif