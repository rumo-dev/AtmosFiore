#include "constants.hlsli"
#include "samplers.hlsli"

cbuffer SkyLook : register(b8)
{
    float gTime;
    float art_strength;
    float directional_strength;
    float fog_blend;
    float4 zenith_tint;
    float4 horizon_tint;
    float4 directional_tint;
    float4 fog_tint;
    float4 cloud_tint;
    float4 paint_direction_vignette;
    float4 cloud_controls;
};

Texture2D<float4> atmosphere_lut : register(t2);
Texture3D<float4> cloud_base_noise : register(t3);
Texture3D<float4> cloud_detail_noise : register(t4);
Texture2D<float4> transmittance_lut : register(t5);
Texture2D<float4> multiscattering_lut : register(t6);

struct PS_IN
{
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD0;
    float2 ndc : TEXCOORD1;
};

float star_hash(float3 cell)
{
    return frac(sin(dot(cell, float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
}

float4 main(PS_IN input) : SV_TARGET
{
    float3 ray = normalize(input.direction);
    float3 sun_to = normalize(-directional_light_direction.xyz);
    float sun_elevation = sun_to.y;
    float4 scatter = atmosphere_lut.SampleLevel(sampler_states[LINEAR_CLAMP],
        float2(saturate((ray.y + 0.08f) / 1.08f), saturate((sun_elevation + 0.2f) / 1.2f)), 0);
    float sun_cosine = dot(ray, sun_to);
    float rayleigh_phase = 0.75f * (1.0f + sun_cosine * sun_cosine);
    float mie_g = 0.76f;
    float mie_denom = max(1.0f + mie_g * mie_g - 2.0f * mie_g * sun_cosine, 0.01f);
    float mie_phase = 0.25f * (1.0f - mie_g * mie_g) * (1.0f + sun_cosine * sun_cosine) /
        ((2.0f + mie_g * mie_g) * pow(mie_denom, 1.5f));
    float3 sky = scatter.rgb * rayleigh_phase + scatter.a * mie_phase;
    sky += multiscattering_lut.SampleLevel(sampler_states[LINEAR_CLAMP],
        float2(sun_elevation * 0.5f + 0.5f, saturate(camera_position.y / 80000.0f)), 0).rgb;

    float horizon = exp(-max(ray.y, 0.0f) * 3.5f);
    float3 palette = lerp(zenith_tint.rgb, horizon_tint.rgb, horizon);
    float physical_luminance = dot(sky, float3(0.2126f,0.7152f,0.0722f));
    float palette_energy = 0.8f + 1.6f * physical_luminance / (physical_luminance + 2.0f);
    sky = lerp(sky, palette * palette_energy, art_strength);
    float2 horizontal = normalize(ray.xz + float2(0.00001f,0.00001f));
    float2 painted_direction = normalize(paint_direction_vignette.xz + float2(0.00001f,0.00001f));
    float direction_weight = pow(saturate(dot(horizontal, painted_direction)), 4.0f) * horizon;
    sky += directional_tint.rgb * directional_strength * direction_weight * art_strength;
    sky = lerp(sky, fog_tint.rgb * max(dot(sky, float3(0.2126f,0.7152f,0.0722f)),0.08f),
        saturate(fog_blend * horizon * art_strength));

    float daylight = smoothstep(-0.10f, 0.08f, sun_elevation);
    float sun_disc = pow(saturate(sun_cosine), 1400.0f);
    float sun_glow = pow(saturate(sun_cosine), 24.0f);

    float night = 1.0f - daylight;
    float moon_disc = pow(saturate(dot(ray, -sun_to)), 2200.0f);
    sky += float3(0.55f,0.67f,1.0f) * moon_disc * 2.5f * night;
    float3 star_cell = floor(ray * 620.0f);
    float star = smoothstep(0.9987f,0.9999f,star_hash(star_cell));
    sky += float3(0.42f,0.55f,0.95f) * star * night * saturate(ray.y * 3.0f);

    // A scrolling cloud layer is painted on the inside of the skybox.
    float3 cloud_uv = ray * 1.8f + float3(0.5f,0.5f,0.5f) +
        float3(gTime * 0.00008f * cloud_controls.y,0,gTime * 0.00005f * cloud_controls.y);
    float4 base_noise = cloud_base_noise.SampleLevel(sampler_states[LINEAR_WRAP],cloud_uv,0);
    float4 detail_noise = cloud_detail_noise.SampleLevel(sampler_states[LINEAR_WRAP],cloud_uv*2.0f,0);
    float shape = dot(base_noise,float4(0.5f,0.25f,0.15f,0.1f)) -
        dot(detail_noise,float4(0.5f,0.25f,0.15f,0.1f))*0.12f;
    float cloud = smoothstep(0.48f+(0.5f-cloud_controls.x)*0.35f,
        0.62f+(0.5f-cloud_controls.x)*0.35f,shape) * smoothstep(-0.04f,0.13f,ray.y);
    float3 cloud_color = lerp(float3(0.72f,0.77f,0.84f),cloud_tint.rgb,art_strength) *
        (0.8f + 0.30f * daylight + 0.25f * saturate(sun_cosine));
    sky = lerp(sky,cloud_color,cloud*0.78f);
    sky += lerp(float3(1.0f,0.30f,0.08f),float3(1.0f,0.90f,0.70f),saturate(sun_elevation*2.0f)) *
        (sun_disc * 3.0f + sun_glow * 0.15f) * daylight;

    float radius = length(input.ndc * float2(0.82f,1.0f));
    sky *= 1.0f - saturate(paint_direction_vignette.w * art_strength) * smoothstep(0.35f,1.25f,radius);
    return float4(sky,1);
}
