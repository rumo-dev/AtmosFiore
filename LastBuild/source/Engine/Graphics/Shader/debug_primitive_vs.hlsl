#include "debug_primitive.hlsli"
VS_OUT main(float4 position : POSITION, float4 normal : NORMAL)
{
    VS_OUT vout;
    vout.position = mul(position, mul(world, view_projection));

    normal.w = 0;
    float4 N = normalize(mul(normal, world));
    float4 L = normalize(-directional_light_direction);

    //vout.color.rgb = material_color.rgb * max(0, dot(L, N));
   // vout.color.a = material_color.a;
    vout.color = material_color;
    return vout;
}
