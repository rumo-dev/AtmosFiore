#include "constants.hlsli"
#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"

cbuffer SkyLook : register(b8)
{
    float gTime;
    float art_strength;
    float directional_strength;
    float fog_blend;
    float4 zenith_tint;
    float4 horizon_tint;
    float4 directional_tint;
    float4 fog_tint;
    float4 cloud_tint;
    float4 paint_direction_vignette;
    float4 cloud_controls;
};

Texture2D<float4> scene_color : register(t0);
Texture2D<float> scene_depth : register(t1);
Texture3D<float4> aerial_perspective_lut : register(t7);
Texture2D<float4> skybox_color : register(t8);

float4 main(VS_OUT input) : SV_TARGET
{
    const float depth = scene_depth.Sample(sampler_states[POINT_CLAMP],input.texcoord).r;
    const bool foreground = isReversed ? depth > 0.000001f : depth < 0.999999f;
    if(!foreground)
        return skybox_color.Sample(sampler_states[LINEAR_CLAMP],input.texcoord);

    float4 scene = scene_color.Sample(sampler_states[LINEAR_CLAMP],input.texcoord);
    float2 ndc = input.texcoord * 2.0f - 1.0f;
    ndc.y = -ndc.y;
    float4 view_position = mul(float4(ndc,depth,1),inv_projection);
    view_position.xyz /= max(view_position.w,0.00001f);
    float distance = min(length(view_position.xyz),80000.0f);
    float3 ray = normalize(mul(view_position.xyz,(float3x3)inv_view));
    float sun_elevation = normalize(-directional_light_direction.xyz).y;
    float3 lookup = float3(saturate(ray.y*0.5f+0.5f),
        saturate((sun_elevation+0.2f)/1.2f),saturate(distance/80000.0f));
    float4 aerial = aerial_perspective_lut.SampleLevel(sampler_states[LINEAR_CLAMP],lookup,0);
    float aerial_luminance = dot(aerial.rgb,float3(0.2126f,0.7152f,0.0722f));
    float3 painted_aerial = lerp(aerial.rgb,fog_tint.rgb*max(aerial_luminance,0.01f),
        saturate(fog_blend*art_strength));
    return float4(scene.rgb*aerial.a+painted_aerial,scene.a);
}
