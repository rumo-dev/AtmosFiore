#include "gltf_model.hlsli"

cbuffer VOXEL_GI_CONSTANTS : register(b8)
{
    float4 volume_min;
    float4 inv_extent;
    uint resolution;
    uint slice;
    float strength;
    float padding;
};

[maxvertexcount(3)]
void main(triangle VS_OUT input[3], inout TriangleStream<VS_OUT> output)
{
    float3 a = input[1].w_position.xyz - input[0].w_position.xyz;
    float3 b = input[2].w_position.xyz - input[0].w_position.xyz;
    float3 axis = abs(cross(a, b));
    uint dominant = axis.x > axis.y && axis.x > axis.z ? 0 : (axis.y > axis.z ? 1 : 2);

    [unroll] for (uint i = 0; i < 3; ++i)
    {
        VS_OUT vertex = input[i];
        float3 p = (vertex.w_position.xyz - volume_min.xyz) * inv_extent.xyz;
        float2 projected = dominant == 0 ? p.yz : (dominant == 1 ? p.xz : p.xy);
        vertex.position = float4(projected.x * 2 - 1, 1 - projected.y * 2, 0.5, 1);
        output.Append(vertex);
    }
    output.RestartStrip();
}
