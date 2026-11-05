#include "fullscreen_quad.hlsli"
#include "constants.hlsli"

Texture3D<float4> voxel_volume : register(t13);

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

float4 main(VS_OUT input) : SV_TARGET
{
    // Construct the ray at a finite view-space depth. This avoids the loss of
    // precision from unprojecting the far plane of the 10 km camera frustum.
    float2 ndc = float2(input.texcoord.x * 2 - 1, 1 - input.texcoord.y * 2);
    float4 view_point = mul(float4(ndc, 0.5, 1), inv_projection);
    float3 view_direction = normalize(view_point.xyz / view_point.w);
    float3 direction = normalize(mul(float4(view_direction, 0), inv_view).xyz);
    float3 origin = camera_position.xyz;

    float3 box_min = volume_min.xyz;
    float3 box_max = box_min + 1.0 / inv_extent.xyz;
    float3 safe_direction = max(abs(direction), 1e-6) *
        (step(0.0, direction) * 2.0 - 1.0);
    float3 t0 = (box_min - origin) / safe_direction;
    float3 t1 = (box_max - origin) / safe_direction;
    float3 entry = min(t0, t1);
    float3 exit = max(t0, t1);
    float t_enter = max(entry.x, max(entry.y, entry.z));
    float t_exit = min(exit.x, min(exit.y, exit.z));

    const float3 background = float3(0.025, 0.035, 0.055);
    if (t_exit < max(t_enter, 0.0)) return float4(background, 1);

    float voxel_size = inv_extent.w;
    float t = max(t_enter, 0.0) + voxel_size * 0.001;
    float3 grid_position = (origin + direction * t - box_min) / voxel_size;
    int3 cell = clamp((int3)floor(grid_position), 0, int(resolution) - 1);
    int3 step_cell = (int3)sign(safe_direction);
    float3 next_boundary = box_min +
        (float3(cell) + max(float3(step_cell), 0.0)) * voxel_size;
    float3 t_max = (next_boundary - origin) / safe_direction;
    float3 t_delta = voxel_size / abs(safe_direction);

    float3 face_normal = float3(0, 0, -sign(direction.z));
    if (entry.x >= entry.y && entry.x >= entry.z)
        face_normal = float3(-sign(direction.x), 0, 0);
    else if (entry.y >= entry.z)
        face_normal = float3(0, -sign(direction.y), 0);

    [loop] for (uint i = 0; i < resolution * 3; ++i)
    {
        float4 value = voxel_volume.Load(int4(cell, 0));
        if (value.a >= 0.5)
        {
            float3 hit = origin + direction * t;
            float3 within_cell = frac((hit - box_min) / voxel_size);
            float2 face_uv = abs(face_normal.x) > 0.5 ? within_cell.yz :
                (abs(face_normal.y) > 0.5 ? within_cell.xz : within_cell.xy);
            float2 edge_distance = min(face_uv, 1 - face_uv);
            float grid_line = min(edge_distance.x, edge_distance.y) < 0.045 ? 0.55 : 1.0;
            // The voxel already stores albedo-weighted outgoing radiance.
            // Face orientation is used only for the grid outline, never as RGB.
            float3 radiance = max(value.rgb, 0);
            float3 mapped = radiance / (1.0 + radiance);
            float3 color = pow(saturate(mapped), 1.0 / 2.2) * grid_line;
            return float4(color, 1);
        }

        if (t_max.x <= t_max.y && t_max.x <= t_max.z)
        {
            t = t_max.x;
            t_max.x += t_delta.x;
            cell.x += step_cell.x;
            face_normal = float3(-step_cell.x, 0, 0);
        }
        else if (t_max.y <= t_max.z)
        {
            t = t_max.y;
            t_max.y += t_delta.y;
            cell.y += step_cell.y;
            face_normal = float3(0, -step_cell.y, 0);
        }
        else
        {
            t = t_max.z;
            t_max.z += t_delta.z;
            cell.z += step_cell.z;
            face_normal = float3(0, 0, -step_cell.z);
        }
        if (t > t_exit || any(cell < 0) || any(cell >= int(resolution))) break;
    }
    return float4(background, 1);
}
