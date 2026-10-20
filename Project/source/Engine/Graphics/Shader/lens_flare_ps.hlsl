#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"

Texture2D<float4> hdr_scene : register(t0);

cbuffer LENS_FLARE_CONSTANT_BUFFER : register(b12)
{
    float flare_threshold;
    float flare_intensity;
    float ghost_spacing;
    float streak_intensity;
};

float3 BrightPass(float2 uv)
{
    float inside = all(uv >= 0.0f) && all(uv <= 1.0f) ? 1.0f : 0.0f;
    float3 color = max(hdr_scene.SampleLevel(sampler_states[LINEAR_CLAMP], saturate(uv), 0).rgb, 0.0f);
    float brightness = max(color.r, max(color.g, color.b));
    float knee = max(flare_threshold * 0.5f, 1.0e-3f);
    float soft = clamp(brightness - flare_threshold + knee, 0.0f, 2.0f * knee);
    soft = soft * soft / (4.0f * knee + 1.0e-4f);
    float contribution = max(brightness - flare_threshold, soft) / max(brightness, 1.0e-4f);
    return color * saturate(contribution) * inside;
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 center = float2(0.5f, 0.5f);
    float2 toward_center = center - pin.texcoord;

    float3 ghosts = 0.0f;
    // Keep ghosts nearly neutral: strong RGB separation reads as chromatic aberration.
    ghosts += BrightPass(pin.texcoord + toward_center * (0.40f * ghost_spacing)) * float3(1.0f, 0.96f, 0.91f) * 0.12f;
    ghosts += BrightPass(pin.texcoord + toward_center * (0.82f * ghost_spacing)) * float3(0.94f, 0.97f, 1.0f) * 0.09f;
    ghosts += BrightPass(pin.texcoord + toward_center * (1.30f * ghost_spacing)) * float3(0.97f, 0.94f, 1.0f) * 0.06f;
    ghosts += BrightPass(pin.texcoord + toward_center * (1.85f * ghost_spacing)) * float3(1.0f, 0.98f, 0.94f) * 0.04f;

    uint width, height;
    hdr_scene.GetDimensions(width, height);
    float2 texel = 1.0f / float2(width, height);
    float3 streak = 0.0f;
    float streak_weight = 0.0f;
    [unroll]
    for (int tap = -8; tap <= 8; ++tap)
    {
        float weight = 1.0f - abs((float)tap) / 9.0f;
        streak += BrightPass(pin.texcoord + float2(texel.x * tap * 4.0f, 0.0f)) * weight;
        streak_weight += weight;
    }
    streak /= max(streak_weight, 1.0e-4f);

    // Local diffusion creates the soft lamp glow seen in real lens flare references.
    float2 glow_step = texel * 5.0f;
    float3 glow = BrightPass(pin.texcoord) * 0.28f;
    glow += BrightPass(pin.texcoord + float2(glow_step.x, 0.0f)) * 0.10f;
    glow += BrightPass(pin.texcoord - float2(glow_step.x, 0.0f)) * 0.10f;
    glow += BrightPass(pin.texcoord + float2(0.0f, glow_step.y)) * 0.10f;
    glow += BrightPass(pin.texcoord - float2(0.0f, glow_step.y)) * 0.10f;
    glow += BrightPass(pin.texcoord + glow_step * float2(0.72f, 0.72f)) * 0.08f;
    glow += BrightPass(pin.texcoord + glow_step * float2(-0.72f, 0.72f)) * 0.08f;
    glow += BrightPass(pin.texcoord + glow_step * float2(0.72f, -0.72f)) * 0.08f;
    glow += BrightPass(pin.texcoord - glow_step * float2(0.72f, 0.72f)) * 0.08f;

    float2 from_center = pin.texcoord - center;
    float radial_fade = saturate(1.0f - length(from_center) * 0.72f);
    // A broad, source-linked halo gives the flare a recognizable lens shape.
    float radius = length(from_center);
    float halo_source_scale = 1.45f;
    float halo_source = BrightPass(center + from_center / halo_source_scale).r;
    float halo_ring = exp(-pow((radius - 0.32f) / 0.035f, 2.0f));
    float3 halo = halo_source * halo_ring * float3(1.0f, 0.96f, 0.88f) * 0.32f;
    float3 flare = (ghosts + streak * streak_intensity * radial_fade + halo + glow * 0.65f) * flare_intensity;
    return float4(flare, 1.0f);
}
