#include "gltf_model.hlsli"
#include "shadow_point.hlsli"

struct POINT_SHADOW_VS_OUT
{
    float4 position : SV_POSITION;
    float3 world_position : TEXCOORD0;
    float clip_distance : SV_ClipDistance0;
};

POINT_SHADOW_VS_OUT main(VS_IN vin)
{
    if (skin > -1)
    {
        row_major float4x4 skin_matrix =
            vin.weights.x * joint_matrices[vin.joints.x] +
            vin.weights.y * joint_matrices[vin.joints.y] +
            vin.weights.z * joint_matrices[vin.joints.z] +
            vin.weights.w * joint_matrices[vin.joints.w];
        vin.position = mul(float4(vin.position.xyz, 1), skin_matrix);
    }

    float3 world_position = mul(float4(vin.position.xyz, 1), world).xyz;
    float3 light_vector = world_position - point_shadow_position_radius.xyz;
    float distance_from_light = length(light_vector);
    float3 direction = light_vector / max(distance_from_light, 0.0001f);

    POINT_SHADOW_VS_OUT vout;
    vout.world_position = world_position;

    const float seam_overlap = 0.02f;
    float hemisphere_z = direction.z * point_shadow_params.z;
    float projection_w = max(1.0f + hemisphere_z, 0.0001f);
    float projection_scale = rcp(1.0f + seam_overlap);
    float depth = PointShadowDepth(distance_from_light, point_shadow_params.y);
    vout.clip_distance = hemisphere_z + seam_overlap;
    // Use the paraboloid denominator as clip-space w. This preserves perspective-
    // correct world-position interpolation and clips the opposite pole before rasterization.
    vout.position = float4(direction.xy * projection_scale, depth * projection_w, projection_w);
    return vout;
}
