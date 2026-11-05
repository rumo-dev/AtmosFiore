cbuffer OcclusionBox : register(b0)
{
    row_major float4x4 box_world_view_projection;
};

float4 main(uint vertex_id : SV_VertexID) : SV_POSITION
{
    static const uint corners[36] = {
        0, 2, 3, 0, 3, 1,
        4, 5, 7, 4, 7, 6,
        0, 1, 5, 0, 5, 4,
        2, 6, 7, 2, 7, 3,
        0, 4, 6, 0, 6, 2,
        1, 3, 7, 1, 7, 5
    };
    const uint corner = corners[vertex_id];
    const float3 position = float3(
        (corner & 1) ? 0.5f : -0.5f,
        (corner & 2) ? 0.5f : -0.5f,
        (corner & 4) ? 0.5f : -0.5f);
    return mul(float4(position, 1.0f), box_world_view_projection);
}
