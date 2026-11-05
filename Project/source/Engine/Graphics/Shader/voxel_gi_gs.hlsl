#include "gltf_model.hlsli"

cbuffer VOXEL_GI_CONSTANTS : register(b8)
{
    float4 volume_min;
    float4 inv_extent;
    uint resolution;
    uint slice;
    float strength;
    float emissive_multiplier;
    float environment_fill;
    float3 padding;
};

[maxvertexcount(3)]
void main(triangle VS_OUT input[3], inout TriangleStream<VS_OUT> output)
{
    float3 a = input[1].w_position.xyz - input[0].w_position.xyz;
    float3 b = input[2].w_position.xyz - input[0].w_position.xyz;
    float3 axis = abs(cross(a, b));
    uint dominant = axis.x > axis.y && axis.x > axis.z ? 0 : (axis.y > axis.z ? 1 : 2);

    float2 projected[3];
    [unroll] for (uint vertex_index = 0; vertex_index < 3; ++vertex_index)
    {
        float3 p = (input[vertex_index].w_position.xyz - volume_min.xyz) * inv_extent.xyz;
        projected[vertex_index] = (dominant == 0 ? p.yz : (dominant == 1 ? p.xz : p.xy)) * resolution;
    }
    float2 edge01 = projected[1] - projected[0];
    float2 edge02 = projected[2] - projected[0];
    float area = edge01.x * edge02.y - edge01.y * edge02.x;
    if (abs(area) < 1e-5) return;
    float winding = area > 0 ? 1.0 : -1.0;
    float2 outward[3];
    [unroll] for (uint edge_index = 0; edge_index < 3; ++edge_index)
    {
        float2 edge = projected[(edge_index + 1) % 3] - projected[edge_index];
        outward[edge_index] = winding * float2(edge.y, -edge.x) / max(length(edge), 1e-5);
    }

    [unroll] for (uint output_index = 0; output_index < 3; ++output_index)
    {
        VS_OUT vertex = input[output_index];
        float2 previous = outward[(output_index + 2) % 3];
        float2 next = outward[output_index];
        // Half a cell of conservative coverage on both incident edges.
        float2 offset = 0.5 * (previous + next) /
            max(1.0 + dot(previous, next), 0.25);
        offset *= min(1.0, 1.5 / max(length(offset), 1e-5));
        float2 expanded = projected[output_index] + offset;
        float2 world_offset = offset / (resolution *
            (dominant == 0 ? inv_extent.yz :
                (dominant == 1 ? inv_extent.xz : inv_extent.xy)));
        if (dominant == 0) vertex.w_position.yz += world_offset;
        else if (dominant == 1) vertex.w_position.xz += world_offset;
        else vertex.w_position.xy += world_offset;
        vertex.position = float4(expanded.x * (2.0 / resolution) - 1.0,
            1.0 - expanded.y * (2.0 / resolution), 0.5, 1);
        output.Append(vertex);
    }
    output.RestartStrip();
}
