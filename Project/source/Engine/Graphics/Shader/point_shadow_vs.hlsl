#include "gltf_model.hlsli"
#include "shadow_point.hlsli"

struct POINT_SHADOW_VS_OUT
{
    float4 position : SV_POSITION;
    float light_z : TEXCOORD0;
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
    vout.light_z = direction.z * point_shadow_params.z;

    direction.z *= point_shadow_params.z;
    float2 projected = direction.xy / (direction.z + 1.0f);

    vout.position = float4(projected, PointShadowDepth(distance_from_light), 1.0f);
    return vout;
}
