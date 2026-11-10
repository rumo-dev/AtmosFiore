  #include "gltf_model.hlsli"
// @VertexInput POSITION float4
// @VertexInput NORMAL float4
// @VertexInput TANGENT float4
// @VertexInput TEXCOORD float2
// @VertexInput JOINTS uint4
// @VertexInput WEIGHTS float4
// @VertexInput TEXCOORD1 float2
// @VertexInput COLOR float4

VS_OUT main(VS_IN vin)
{
    float sigma = vin.tangent.w;

	// UNIT.37
    if (skin > -1)
    {
        row_major float4x4 skin_matrix = vin.weights.x * joint_matrices[vin.joints.x] + vin.weights.y * joint_matrices[vin.joints.y] + vin.weights.z * joint_matrices[vin.joints.z] + vin.weights.w * joint_matrices[vin.joints.w];
        vin.position = mul(float4(vin.position.xyz, 1), skin_matrix);
        vin.normal = normalize(mul(float4(vin.normal.xyz, 0), skin_matrix));
        vin.tangent = normalize(mul(float4(vin.tangent.xyz, 0), skin_matrix));
    }

    VS_OUT vout;

    vin.position.w = 1;
    vout.position = mul(vin.position, mul(world, view_projection));
    vout.w_position = mul(vin.position, world);

    vin.normal.w = 0;
    float3x3 model_linear = (float3x3)world;
    float determinant = dot(model_linear[0], cross(model_linear[1], model_linear[2]));
    float3x3 cofactor = float3x3(cross(model_linear[1], model_linear[2]), cross(model_linear[2], model_linear[0]), cross(model_linear[0], model_linear[1]));
    vout.w_normal = float4(normalize(mul(vin.normal.xyz, cofactor) / (abs(determinant)>1e-10?determinant:1)), 0);

	//float sigma = vin.tangent.w;
    vin.tangent.w = 0;
    vout.w_tangent = normalize(mul(vin.tangent, world));
    vout.w_tangent.w = sigma * (determinant < 0 ? -1 : 1);

    vout.texcoord = vin.texcoord;
    vout.texcoord1 = (vertex_flags & 2) ? vin.texcoord1 : vin.texcoord;
    vout.color = (vertex_flags & 1) ? vin.color : 1.0f.xxxx;

    return vout;
}
