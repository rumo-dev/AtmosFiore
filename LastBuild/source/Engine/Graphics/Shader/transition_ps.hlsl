// transition_ps.hlsl
// Grid-based scene transition effect
// Based on skaplun 3D "Megapolis" https://shadertoy.com/view/MlKBWD
#include "fullscreen_quad.hlsli"

cbuffer TransitionConstants : register(b11)
{
    float progress; // 0.0 to 1.0
    int transition_type; // 0: Transition Out (wipe to black), 1: Transition In (wipe from black)
    float2 resolution; // Screen resolution (width, height)
    float seed; // Random seed
    float3 padding;
};

float rnd(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453123);
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;
    
    // Safety check for resolution to prevent division by zero
    float aspect = (resolution.y > 0.0f) ? (resolution.x / resolution.y) : (16.0f / 9.0f);
    float resY = (resolution.y > 0.0f) ? resolution.y : 720.0f;
    
    // Grid settings
    float cellCountY = 12.0f; // Grid density vertically
    float cellCountX = cellCountY * aspect;
    
    float2 U = uv * float2(cellCountX, cellCountY);
    
    float p = cellCountY / resY; // 1 pixel width in cell units for anti-aliasing
    
    // Generate cell start time based on cell coordinates and random seed
    float cellStartTime = rnd(floor(U) + float2(seed, seed)) * 0.5f; // [0.0, 0.5]
    
    float w = 0.0f;
    float mask = 0.0f;
    
    if (progress > cellStartTime)
    {
        // w starts at 0.25 and grows to 1.0 over a duration of 0.25, starting 0.25 after cellStartTime
        w = 0.25f + 0.75f * smoothstep(0.0f, 0.25f, progress - cellStartTime - 0.25f);
        
        float2 localU = frac(U) - 0.5f;
        float2 edge = smoothstep(p, 0.0f, abs(localU) - w * 0.5f);
        mask = edge.x * edge.y;
    }
    
    // type == 0 (Out): alpha goes from 0.0 (transparent) to 1.0 (opaque black/color)
    // type == 1 (In): alpha goes from 1.0 (opaque black/color) to 0.0 (transparent)
    float alpha = (transition_type == 0) ? mask : (1.0f - mask);
    
    // Return solid color `#09080F` (deep purple/black) with calculated opacity
    return float4(0.035f, 0.031f, 0.059f, alpha);
}
