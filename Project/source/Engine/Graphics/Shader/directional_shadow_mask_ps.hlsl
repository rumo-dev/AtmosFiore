#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"
#define DIRECTIONAL_SHADOW_PCSS 1
#include "shadow_directional.hlsli"

Texture2D<float4> gbuffer_normal : register(t0);
Texture2D<float4> gbuffer_position : register(t1);
Texture2D<float> directional_shadow_map : register(t2);

float4 main(VS_OUT pin) : SV_TARGET
{
    float3 normal = gbuffer_normal.SampleLevel(
        sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz;
    if (dot(normal, normal) < 1.0e-4f)
        return 1.0f.xxxx;

    normal = normalize(normal);
    float3 light_direction = normalize(-directional_light_direction.xyz);
    float3 position = gbuffer_position.SampleLevel(
        sampler_states[POINT_CLAMP], pin.texcoord, 0).xyz;
    float visibility = SampleDirectionalShadow(
        directional_shadow_map, position, normal, light_direction, pin.position.xy);
    return visibility.xxxx;
}
