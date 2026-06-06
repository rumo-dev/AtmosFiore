cbuffer POINT_SHADOW_CONSTANT_BUFFER : register(b5)
{
    float4 point_shadow_position_radius; // xyz: light position, w: radius
    float4 point_shadow_params; // x: near, y: far, z: z sign, w: depth bias
    float4 point_shadow_options; // x: strength, y: enabled
};

float2 DualParaboloidUV(float3 light_vector)
{
    float3 direction = normalize(light_vector);
    float z = abs(direction.z);
    return direction.xy / (z + 1.0f) * float2(0.5f, -0.5f) + 0.5f;
}

float PointShadowDepth(float distance_from_light)
{
    return saturate((distance_from_light - point_shadow_params.x) /
        max(point_shadow_params.y - point_shadow_params.x, 0.0001f));
}

#include "samplers.hlsli"
#include "constants.hlsli"
float SamplePointLightShadow(Texture2D<float> frontshadow_map, Texture2D<float> backshadow_map, float3 world_position, float3 normal, float3 light_dir, int light_index)
{
    float3 light_vector = world_position - point_shadow_position_radius.xyz;
    float distance_from_light = length(light_vector);
    bool use_shadow =
        point_shadow_options.y > 0.0f &&
        light_index == 0 &&
        distance_from_light > point_shadow_params.x &&
        distance_from_light < point_shadow_params.y;

    float lit = 1.0f;
    if (use_shadow)
    {
        float2 uv = DualParaboloidUV(light_vector);
        float slope_bias = point_shadow_params.w * (1.0f - saturate(dot(normal, light_dir)));
        float receiver_depth = PointShadowDepth(distance_from_light) - max(point_shadow_params.w, slope_bias);
        lit = light_vector.z >= 0.0f
            ? frontshadow_map.SampleCmpLevelZero(comparison_sampler_state, uv, receiver_depth)
            : backshadow_map.SampleCmpLevelZero(comparison_sampler_state, uv, receiver_depth);
    }

   // return lerp(1.0f - point_shadow_options.x, 1.0f, lit);
    return lerp(1.0f - 1.0f, 1.0f, lit);
}

// スクリーンスペースシャドウ（ライトごとのループ内で呼び出す）
float CalculateDeferredWorldContactShadow(
    float2 uv,
    float3 lightPos,
    float lightRadius,
    Texture2D<float4> GBufferPositionTex,
    Texture2D<float4> GBufferNormalTex)
{
    float3 P =
        GBufferPositionTex.SampleLevel(
            sampler_states[POINT_CLAMP],
            uv,
            0).xyz;

    float3 N =
        normalize(
            GBufferNormalTex.SampleLevel(
                sampler_states[POINT_CLAMP],
                uv,
                0).xyz);

    float3 toLight =
        lightPos - P;

    float lightDist =
        length(toLight);

    if (lightDist < 0.001f)
        return 1.0f;

    if (lightDist > lightRadius)
        return 1.0f;

    float3 L =
        normalize(toLight);

    if (dot(N, L) <= 0.0f)
        return 1.0f;

    const int MAX_STEPS = 48;

    float maxDistance =
        min(lightDist, 0.8f);

    float step =
        maxDistance /
        MAX_STEPS;

    const float normalBias = 0.015f;
    const float lightBias = 0.005f;

    const float thickness = 0.05f;

    float3 start =
        P +
        N * normalBias +
        L * lightBias;

    for (int i = 1;
         i <= MAX_STEPS;
         ++i)
    {
        float3 rayPos =
            start +
            L *
            (i * step);

        float4 clip =
            mul(
                float4(
                    rayPos,
                    1),
                view_projection);

        if (clip.w <= 0)
            break;

        float2 rayUV =
            clip.xy /
            clip.w;

        rayUV =
            rayUV *
            float2(
                0.5,
                -0.5)
            + 0.5;

        if (
            any(rayUV < 0) ||
            any(rayUV > 1))
            break;

        float3 scenePos =
            GBufferPositionTex
            .SampleLevel(
                sampler_states[POINT_CLAMP],
                rayUV,
                0)
            .xyz;

        if (all(scenePos == 0))
            continue;

        float3 d =
            scenePos -
            rayPos;

        float along =
            dot(d, L);

        float lateral =
            length(
                d -
                along * L);

        if (
            abs(along) <
            thickness &&
            lateral <
            thickness * 3)
        {
            return 0.0f;
        }
    }

    return 1.0f;
}