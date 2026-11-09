#include "common.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"
#if defined(DIRECTIONAL_SHADOW_PCSS)
cbuffer DIRECTIONAL_PCSS_CONSTANT_BUFFER : register(b13)
{
    float pcss_angular_radius;
    float pcss_search_radius_texels;
    float pcss_max_filter_radius_texels;
    float pcss_min_filter_radius_texels;
};

static const float2 pcss_disk[16] =
{
    float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725),
    float2(-0.09418410, -0.92938870), float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432), float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845), float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554), float2(0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023), float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507), float2(-0.81409955, 0.91437590),
    float2(0.19984126, 0.78641367), float2(0.14383161, -0.14100790)
};

float2 RotateShadowDisk(float2 sample_position, float2 rotation)
{
    return float2(sample_position.x * rotation.x - sample_position.y * rotation.y,
        sample_position.x * rotation.y + sample_position.y * rotation.x);
}
#endif

float SampleDirectionalShadow(Texture2D<float> shadow_map, float3 world_position, float3 normal, float3 light_dir
#if defined(DIRECTIONAL_SHADOW_PCSS)
    , float2 screen_pixel
#endif
)
{
    float4 light_clip = mul(float4(world_position, 1.0f), light_view_projection);
    // LiSPSM has a virtual eye in light space. Reject points behind it.
    bool behind_warp_eye = light_clip.w <= 0.0f;
    float4 light_view_position = light_clip / max(light_clip.w, 1.0e-6f);

    float2 uv = light_view_position.xy * float2(0.5f, -0.5f) + 0.5f;
    bool outside_shadow_map = behind_warp_eye || any(uv < 0.0f) || any(uv > 1.0f) || light_view_position.z < 0.0f || light_view_position.z > 1.0f;
    uv = saturate(uv);

    float2 shadow_map_size = float2(1.0f, 1.0f);
    shadow_map.GetDimensions(shadow_map_size.x, shadow_map_size.y);
    float2 texel_size = 1.0f / shadow_map_size;

    // LiSPSM の深度勾配は光の角度で変わるため、バイアスはワールド距離から換算する。
    float4 light_step = mul(float4(-light_dir, 0.0f), light_view_projection);
    float depth_per_metre = abs((light_step.z - light_view_position.z * light_step.w) /
        max(light_clip.w, 1.0e-6f));
    // Preserve contact placement. With the 32-bit directional map, only a
    // millimetre-scale receiver offset is needed to suppress acne.
    float bias_metres = 0.0015f + 0.010f * (1.0f - saturate(dot(normal, light_dir)));
    float depth = saturate(light_view_position.z - bias_metres * depth_per_metre);

#if defined(DIRECTIONAL_SHADOW_PCSS)
    // Rotate each pixel's disk so its fixed tap pattern cannot project into
    // repeating bands on a large, nearly uniform receiver.
    float noise = frac(52.9829189f * frac(dot(screen_pixel,
        float2(0.06711056f, 0.00583715f))));
    float angle = noise * 6.2831853f;
    float2 disk_rotation = float2(cos(angle), sin(angle));
    // Fit the receiver plane in shadow UV space. Comparing every search tap
    // against the center depth mistakes a sloped floor for a blocker.
    float2 uv_dx = ddx(uv);
    float2 uv_dy = ddy(uv);
    float depth_dx = ddx(light_view_position.z);
    float depth_dy = ddy(light_view_position.z);
    float jacobian = uv_dx.x * uv_dy.y - uv_dx.y * uv_dy.x;
    float2 receiver_depth_gradient = 0.0f;
    float derivative_area = max(length(uv_dx) * length(uv_dy), 1.0e-20f);
    if (abs(jacobian) > derivative_area * 1.0e-3f)
    {
        receiver_depth_gradient = float2(
            (depth_dx * uv_dy.y - depth_dy * uv_dx.y) / jacobian,
            (uv_dx.x * depth_dy - uv_dy.x * depth_dx) / jacobian);
    }
    float pcss_result = 1.0f;
    if (!outside_shadow_map)
    {
        // A runtime PCF reference distinguishes PCSS sampling artifacts
        // from GI or geometry artifacts in the shadow debug view.
        if (pcss_angular_radius <= 0.0f)
        {
            float pcf_visibility = 0.0f;
            [unroll]
            for (int y = -1; y <= 1; ++y)
            {
                [unroll]
                for (int x = -1; x <= 1; ++x)
                {
                    float2 offset = float2(x, y) * texel_size;
                    float sample_depth = saturate(depth + clamp(
                        dot(receiver_depth_gradient, offset), -0.02f, 0.02f));
                    pcf_visibility += shadow_map.SampleCmpLevelZero(
                        comparison_sampler_state, uv + offset, sample_depth);
                }
            }
            pcss_result = pcf_visibility / 9.0f;
        }
        else
        {
        // Search near the receiver and across the light footprint. Rotated
        // Poisson taps avoid the repeated grid pattern on wide floor areas.
        float blocker_gap_sum = 0.0f;
        float blocker_count = 0.0f;
        // Ignore depth differences smaller than the shadow map's practical
        // precision. A flat receiver otherwise becomes its own blocker and
        // produces broad, view-dependent bands on floors.
        float blocker_epsilon = max(depth_per_metre * 0.002f, 1.0e-6f);
        float center_blocker_depth = shadow_map.SampleLevel(
            sampler_states[POINT_CLAMP], uv, 0);
        if (center_blocker_depth < depth - blocker_epsilon)
        {
            blocker_gap_sum = depth - center_blocker_depth;
            blocker_count = 1.0f;
        }
        [unroll]
        for (int i = 0; i < 24; ++i)
        {
            float sample_radius = i < 8 ? 1.5f : pcss_search_radius_texels;
            float2 disk = RotateShadowDisk(pcss_disk[i & 15], disk_rotation);
            float2 sample_uv = uv + disk * texel_size * sample_radius;
            if (any(sample_uv < 0.0f) || any(sample_uv > 1.0f))
                continue;
            float sample_depth = saturate(depth + clamp(
                dot(receiver_depth_gradient, sample_uv - uv), -0.02f, 0.02f));
            float blocker_depth = shadow_map.SampleLevel(
                sampler_states[POINT_CLAMP], sample_uv, 0);
            if (blocker_depth < sample_depth - blocker_epsilon)
            {
                blocker_gap_sum += sample_depth - blocker_depth;
                blocker_count += 1.0f;
            }
        }
        if (blocker_count > 0.0f)
        {
            // Each tap's depth gap has already been measured from its local
            // receiver plane. Convert the mean to world metres.
            float blocker_distance = (blocker_gap_sum / blocker_count) /
                max(depth_per_metre, 1.0e-6f);

            float3 reference_axis = abs(light_dir.y) < 0.9f ? float3(0, 1, 0) : float3(1, 0, 0);
            float3 tangent = normalize(cross(light_dir, reference_axis));
            float3 bitangent = cross(light_dir, tangent);
            float4 tangent_step = mul(float4(tangent, 0.0f), light_view_projection);
            float4 bitangent_step = mul(float4(bitangent, 0.0f), light_view_projection);
            // LiSPSM の非一様な伸縮を考慮し、光源面の円を UV 上へ投影する。
            float2 tangent_uv = (tangent_step.xy - light_view_position.xy * tangent_step.w) *
                float2(0.5f, -0.5f) / light_clip.w;
            float2 bitangent_uv = (bitangent_step.xy - light_view_position.xy * bitangent_step.w) *
                float2(0.5f, -0.5f) / light_clip.w;
            float texels_per_metre = max(length(tangent_uv * shadow_map_size),
                length(bitangent_uv * shadow_map_size));
            float filter_radius = clamp(blocker_distance * pcss_angular_radius * texels_per_metre,
                pcss_min_filter_radius_texels, pcss_max_filter_radius_texels);
            float filter_radius_metres = filter_radius / max(texels_per_metre, 1.0e-5f);

            float shadow = 0.0f;
            [unroll]
            for (int i = 0; i < 24; ++i)
            {
                float sample_radius = i < 8 ? 0.5f : 1.0f;
                float2 disk = RotateShadowDisk(pcss_disk[i & 15], disk_rotation) *
                    (filter_radius_metres * sample_radius);
                float2 offset = disk.x * tangent_uv + disk.y * bitangent_uv;
                float sample_depth = saturate(depth + clamp(
                    dot(receiver_depth_gradient, offset), -0.02f, 0.02f));
                shadow += shadow_map.SampleCmpLevelZero(comparison_sampler_state,
                    uv + offset, sample_depth);
            }
            pcss_result = shadow / 24.0f;
        }
        }
    }
    return pcss_result;
#else
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
#endif
}
