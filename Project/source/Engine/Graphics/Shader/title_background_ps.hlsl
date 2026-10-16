#include "fullscreen_quad.hlsli"

cbuffer TitleConstants : register(b11)
{
    float2 resolution;
    float time;
    float padding;
};

float hash21(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;
    float aspect = resolution.x / max(resolution.y, 1.0);
    // Direct3DのUVは左上原点。角度・回転方向はY上向きの座標で計算する。
    float2 p = float2(uv.x - 0.5, 0.5 - uv.y) * float2(aspect, 1.0);
    float radius = length(p);
    float angle = atan2(p.y, p.x);

    float flower = 0.5 + 0.5 * cos(angle * 8.0 + sin(time * 0.18 + radius * 2.2) * 0.6);
    float ringWave = 0.5 + 0.5 * cos(radius * 29.0 - time * 0.7 + sin(angle * 5.0) * 1.2);
    float ring = exp(-abs(ringWave - 0.5) * 12.0) * exp(-radius * 1.55);
    float petal = pow(saturate(flower), 8.0) * exp(-abs(radius - (0.19 + 0.05 * sin(angle * 4.0 + time * 0.25))) * 14.0);
    float rays = pow(saturate(0.5 + 0.5 * cos(angle * 28.0 + time * 0.025)), 18.0) * exp(-radius * 1.6);
    float grain = hash21(floor(uv * resolution / 3.0) + floor(time * 8.0)) * 0.012;

    float3 gold = float3(0.67, 0.39, 0.16);
    float3 warm = float3(0.95, 0.72, 0.40);
    float3 color = float3(0.003, 0.002, 0.006);
    color += gold * (ring * 0.075 + rays * 0.035);
    color += warm * (petal * 0.13 + exp(-radius * 18.0) * 0.055);
    color += grain;
    return float4(saturate(color), 1.0);
}
