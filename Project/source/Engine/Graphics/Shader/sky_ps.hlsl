#include "constants.hlsli"
#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"

cbuffer PostProcessParams : register(b8)
{
    float gTime;
    float3 _skyPadding;
};

Texture2D<float4> scene_color : register(t0);
Texture2D<float> scene_depth : register(t1);
Texture2D<float4> atmosphere_lut : register(t2);
Texture3D<float4> cloud_base_noise : register(t3);
Texture3D<float4> cloud_detail_noise : register(t4);
Texture2D<float4> transmittance_lut : register(t5);
Texture2D<float4> multiscattering_lut : register(t6);
Texture3D<float4> aerial_perspective_lut : register(t7);

float cloud_density(float3 p, float layer_height)
{
    float4 base_noise = cloud_base_noise.SampleLevel(sampler_states[LINEAR_WRAP], p, 0);
    float4 detail_noise = cloud_detail_noise.SampleLevel(sampler_states[LINEAR_WRAP], p * 2.0f, 0);
    const float4 octave_weights = float4(0.50f, 0.25f, 0.15f, 0.10f);
    float base_shape = dot(base_noise, octave_weights);
    float detail_shape = dot(detail_noise, octave_weights);
    float base_density = saturate((base_shape - 0.43f) / 0.57f);
    // Erode the cloud boundary with the high-frequency Worley volume. Weight
    // the erosion by inverse base density so details stay concentrated at edges.
    float shaped_density = saturate(base_density - detail_shape * 0.32f * (1.0f - base_density));
    // Fade the cloud body at the bottom and top of the cumulus layer.
    float vertical = smoothstep(0.0f, 0.08f, layer_height) * (1.0f - smoothstep(0.78f, 1.0f, layer_height));
    return shaped_density * vertical;
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float depth_value = scene_depth.Sample(sampler_states[POINT_CLAMP], pin.texcoord).r;
    bool foreground = isReversed ? depth_value > 0.000001f : depth_value < 0.999999f;
    float4 scene = scene_color.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord);

    float2 ndc = pin.texcoord * 2.0f - 1.0f;
    ndc.y = -ndc.y;
    float4 view_pos = mul(float4(ndc, 1.0f, 1.0f), inv_projection);
    view_pos.xyz /= max(view_pos.w, 0.00001f);
    float3 ray = normalize(mul(view_pos.xyz, (float3x3)inv_view));

    float3 sun_to = normalize(-directional_light_direction.xyz); // direction from scene toward sun
    float sun_elevation = sun_to.y;
    float scene_distance = 80000.0f;
    float3 scene_radiance = scene.rgb;
    float elev_uv = saturate((ray.y + 0.08f) / 1.08f);
    float sun_uv = saturate((sun_elevation + 0.2f) / 1.2f);
    float4 sky_scatter = atmosphere_lut.SampleLevel(sampler_states[LINEAR_CLAMP], float2(elev_uv, sun_uv), 0);
    float view_sun_cosine = dot(ray, sun_to);
    float rayleigh_phase = 0.75f * (1.0f + view_sun_cosine * view_sun_cosine);
    const float mie_g = 0.76f;
    float mie_denom = max(1.0f + mie_g * mie_g - 2.0f * mie_g * view_sun_cosine, 0.01f);
    float mie_phase = 0.25f * (1.0f - mie_g * mie_g) * (1.0f + view_sun_cosine * view_sun_cosine) /
        ((2.0f + mie_g * mie_g) * pow(mie_denom, 1.5f));
    float3 sky = sky_scatter.rgb * rayleigh_phase + sky_scatter.a * mie_phase;
    float atmosphere_height = saturate(camera_position.y / 80000.0f);
    float3 multiple_scattering = multiscattering_lut.SampleLevel(
        sampler_states[LINEAR_CLAMP], float2(sun_elevation * 0.5f + 0.5f, atmosphere_height), 0).rgb;
    sky += multiple_scattering * (0.45f + 0.55f * saturate(ray.y * 0.5f + 0.5f));

    float sun_angle = max(dot(ray, sun_to), 0.0f);
    float glow = pow(sun_angle, 24.0f) * smoothstep(-0.16f, 0.08f, sun_elevation);
    float disc = pow(sun_angle, 1400.0f) * smoothstep(-0.05f, 0.10f, sun_elevation);
    float3 sun_tint = lerp(float3(1.0f, 0.24f, 0.055f), float3(1.0f, 0.88f, 0.68f), saturate(sun_elevation * 2.0f));
    sky += sun_tint * (glow * 0.32f + disc * 5.0f);

    // Concentrate the red/orange twilight band toward the sun's azimuth.
    float2 sun_horizontal = sun_to.xz / max(length(sun_to.xz), 0.0001f);
    float2 view_horizontal = ray.xz / max(length(ray.xz), 0.0001f);
    float toward_sun = pow(saturate(dot(view_horizontal, sun_horizontal)), 3.0f);
    float twilight = exp(-pow(sun_elevation / 0.24f, 2.0f));
    float horizon_band = exp(-max(ray.y, 0.0f) * 7.0f);
    sky += float3(0.95f, 0.22f, 0.035f) * toward_sun * twilight * horizon_band;

    if (foreground)
    {
        float4 view_position = mul(float4(ndc, depth_value, 1.0f), inv_projection);
        view_position.xyz /= max(view_position.w, 0.00001f);
        scene_distance = min(length(view_position.xyz), 80000.0f);
        float distance_uv = saturate(scene_distance / 80000.0f);
        float3 aerial_uv = float3(saturate(ray.y * 0.5f + 0.5f), saturate((sun_elevation + 0.2f) / 1.2f), distance_uv);
        float4 aerial = aerial_perspective_lut.SampleLevel(sampler_states[LINEAR_CLAMP], aerial_uv, 0);
        float3 aerial_scattering = aerial.rgb * rayleigh_phase;
        aerial_scattering += multiple_scattering * (1.0f - aerial.a) * 0.15f;
        scene_radiance = scene.rgb * aerial.a + aerial_scattering;
    }

    // Volumetric cumulus deck. The view integral is front-to-back and uses
    // Beer-Lambert transmittance; a short sun-ray integral creates self-shadowing.
    if (abs(ray.y) > 0.005f)
    {
        float cloud_base = 5700.0f;
        float cloud_top = 8000.0f;
        float base_intersection = (cloud_base - camera_position.y) / ray.y;
        float top_intersection = (cloud_top - camera_position.y) / ray.y;
        float t0 = max(min(base_intersection, top_intersection), 0.0f);
        float t1 = max(base_intersection, top_intersection);
        t0 = min(t0, scene_distance);
        t1 = min(max(t1, t0), scene_distance);
        float segment = min(t1 - t0, 18000.0f);
        int view_steps = segment > 0.0f ? 24 : 0;
        float step_length = segment / 24.0f;
        float transmittance = 1.0f;
        float3 cloud_radiance = 0.0f;
        [loop] for (int i = 0; i < view_steps; ++i)
        {
            float t = t0 + (i + 0.5f) * step_length;
            float3 wp = camera_position.xyz + ray * t;
            float layer_height = saturate((wp.y - cloud_base) / (cloud_top - cloud_base));
            // Stretch the volume vertically: the paper's 2.3 km cloud deck
            // should contain tall convective bodies, not rapidly stacked noise bands.
            float3 q = wp * float3(0.00003125f, 0.000018f, 0.00003125f) + float3(-gTime * 0.0002f, 0.0f, gTime * 0.00015f);
            float density = cloud_density(q, layer_height);
            if (density > 0.001f)
            {
                float sun_transmittance = 1.0f;
                const float sun_step = 420.0f;
                [unroll] for (int s = 0; s < 4; ++s)
                {
                    float3 sp = wp + sun_to * (sun_step * (s + 0.5f));
                    float sh = saturate((sp.y - cloud_base) / (cloud_top - cloud_base));
                    float sd = cloud_density(sp * float3(0.00003125f, 0.000018f, 0.00003125f) + float3(-gTime * 0.0002f, 0.0f, gTime * 0.00015f), sh);
                    sun_transmittance *= exp(-sd * sun_step * 0.00032f);
                }
                float extinction = density * step_length * 0.00024f;
                float sample_alpha = 1.0f - exp(-extinction);
                float powder = 1.0f + 0.5f * (1.0f - exp(-2.0f * extinction));
                float phase = 0.65f + 0.35f * pow(saturate(dot(ray, sun_to)), 8.0f);
                float3 ambient = lerp(float3(0.11f, 0.14f, 0.20f), sky * 0.48f, saturate(ray.y * 2.0f));
                float3 sun_air_transmittance = transmittance_lut.SampleLevel(
                    sampler_states[LINEAR_CLAMP], float2(sun_elevation * 0.5f + 0.5f, saturate(wp.y / 80000.0f)), 0).rgb;
                float3 direct = sun_tint * sun_transmittance * sun_air_transmittance * max(sun_elevation, 0.08f) * phase * 3.2f;
                cloud_radiance += (ambient + direct) * powder * sample_alpha * transmittance;
                transmittance *= 1.0f - sample_alpha;
                if (transmittance < 0.015f) break;
            }
        }
        float3 background = foreground ? scene_radiance : sky;
        sky = background * transmittance + cloud_radiance;
    }
    return float4(foreground ? (abs(ray.y) > 0.005f ? sky : scene_radiance) : sky, scene.a);
}
