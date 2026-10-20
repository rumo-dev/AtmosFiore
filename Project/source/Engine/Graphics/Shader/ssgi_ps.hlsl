// Half-resolution, single-bounce diffuse GI gather from the lit screen.
// Uses only visible surfaces and deliberately bounds the work to eight taps.
#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"

Texture2D<float4> lit_scene : register(t0);
Texture2D<float4> gbuffer_normal_roughness : register(t1);
Texture2D<float4> gbuffer_albedo_metallic : register(t2);
Texture2D<float4> gbuffer_position_ao : register(t3);

cbuffer SSGI_CONSTANT_BUFFER : register(b12)
{
    float ssgi_radius;
    float ssgi_intensity;
    float ssgi_thickness;
    float ssgi_debug_view;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 center_data = gbuffer_position_ao.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float3 center_normal_world = gbuffer_normal_roughness.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz;
    float3 albedo = gbuffer_albedo_metallic.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0).rgb;
    if (center_data.w <= 1.0e-4f || dot(center_normal_world, center_normal_world) < 1.0e-4f)
        return 0.0f.xxxx;

    float3 center_position = mul(float4(center_data.xyz, 1.0f), view).xyz;
    float3 center_normal = normalize(mul(float4(center_normal_world, 0.0f), view).xyz);
    float2 radius_uv = 0.5f * ssgi_radius * float2(projection[0][0], projection[1][1]) / max(center_position.z, near_z);
    uint width, height;
    gbuffer_position_ao.GetDimensions(width, height);
    float2 texel = 1.0f / float2(width, height);
    static const float2 taps[8] = {
        float2(1.0f, 0.0f), float2(-1.0f, 0.0f), float2(0.0f, 1.0f), float2(0.0f, -1.0f),
        float2(0.7071f, 0.7071f), float2(-0.7071f, 0.7071f),
        float2(0.7071f, -0.7071f), float2(-0.7071f, -0.7071f)
    };

    float3 gathered = 0.0f;
    float weight_sum = 0.0f;
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        float2 sample_uv = pin.texcoord + taps[i] * radius_uv * 0.65f;
        if (any(sample_uv <= texel) || any(sample_uv >= 1.0f - texel)) continue;

        float4 sample_data = gbuffer_position_ao.SampleLevel(sampler_states[POINT_CLAMP], sample_uv, 0);
        float3 sample_normal_world = gbuffer_normal_roughness.SampleLevel(sampler_states[POINT_CLAMP], sample_uv, 0).xyz;
        if (sample_data.w <= 1.0e-4f || dot(sample_normal_world, sample_normal_world) < 1.0e-4f) continue;

        float3 sample_position = mul(float4(sample_data.xyz, 1.0f), view).xyz;
        float3 delta = sample_position - center_position;
        float distance_to_sample = length(delta);
        if (distance_to_sample <= 1.0e-3f || distance_to_sample > ssgi_radius) continue;
        float3 direction_to_sample = delta / distance_to_sample;
        float3 sample_normal = normalize(mul(float4(sample_normal_world, 0.0f), view).xyz);
        float receiver_cosine = saturate(dot(center_normal, direction_to_sample));
        float source_cosine = saturate(dot(sample_normal, -direction_to_sample));
        if (receiver_cosine <= 0.0f || source_cosine <= 0.0f) continue;

        float separation = abs(sample_position.z - center_position.z);
        float thickness_weight = saturate(1.0f - separation / max(ssgi_radius + ssgi_thickness, 1.0e-3f));
        float distance_weight = rcp(1.0f + distance_to_sample * distance_to_sample);
        float weight = receiver_cosine * source_cosine * thickness_weight * distance_weight;
        float3 incoming_radiance = lit_scene.SampleLevel(sampler_states[LINEAR_CLAMP], sample_uv, 0).rgb;
        gathered += incoming_radiance * weight;
        weight_sum += weight;
    }

    float3 indirect = (weight_sum > 1.0e-5f ? gathered / weight_sum : 0.0f) * albedo * ssgi_intensity;
    return float4(indirect, 1.0f);
}
