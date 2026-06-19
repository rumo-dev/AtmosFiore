//// Screen-space ambient occlusion (SSAO)
//// https://learnopengl.com/Advanced-Lighting/SSAO
//// The Alchemy Screen-Space Ambient Obscurance Algorithm
//// https://casual-effects.com/research/McGuire2011AlchemyAO/VV11AlchemyAO.pdf

//#include "fullscreen_quad.hlsli"
//#include "constants.hlsli"
//#include "samplers.hlsli"
//#include "common.hlsli"


//Texture2D<float> scene_depth_texture : register(t0);
//Texture2D<float4> scene_normal_texture : register(t1);
//StructuredBuffer<float3> kernel_points : register(t2);
//StructuredBuffer<float3> noise : register(t3);

//#define ALCHEMY_AO
float main(float4 sv_position : SV_POSITION, float2 texcoord : TEXCOORD) : SV_TARGET
{
//    float3 normal = scene_normal_texture.SampleLevel(sampler_states[LINEAR_BORDER_OPAQUE_BLACK], texcoord, 0).xyz;
//    normal = mul(float4(normal, 0), view).xyz; // from world to view-space

//    float depth = scene_depth_texture.SampleLevel(sampler_states[LINEAR_BORDER_OPAQUE_BLACK], texcoord, 0);
//#if 1
//    if (depth > 0.9999999)
//    {
//        discard;
//    }
//#endif	
//    float4 ndc;
//    ndc.xy = uv_to_ndc(texcoord);
//    ndc.z = depth;
//    ndc.w = 1.0;

	
//	// from ndc to view-space
//    float4 position = mul(ndc, inverse_projection);
//    position /= position.w;
	
//	// TBN is a matrix to transform from tangent to view-space matrix
//    float3 random_vec = noise[(sv_position.x % 4) + 4 * (sv_position.y % 4)]; // Random kernel rotation
//    float3 tangent = normalize(random_vec - normal * dot(random_vec, normal));
//    float3 bitangent = cross(normal, tangent);
//    float3x3 TBN = float3x3(tangent, bitangent, normal);
	
//    const int kernel_size = 64;
	
//    float occlusion = 0.0; // accumulated value
//    for (int kernel = 0; kernel < kernel_size; ++kernel)
//    {
//        float3 sample_position = mul(kernel_points[kernel], TBN); // from tangent to view-space
//        const float radius = screen_space_ambient_occlusion_constants_data.radius;
//        sample_position = position.xyz + sample_position * radius;
		
//		// Find a view-space scene intersection point on the ray.
//        float4 intersection = mul(float4(sample_position, 1.0), projection); // from view to clip-space
//        intersection /= intersection.w; // from clip-space to ndc
//        intersection.z = scene_depth_texture.SampleLevel(sampler_states[LINEAR_BORDER_OPAQUE_WHITE], ndc_to_uv(intersection).xy, 0);
//        intersection = mul(intersection, inverse_projection); // from ndc to view-space
//        intersection /= intersection.w; // perspective divide
		
//		// Alchemy AO
//        float3 v = intersection.xyz - position.xyz;
//        const float beta = screen_space_ambient_occlusion_constants_data.bias; // bias distance
//        const float epsilon = 0.001;
//        occlusion += max(0, dot(normal, v) - position.z * beta) / (dot(v, v) + epsilon);
//    }
	
//    const float sigma = 0.3;
//    occlusion = max(0.0, 1.0 - (2.0 * sigma * occlusion / kernel_size));
	
//    float power = screen_space_ambient_occlusion_constants_data.power;
//    return power > 0.0 ? pow(occlusion, power) : 1.0; // TODO
    return 1.0;
}


