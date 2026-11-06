// Progressive voxel-guided diffuse probe update. Eight probe groups are
// refreshed in rotation so the cost is bounded even for a full volume.
Texture3D<float4> source_voxels : register(t13);
Texture3D<float4> source_albedo : register(t15);
Texture3D<float4> source_normal : register(t16);
Texture2D<float> directional_shadow_map : register(t7);
#include "constants.hlsli"
#include "samplers.hlsli"
#include "shadow_directional.hlsli"
#include "point_light.hlsli"
#include "spot_light.hlsli"

struct DDGIAreaLight
{
    float3 position; float width;
    float3 direction; float height;
    float3 right; float radius;
    uint shape; float intensity; float range; uint pad2;
    float3 color; float pad3;
};
StructuredBuffer<DDGIAreaLight> ddgi_area_lights : register(t10);
cbuffer DDGI_AREA_COUNT : register(b7)
{
    uint num_ddgi_area_lights;
    uint3 ddgi_area_padding;
};

struct Probe
{
    float4 irradiance[6];
    float4 distance[6]; // mean, mean squared, unused, validity
};
StructuredBuffer<Probe> previous_probes : register(t14);
RWStructuredBuffer<Probe> updated_probes : register(u0);

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
    float4 voxel_min;
    float4 voxel_inv_extent;
    uint voxel_resolution;
    float3 voxel_padding;
};

static const float3 axes[6] = {
    float3(1, 0, 0), float3(-1, 0, 0),
    float3(0, 1, 0), float3(0, -1, 0),
    float3(0, 0, 1), float3(0, 0, -1)
};
static const uint RAYS_PER_PROBE = 64;

float3 ray_direction(uint index, uint probe_index)
{
    // Each refresh uses a different orientation so temporal accumulation
    // integrates more directions instead of repeatedly seeing the same rays.
    float z = 1.0 - 2.0 * (index + 0.5) / RAYS_PER_PROBE;
    float angle = 2.39996323 * index + 0.37 * (probe_index % 17)
        + 2.39996323 * (frame_index / 8);
    float r = sqrt(max(0.0, 1.0 - z * z));
    return float3(r * cos(angle), z, r * sin(angle));
}

float3 evaluate_lights(float3 position, float3 normal)
{
    float3 sun_direction = normalize(-directional_light_direction.xyz);
    float sun_visibility = SampleDirectionalShadow(
        directional_shadow_map, position, normal, sun_direction);
    float3 incident = directional_light_color.rgb *
        (directional_light_color.w / 683.0) *
        saturate(dot(normal, sun_direction)) * sun_visibility;
    [loop] for (uint i = 0; i < numPointLights; ++i)
    {
        PointLight_GPU light = pointLights[i];
        if (light.radius <= 0 || light.intensity <= 0) continue;
        float3 vector_to_light = light.position - position;
        float distance_sq = dot(vector_to_light, vector_to_light);
        if (distance_sq >= light.radius * light.radius) continue;
        float distance = sqrt(distance_sq);
        float NoL = saturate(dot(normal, vector_to_light / max(distance, 1e-4)));
        float ratio = distance / light.radius;
        float cutoff = saturate(1.0 - ratio * ratio * ratio * ratio);
        incident += light.color * light.intensity * NoL * cutoff * cutoff /
            (683.0 * (distance_sq + 0.04));
    }
    [loop] for (uint s = 0; s < numSpotLights; ++s)
    {
        SpotLight_GPU light = spotLights[s];
        if (light.radius <= 0 || light.intensity <= 0) continue;
        float3 vector_to_light = light.position - position;
        float distance_sq = dot(vector_to_light, vector_to_light);
        if (distance_sq >= light.radius * light.radius) continue;
        float distance = sqrt(distance_sq);
        float3 L = vector_to_light / max(distance, 1e-4);
        float NoL = saturate(dot(normal, L));
        float cone = smoothstep(cos(light.outerAngle), cos(light.innerAngle),
            dot(-L, normalize(light.direction)));
        float ratio = distance / light.radius;
        float cutoff = saturate(1.0 - ratio * ratio * ratio * ratio);
        incident += light.color * light.intensity * NoL * cone * cutoff * cutoff /
            (683.0 * (distance_sq + 0.04));
    }
    [loop] for (uint a = 0; a < num_ddgi_area_lights; ++a)
    {
        DDGIAreaLight light = ddgi_area_lights[a];
        if (light.range <= 0 || light.intensity <= 0) continue;
        float3 vector_to_light = light.position - position;
        float distance_sq = dot(vector_to_light, vector_to_light);
        if (distance_sq >= light.range * light.range) continue;
        float distance = sqrt(distance_sq);
        float3 L = vector_to_light / max(distance, 1e-4);
        float NoL = saturate(dot(normal, L));
        float facing = light.shape == 2 ? 1.0 :
            saturate(dot(-L, normalize(light.direction)));
        float emitter_area = light.shape == 0 ? light.width * light.height :
            3.14159265 * light.radius * light.radius;
        float ratio = distance / light.range;
        float cutoff = saturate(1.0 - ratio * ratio * ratio * ratio);
        incident += light.color * light.intensity * max(emitter_area, 0.01) *
            NoL * facing * cutoff * cutoff /
            (683.0 * (distance_sq + light.radius * light.radius + 0.04));
    }
    return incident;
}

void trace_voxel_ray(float3 origin, float3 direction, out float3 radiance,
    out float distance)
{
    float cell_size = voxel_inv_extent.w;
    float max_distance = max(rcp(inv_extent.x), max(rcp(inv_extent.y), rcp(inv_extent.z))) * 1.5;
    float step_size = cell_size * 0.8;
    radiance = 0;
    distance = max_distance;
    [loop] for (uint step = 1; step <= 96; ++step)
    {
        float travel = step * step_size;
        if (travel >= max_distance) break;
        float3 uvw = (origin + direction * travel - voxel_min.xyz) * voxel_inv_extent.xyz;
        if (any(uvw < 0) || any(uvw >= 1)) break;
        uint3 cell = min((uint3)(uvw * voxel_resolution), voxel_resolution - 1u);
        float4 sample_value = source_voxels.Load(int4(cell, 0));
        if (sample_value.a >= 0.5)
        {
            // Visibility uses the exact occupied cell. Gather radiance from
            // its cone footprint so a nearby thin emitter can illuminate the
            // wall hit by this ray, even when no ray hits the emitter cell.
            float diameter = max(cell_size, travel * 0.75);
            float lod = max(0.0, log2(diameter / cell_size));
            float3 footprint_radiance = source_voxels.SampleLevel(
                sampler_states[LINEAR_CLAMP], uvw, lod).rgb;
            radiance = max(max(sample_value.rgb, footprint_radiance), 0.0);
            if (inject_lights != 0)
            {
                float3 albedo = source_albedo.Load(int4(cell, 0)).rgb;
                float3 normal = source_normal.Load(int4(cell, 0)).xyz;
                normal = dot(normal, normal) > 0.1 ? normalize(normal) : -direction;
                if (dot(normal, -direction) < 0) normal = -normal;
                radiance += albedo * light_multiplier * evaluate_lights(
                    origin + direction * travel, normal) / 3.14159265;
            }
            distance = travel;
            break;
        }
    }
}

[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id >= probe_count)) return;
    uint index = id.x + probe_count.x * (id.y + probe_count.y * id.z);
    Probe result;
    if (index % 8 != frame_index % 8)
    {
        if (frame_index < 8)
        {
            [unroll] for (uint l = 0; l < 6; ++l)
            {
                result.irradiance[l] = 0;
                result.distance[l] = 0;
            }
            updated_probes[index] = result;
        }
        else updated_probes[index] = previous_probes[index];
        return;
    }

    float3 probe_position = volume_min.xyz +
        ((float3(id) + 0.5) / float3(probe_count)) / inv_extent.xyz;
    float3 cell_coord = (probe_position - voxel_min.xyz) * voxel_inv_extent.xyz;
    float occupied = all(cell_coord >= 0.0) && all(cell_coord < 1.0) ?
        source_voxels.Load(int4(min((uint3)(cell_coord * voxel_resolution), voxel_resolution - 1u), 0)).a : 0.0;
    float3 sum_radiance[6];
    float3 sum_moments[6];
    [unroll] for (uint l = 0; l < 6; ++l)
    {
        sum_radiance[l] = 0;
        sum_moments[l] = 0;
    }
    if (occupied < 0.5)
    {
        [loop] for (uint ray = 0; ray < RAYS_PER_PROBE; ++ray)
        {
            float3 direction = ray_direction(ray, index);
            float3 radiance;
            float distance;
            trace_voxel_ray(probe_position, direction, radiance, distance);
            [unroll] for (uint l = 0; l < 6; ++l)
            {
                float weight = saturate(dot(direction, axes[l]));
                sum_radiance[l] += radiance * weight;
                sum_moments[l] += float3(distance, distance * distance, 1) * weight;
            }
        }
    }
    [unroll] for (uint output_lobe = 0; output_lobe < 6; ++output_lobe)
    {
        float weight = max(sum_moments[output_lobe].z, 1e-5);
        float3 irradiance = 3.14159265 * sum_radiance[output_lobe] / weight;
        float2 moments = sum_moments[output_lobe].xy / weight;
        float4 current_irradiance = float4(irradiance, occupied < 0.5 ? 1 : 0);
        float4 current_distance = float4(moments, 0, occupied < 0.5 ? 1 : 0);
        if (frame_index >= 8)
        {
            Probe old_probe = previous_probes[index];
            current_irradiance = lerp(current_irradiance,
                old_probe.irradiance[output_lobe], hysteresis);
            current_distance = lerp(current_distance,
                old_probe.distance[output_lobe], hysteresis);
        }
        result.irradiance[output_lobe] = current_irradiance;
        result.distance[output_lobe] = current_distance;
    }
    updated_probes[index] = result;
}
