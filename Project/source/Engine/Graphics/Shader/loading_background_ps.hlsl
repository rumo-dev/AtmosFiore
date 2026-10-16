#include "fullscreen_quad.hlsli"

cbuffer LoadingConstants : register(b11)
{
    float2 resolution;
    float time;
    float padding;
};

float sdBox(float2 p, float2 halfSize)
{
    float2 d = abs(p) - halfSize;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float2 rotate2d(float2 p, float angle)
{
    float s = sin(angle), c = cos(angle);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;
    float aspect = resolution.x / max(resolution.y, 1.0);
    // Direct3DのUVは左上原点。Shadertoyと同じY上向きに揃える。
    float2 p = float2(uv.x - 0.5, 0.48 - uv.y) * float2(aspect, 1.0);
    float unit = min(resolution.x, resolution.y) / max(resolution.y, 1.0);
    float3 color = float3(0.008, 0.008, 0.014);
    const float3 palette[4] = {
        float3(0.83, 0.91, 0.08), float3(0.035, 0.87, 0.69),
        float3(0.27, 0.18, 1.0), float3(1.0, 0.14, 0.27)
    };

    [unroll]
    for (int arm = 0; arm < 4; ++arm)
    {
        float baseAngle = time * 1.35 + arm * 1.5707963;
        float2 q = rotate2d(p, -baseAngle);
        float pulse = 0.5 + 0.5 * sin(time * 3.2 + arm * 1.3);
        float radius = (0.13 + 0.07 * pulse) * unit;
        float barWidth = (0.075 + 0.10 * pulse) * unit;
        float barHeight = 0.014 * unit;
        float glow = 0.0;
        float3 armColor = 0.0;

        [unroll]
        for (int trail = 0; trail < 8; ++trail)
        {
            float delay = trail * 0.022;
            float fade = (1.0 - trail / 8.0) * 0.42;
            float pastPulse = 0.5 + 0.5 * sin((time - delay) * 3.2 + arm * 1.3);
            float pastRadius = (0.13 + 0.07 * pastPulse) * unit;
            float pastWidth = (0.075 + 0.10 * pastPulse) * unit;
            float2 barPos = float2(pastRadius, 0.0);
            float barD = sdBox(q - barPos, float2(pastWidth * 0.5, barHeight * 0.5));
            float2 tilePos = float2(pastRadius + pastWidth * 0.43, 0.0);
            float tileD = sdBox(q - tilePos, float2(0.028 * unit, 0.028 * unit));
            float shape = max(exp(-barD * 95.0), exp(-tileD * 110.0)) * fade;
            glow += shape;
            armColor += palette[arm] * shape;
        }

        float2 centerQ = q - float2(radius, 0.0);
        float mainTile = exp(-sdBox(centerQ, float2(0.032 * unit, 0.032 * unit)) * 110.0);
        color += armColor * 0.85 + palette[arm] * mainTile * 0.32;
        color += palette[arm] * exp(-length(q - float2(radius, 0.0)) * 18.0) * 0.045;
    }

    float centerGlow = exp(-length(p) * 28.0);
    color += float3(0.95, 0.72, 0.39) * centerGlow * 0.10;
    return float4(saturate(color), 1.0);
}
