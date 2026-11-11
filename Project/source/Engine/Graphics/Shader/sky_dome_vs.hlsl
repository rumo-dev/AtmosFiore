#include "constants.hlsli"

struct VS_OUT
{
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD0;
    float2 ndc : TEXCOORD1;
};

VS_OUT main(float3 position : POSITION)
{
    VS_OUT output;
    float3 view_direction = mul(float4(position, 0.0f), view).xyz;
    float4 clip = mul(float4(view_direction, 1.0f), projection);
    output.position = clip;
    output.direction = position;
    output.ndc = clip.xy / max(clip.w, 0.00001f);
    return output;
}
