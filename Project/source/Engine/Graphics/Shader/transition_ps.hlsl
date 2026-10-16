#include "fullscreen_quad.hlsli"

cbuffer TransitionConstants : register(b11)
{
    float progress;
    int transition_type;
    float2 resolution;
    float seed;
    float3 padding;
};

float hash21(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 58.233)) + seed) * 13758.5453);
}

float sdHexagon(float2 p, float radius)
{
    const float3 k = float3(-0.8660254, 0.5, 0.5773503);
    p = abs(p);
    p -= 2.0 * min(dot(k.xy, p), 0.0) * k.xy;
    p -= float2(clamp(p.x, -k.z * radius, k.z * radius), radius);
    return length(p) * sign(p.y);
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float aspect = resolution.x / max(resolution.y, 1.0);
    // FullscreenQuad UVは左上原点。Shadertoyの左下原点・Y上向きへ変換する。
    float2 p = (pin.texcoord - 0.5) * float2(aspect * 2.0, -2.0);
    const float tileRadius = 0.105;
    float2 gridSize = float2(1.7320508 * tileRadius, 1.5 * tileRadius);
    float2 estimate = float2(p.x / gridSize.x, p.y / gridSize.y);
    float mask = 0.0;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 cell = floor(estimate) + float2(x, y);
            float2 center = float2(cell.x * gridSize.x + (fmod(abs(cell.y), 2.0) > 0.5 ? gridSize.x * 0.5 : 0.0), cell.y * gridSize.y);
            float2 local = p - center;
            // centerはY上向き空間なので、UVへ戻すときはYを再反転する。
            float2 uvCell = float2(center.x / (aspect * 2.0) + 0.5, 0.5 - center.y * 0.5);
            float wave = saturate(uvCell.x * 0.62 + uvCell.y * 0.38);
            float start = saturate(wave * 0.66 + hash21(cell) * 0.18 + 0.04);
            float localProgress = saturate((progress - start) / 0.22);
            localProgress = localProgress * localProgress * (3.0 - 2.0 * localProgress);
            float radius = tileRadius * (0.015 + 0.985 * localProgress);
            float d = sdHexagon(local, radius);
            float aa = fwidth(d) * 1.25;
            mask = max(mask, 1.0 - smoothstep(-aa, aa, d));
        }
    }

    float edge = exp(-abs(mask - 0.5) * 16.0) * mask;
    float3 color = lerp(float3(0.0, 0.0, 0.0), float3(0.055, 0.035, 0.014), edge * 0.65);
	return float4(color, mask);
}
