struct DDGIProbe
{
    float4 irradiance[6];
    float4 distance[6];
};
StructuredBuffer<DDGIProbe> ddgi_probes : register(t14);

cbuffer DDGI_CONSTANTS : register(b9)
{
    float4 ddgi_volume_min;
    float4 ddgi_inv_extent;
    uint3 ddgi_probe_count;
    float ddgi_strength;
    uint ddgi_frame_index;
    float ddgi_hysteresis;
    float ddgi_blend;
    float ddgi_visibility_bias;
    uint ddgi_inject_lights;
    float ddgi_light_multiplier;
    float2 ddgi_padding;
    float4 ddgi_voxel_min;
    float4 ddgi_voxel_inv_extent;
    uint ddgi_voxel_resolution;
    float3 ddgi_voxel_padding;
};

float3 ddgi_axis(uint index)
{
    if (index == 0) return float3(1, 0, 0);
    if (index == 1) return float3(-1, 0, 0);
    if (index == 2) return float3(0, 1, 0);
    if (index == 3) return float3(0, -1, 0);
    if (index == 4) return float3(0, 0, 1);
    return float3(0, 0, -1);
}

float4 sample_ddgi(float3 position, float3 normal)
{
    float4 result = float4(0, 0, 0, 0);
    if (ddgi_strength > 0.0f &&
        ddgi_probe_count.x >= 2 && ddgi_probe_count.y >= 2 && ddgi_probe_count.z >= 2)
    {
    // Query from the visible side of the surface, like the voxel cone origin.
    // This avoids interpolating across a thin wall when the surface lies right
    // on the boundary between two probe cells.
    float3 sample_position = position + normal * ddgi_voxel_inv_extent.w * 1.5;
    float3 uvw = (sample_position - ddgi_volume_min.xyz) * ddgi_inv_extent.xyz;
    if (all(uvw >= 0) && all(uvw < 1))
    {
    float3 grid = uvw * float3(ddgi_probe_count) - 0.5;
    int3 base_cell = (int3)floor(grid);
    float3 fraction = frac(grid);
    float3 sum_radiance = 0;
    float sum_weight = 0;
    float sum_base_weight = 0;
    [unroll] for (uint corner = 0; corner < 8; ++corner)
    {
        int3 offset = int3(corner & 1, (corner >> 1) & 1, (corner >> 2) & 1);
        int3 cell = base_cell + offset;
        if (any(cell < 0) || any(cell >= int3(ddgi_probe_count))) continue;
        float3 tri = lerp(1.0 - fraction, fraction, float3(offset));
        float weight = tri.x * tri.y * tri.z;
        float3 probe_position = ddgi_volume_min.xyz +
            ((float3(cell) + 0.5) / float3(ddgi_probe_count)) / ddgi_inv_extent.xyz;
        float3 to_probe = probe_position - sample_position;
        float receiver_distance = length(to_probe);
        float3 probe_direction = -to_probe / max(receiver_distance, 1e-4);
        weight *= 0.1 + 0.9 * saturate(dot(normal, to_probe / max(receiver_distance, 1e-4)));
        sum_base_weight += weight;
        uint index = cell.x + ddgi_probe_count.x * (cell.y + ddgi_probe_count.y * cell.z);
        DDGIProbe probe = ddgi_probes[index];
        float2 moments = 0;
        float moment_weight = 0;
        [unroll] for (uint visibility_lobe = 0; visibility_lobe < 6; ++visibility_lobe)
        {
            float direction_weight = saturate(dot(probe_direction, ddgi_axis(visibility_lobe)));
            float valid_weight = direction_weight * probe.distance[visibility_lobe].w;
            moments += probe.distance[visibility_lobe].xy * valid_weight;
            moment_weight += valid_weight;
        }
        if (moment_weight < 0.5) continue;
        moments /= moment_weight;
        float delta = receiver_distance - ddgi_visibility_bias - moments.x;
        if (delta > 0)
        {
            float variance = max(moments.y - moments.x * moments.x, 0.01);
            weight *= variance / (variance + delta * delta);
        }
        float3 irradiance = 0;
        float normal_weight = 0;
        [unroll] for (uint l = 0; l < 6; ++l)
        {
            float lobe_weight = saturate(dot(normal, ddgi_axis(l)));
            irradiance += probe.irradiance[l].rgb * lobe_weight;
            normal_weight += lobe_weight;
        }
        sum_radiance += irradiance / max(normal_weight, 1e-4) * weight;
        sum_weight += weight;
    }
    // The blend confidence must use the same visibility weights as the
    // irradiance. Renormalizing only the surviving probes while keeping a
    // separately saturated blend factor makes individual probes form bands.
    result = float4(sum_radiance / max(sum_weight, 1e-5),
        saturate(sum_weight / max(sum_base_weight, 1e-5)));
    }
    }
    return result;
}

// Isotropic incident radiance for participating media. Probe irradiance is
// divided by pi to convert the six diffuse lobes to an approximate radiance.
float3 sample_ddgi_fog(float3 position)
{
    float3 uvw = (position - ddgi_volume_min.xyz) * ddgi_inv_extent.xyz;
    float3 result = 0.0;
    if (all(uvw >= 0.0) && all(uvw < 1.0))
    {
    float3 grid = uvw * float3(ddgi_probe_count) - 0.5;
    int3 base_cell = (int3)floor(grid);
    float3 fraction = frac(grid);
    float3 radiance = 0.0;
    float base_weight = 0.0;
    [unroll] for (uint corner = 0; corner < 8; ++corner)
    {
        int3 offset = int3(corner & 1, (corner >> 1) & 1, (corner >> 2) & 1);
        int3 cell = base_cell + offset;
        if (any(cell < 0) || any(cell >= int3(ddgi_probe_count))) continue;
        float3 tri = lerp(1.0 - fraction, fraction, float3(offset));
        float weight = tri.x * tri.y * tri.z;
        base_weight += weight;
        uint index = cell.x + ddgi_probe_count.x * (cell.y + ddgi_probe_count.y * cell.z);
        DDGIProbe probe = ddgi_probes[index];
        if (probe.distance[0].w < 0.5) continue;
        float3 probe_position = ddgi_volume_min.xyz +
            ((float3(cell) + 0.5) / float3(ddgi_probe_count)) / ddgi_inv_extent.xyz;
        float3 from_probe = position - probe_position;
        float receiver_distance = length(from_probe);
        float3 direction = from_probe / max(receiver_distance, 1e-4);
        float2 moments = 0.0;
        float moment_weight = 0.0;
        [unroll] for (uint l = 0; l < 6; ++l)
        {
            float axis_weight = saturate(dot(direction, ddgi_axis(l)));
            moments += probe.distance[l].xy * axis_weight;
            moment_weight += axis_weight;
        }
        moments /= max(moment_weight, 1e-4);
        float delta = max(0.0, receiver_distance - ddgi_visibility_bias - moments.x);
        float variance = max(moments.y - moments.x * moments.x, 0.01);
        float visibility = variance / (variance + delta * delta);
        float3 probe_radiance = 0.0;
        [unroll] for (uint irradiance_lobe = 0; irradiance_lobe < 6; ++irradiance_lobe)
            probe_radiance += probe.irradiance[irradiance_lobe].rgb;
        radiance += probe_radiance * (weight * visibility / (6.0 * 3.14159265));
    }
        result = radiance * ddgi_strength / max(base_weight, 1e-5);
    }
    return result;
}
