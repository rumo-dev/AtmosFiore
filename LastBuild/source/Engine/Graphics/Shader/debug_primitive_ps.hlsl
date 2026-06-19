#include "debug_primitive.hlsli"
float4 main(VS_OUT pin) : SV_TARGET
{
    return pin.color;
}
