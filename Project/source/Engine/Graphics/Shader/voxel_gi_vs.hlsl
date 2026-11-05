#include "gltf_model.hlsli"
// @VertexInput POSITION float4
// @VertexInput NORMAL float4
// @VertexInput TANGENT float4
// @VertexInput TEXCOORD float2
// @VertexInput JOINTS uint4
// @VertexInput WEIGHTS float4

VS_OUT main(VS_IN vin)
{
    VS_OUT output;
    output.w_position = mul(float4(vin.position.xyz, 1), world);
    output.w_normal = normalize(mul(float4(vin.normal.xyz, 0), world));
    output.w_tangent = float4(0, 0, 0, 1);
    output.texcoord = vin.texcoord;
    // The geometry shader projects the triangle along its dominant axis.
    output.position = float4(0, 0, 0, 1);
    return output;
}
