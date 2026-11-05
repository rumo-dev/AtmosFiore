// Fused horizontal and vertical Gaussian blur for an 8x8 output tile.
// The weights are the discrete equivalent of the linear-sampled PS kernel.
Texture2D<float4> source_texture : register(t0);
RWTexture2D<float4> output_texture : register(u0);

groupshared float4 tile[16 * 16];
groupshared float4 horizontal[16 * 8];

float kernel(uint distance)
{
    if (distance == 0) return 0.2270270270f;
    if (distance == 1) return 0.1945945946f;
    if (distance == 2) return 0.1216216216f;
    if (distance == 3) return 0.0540540541f;
    return 0.0162162162f;
}

[numthreads(8, 8, 1)]
void main(uint3 group_id : SV_GroupID, uint3 local_id : SV_GroupThreadID,
    uint3 dispatch_id : SV_DispatchThreadID)
{
    uint width, height;
    source_texture.GetDimensions(width, height);
    int2 tile_origin = int2(group_id.xy * 8) - 4;

    // Each thread brings four texels into the 16x16 tile, including a
    // four-pixel halo for both filter directions.
    [unroll]
    for (uint y_part = 0; y_part < 2; ++y_part)
    {
        [unroll]
        for (uint x_part = 0; x_part < 2; ++x_part)
        {
            uint2 local = local_id.xy + uint2(x_part * 8, y_part * 8);
            int2 source = clamp(tile_origin + int2(local), int2(0, 0),
                int2(width - 1, height - 1));
            tile[local.y * 16 + local.x] = source_texture.Load(int3(source, 0));
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // Horizontal results cover all 16 rows needed by the vertical pass.
    [unroll]
    for (uint row_part = 0; row_part < 2; ++row_part)
    {
        uint row = local_id.y + row_part * 8;
        uint center = row * 16 + local_id.x + 4;
        float4 value = tile[center] * kernel(0);
        [unroll]
        for (uint offset = 1; offset <= 4; ++offset)
            value += (tile[center - offset] + tile[center + offset]) * kernel(offset);
        horizontal[row * 8 + local_id.x] = value;
    }
    GroupMemoryBarrierWithGroupSync();

    uint center_row = local_id.y + 4;
    float4 result = horizontal[center_row * 8 + local_id.x] * kernel(0);
    [unroll]
    for (uint offset = 1; offset <= 4; ++offset)
        result += (horizontal[(center_row - offset) * 8 + local_id.x] +
            horizontal[(center_row + offset) * 8 + local_id.x]) * kernel(offset);

    if (dispatch_id.x < width && dispatch_id.y < height)
        output_texture[dispatch_id.xy] = result;
}
