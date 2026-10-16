#include "shadow_point.hlsli"

struct POINT_SHADOW_VS_OUT
{
    float4 position : SV_POSITION;
    float3 world_position : TEXCOORD0;
};

float main(POINT_SHADOW_VS_OUT pin) : SV_Depth
{
    float3 light_vector = pin.world_position - point_shadow_position_radius.xyz;
    float distance_from_light = length(light_vector);

    // Evaluate radial depth per pixel. Interpolating nonlinear radial depth from
    // vertices causes visible displacement near the light.
    return PointShadowDepth(distance_from_light, point_shadow_params.y);
}
