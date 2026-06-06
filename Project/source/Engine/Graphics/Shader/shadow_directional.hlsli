#include "common.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"
float SampleDirectionalShadow(Texture2D<float> shadow_map, float3 world_position, float3 normal, float3 light_dir)
{
    float4 light_view_position = mul(float4(world_position, 1.0f), light_view_projection);
    light_view_position /= light_view_position.w;

    float2 uv = light_view_position.xy * float2(0.5f, -0.5f) + 0.5f;
    bool outside_shadow_map = any(uv < 0.0f) || any(uv > 1.0f) || light_view_position.z < 0.0f || light_view_position.z > 1.0f;
    uv = saturate(uv);

    float2 shadow_map_size = float2(1.0f, 1.0f);
    shadow_map.GetDimensions(shadow_map_size.x, shadow_map_size.y);
    float2 texel_size = 1.0f / shadow_map_size;

    float bias = max(0.005f * (1.0f - saturate(dot(normal, light_dir))), 0.0005f);
    float depth = saturate(light_view_position.z - bias);

    float shadow = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            shadow += shadow_map.SampleCmpLevelZero(
                comparison_sampler_state,
                uv + float2(x, y) * texel_size,
                depth
            );
        }
    }
    float shadow_factor = shadow / 9.0f;
    float min_shadow = 0.0f; // ここを 0.0f に近づけるほど影が濃くなります
    float final_shadow = lerp(min_shadow, 1.0f, shadow_factor);

    return outside_shadow_map ? 1.0f : final_shadow;
}