// Screen-space reflections using perspective-correct screen-space DDA tracing.
// Based on McGuire and Mara, Efficient GPU Screen-Space Ray Tracing (2014).
#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"

Texture2D<float4> scene_color : register(t0);
Texture2D<float4> gbuffer_normal_roughness : register(t1);
Texture2D<float4> gbuffer_albedo_metallic : register(t2);
Texture2D<float4> gbuffer_position_ao : register(t3);
Texture2D<float> scene_depth : register(t4);

cbuffer SSR_CONSTANT_BUFFER : register(b12)
{
    float max_distance;
    float surface_thickness;
    float reflection_strength;
    float roughness_cutoff;
    float metallic_cutoff;
    int max_steps;
    float2 padding;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 base_color = scene_color.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float4 normal_roughness = gbuffer_normal_roughness.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float4 albedo_metallic = gbuffer_albedo_metallic.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float3 world_position = gbuffer_position_ao.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz;
    float3 world_normal = normal_roughness.xyz;
    float roughness = saturate(normal_roughness.w);
    float metallic = saturate(albedo_metallic.w);

    if (dot(world_normal, world_normal) < 1.0e-4f ||
        roughness >= roughness_cutoff || metallic < metallic_cutoff)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float3 view_position = mul(float4(world_position, 1.0f), view).xyz;
    float3 view_normal = normalize(mul(float4(world_normal, 0.0f), view).xyz);
    float3 view_vector = normalize(-view_position);
    float3 ray_direction = normalize(reflect(-view_vector, view_normal));
    if (abs(ray_direction.z) <= 1.0e-4f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float3 ray_origin = view_position + view_normal * max(surface_thickness * 0.5f, 0.001f);
    float ray_length = max_distance;
    if (ray_direction.z < 0.0f && ray_origin.z + ray_direction.z * ray_length < near_z)
        ray_length = max(0.0f, (near_z - ray_origin.z) / ray_direction.z);
    float3 ray_end = ray_origin + ray_direction * ray_length;
    float4 clip0 = mul(float4(ray_origin, 1.0f), projection);
    float4 clip1 = mul(float4(ray_end, 1.0f), projection);
    if (clip0.w <= 1.0e-5f || clip1.w <= 1.0e-5f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    uint width, height;
    gbuffer_position_ao.GetDimensions(width, height);
    float2 dimensions = float2(width, height);
    float2 uv0 = float2(0.5f + 0.5f * clip0.x / clip0.w, 0.5f - 0.5f * clip0.y / clip0.w);
    float2 uv1 = float2(0.5f + 0.5f * clip1.x / clip1.w, 0.5f - 0.5f * clip1.y / clip1.w);
    float2 pixel0 = uv0 * dimensions;
    float2 pixel1 = uv1 * dimensions;
    float2 delta = pixel1 - pixel0;
    float pixel_length = max(abs(delta.x), abs(delta.y));
    if (pixel_length < 1.0f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    // A bounded DDA visits adjacent screen pixels. Perspective-correctly
    // interpolate camera-space Z along the projected ray, as in the paper.
    int step_count = min((int)ceil(pixel_length), max(max_steps, 8));
    float reciprocal_w0 = rcp(clip0.w);
    float reciprocal_w1 = rcp(clip1.w);
    float z_over_w0 = ray_origin.z * reciprocal_w0;
    float z_over_w1 = ray_end.z * reciprocal_w1;
    float hit_t = 1.0f;
    float2 hit_uv = 0.0f;
    bool hit = false;

    [loop]
    for (int step_index = 1; step_index <= step_count; ++step_index)
    {
        float t = (float)step_index / step_count;
        float2 pixel = pixel0 + delta * t;
        float2 sample_uv = (floor(pixel) + 0.5f) / dimensions;
        if (any(sample_uv <= 0.0f) || any(sample_uv >= 1.0f))
            break;

        float reciprocal_w = lerp(reciprocal_w0, reciprocal_w1, t);
        float ray_z = lerp(z_over_w0, z_over_w1, t) / max(reciprocal_w, 1.0e-6f);
        // The depth buffer stores projected Z; reconstruct view-space Z
        // directly so the common ray-march step needs one texture read.
        float raw_depth = scene_depth.SampleLevel(sampler_states[POINT_CLAMP], sample_uv, 0);
        float depth_denominator = raw_depth - projection._33;
        if (abs(depth_denominator) < 1.0e-6f)
            continue;
        float scene_z = projection._43 / depth_denominator;
        float depth_delta = ray_z - scene_z;
        float traveled = ray_length * t;
        if (traveled > surface_thickness * 2.0f && depth_delta >= 0.0f && depth_delta <= surface_thickness)
        {
            float3 sample_normal = gbuffer_normal_roughness.SampleLevel(
                sampler_states[POINT_CLAMP], sample_uv, 0).xyz;
            if (dot(sample_normal, sample_normal) < 1.0e-4f)
                continue;
            hit = true;
            hit_t = t;
            hit_uv = sample_uv;
            break;
        }
    }

    if (!hit)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float3 reflected_radiance = scene_color.SampleLevel(sampler_states[LINEAR_CLAMP], hit_uv, 0).rgb;
    float3 view_normal_world = normalize(world_normal);
    float no_v = saturate(dot(view_normal_world, normalize(camera_position.xyz - world_position)));
    float3 f0 = lerp(0.04f.xxx, albedo_metallic.rgb, metallic);
    float3 fresnel = f0 + (1.0f - f0) * pow(1.0f - no_v, 5.0f);
    float roughness_fade = saturate(1.0f - roughness / max(roughness_cutoff, 1.0e-3f));
    float edge_fade = saturate(min(min(hit_uv.x, hit_uv.y), min(1.0f - hit_uv.x, 1.0f - hit_uv.y)) * 12.0f);
    float distance_fade = saturate(1.0f - hit_t * 0.5f);
    float3 weight = saturate(fresnel * reflection_strength * roughness_fade * edge_fade * distance_fade);
    return float4((reflected_radiance - base_color.rgb) * weight, 1.0f);
}
