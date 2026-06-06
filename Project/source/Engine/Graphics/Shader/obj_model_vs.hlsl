#include "obj_model.hlsli"

VS_OUT main(float4 position : POSITION, float4 normal : NORMAL, float2 texcoord : TEXCOORD)
{
    VS_OUT vout = (VS_OUT) 0;
    float4 world_position = mul(position, world);
    vout.position = mul(world_position, view_projection);
    vout.world_position = world_position;
    vout.world_normal = normalize(mul(float4(normal.xyz, 0), world));
    vout.texcoord = texcoord;
    return vout;
}
