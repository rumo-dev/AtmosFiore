// Smooth neighboring probes after the raw temporal update. Keep the raw field
// separate so the temporal history never accumulates an already filtered value.
Texture3D<float4> source_voxels : register(t13);

struct Probe
{
    float4 irradiance[6];
    float4 distance[6];
};
StructuredBuffer<Probe> raw_probes : register(t14);
RWStructuredBuffer<Probe> filtered_probes : register(u0);

cbuffer DDGI_CONSTANTS : register(b9)
{
    float4 volume_min;
    float4 inv_extent;
    uint3 probe_count;
    float probe_strength;
    uint frame_index;
    float hysteresis;
    float probe_blend;
    float visibility_bias;
    uint inject_lights;
    float light_multiplier;
    float2 ddgi_padding;
};

uint probe_index(uint3 cell)
{
    return cell.x + probe_count.x * (cell.y + probe_count.y * cell.z);
}

float3 probe_position(uint3 cell)
{
    return volume_min.xyz +
        ((float3(cell) + 0.5) / float3(probe_count)) / inv_extent.xyz;
}

bool clear_between(float3 from, float3 to)
{
    float distance = length(to - from);
    uint steps = min(48u, max(2u, (uint)ceil(distance / (inv_extent.w * 0.75))));
    [loop] for (uint i = 1; i < steps; ++i)
    {
        float3 uvw = (lerp(from, to, (float)i / steps) - volume_min.xyz) * inv_extent.xyz;
        uint3 voxel = min((uint3)max(uvw * 64.0, 0.0), 63u);
        if (source_voxels.Load(int4(voxel, 0)).a >= 0.5) return false;
    }
    return true;
}

[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id >= probe_count)) return;
    uint index = probe_index(id);
    Probe result = raw_probes[index];
    if (result.irradiance[0].w < 0.5)
    {
        filtered_probes[index] = result;
        return;
    }

    float3 center = probe_position(id);
    float3 sums[6];
    [unroll] for (uint l = 0; l < 6; ++l) sums[l] = result.irradiance[l].rgb;
    float total_weight = 1.0;
    [unroll] for (uint neighbor = 0; neighbor < 6; ++neighbor)
    {
        int3 offset = int3(0, 0, 0);
        if (neighbor == 0) offset.x = 1;
        if (neighbor == 1) offset.x = -1;
        if (neighbor == 2) offset.y = 1;
        if (neighbor == 3) offset.y = -1;
        if (neighbor == 4) offset.z = 1;
        if (neighbor == 5) offset.z = -1;
        int3 cell = int3(id) + offset;
        if (any(cell < 0) || any(cell >= int3(probe_count))) continue;
        Probe adjacent = raw_probes[probe_index(uint3(cell))];
        if (adjacent.irradiance[0].w < 0.5) continue;
        if (!clear_between(center, probe_position(uint3(cell)))) continue;
        const float neighbor_weight = 0.35;
        [unroll] for (uint lobe = 0; lobe < 6; ++lobe)
            sums[lobe] += adjacent.irradiance[lobe].rgb * neighbor_weight;
        total_weight += neighbor_weight;
    }
    [unroll] for (uint output_lobe = 0; output_lobe < 6; ++output_lobe)
        result.irradiance[output_lobe].rgb = sums[output_lobe] / total_weight;
    filtered_probes[index] = result;
}
