 #include "gltf_model.hlsli"
#include "bidirectional_reflectance_distribution_function.hlsli"

struct texture_info
{
    int index;
    int texcoord;
};
struct normal_texture_info
{
    int index;
    int texcoord;
    float scale;
};
struct occlusion_texture_info
{
    int index;
    int texcoord;
    float strength;
};
struct pbr_metallic_roughness
{
    float4 basecolor_factor;
    texture_info basecolor_texture;
    float metallic_factor;
    float roughness_factor;
    texture_info metallic_roughness_texture;
};
struct material_constants
{
    float3 emissive_factor;
    int alpha_mode; // "OPAQUE" : 0, "MASK" : 1, "BLEND" : 2
    float alpha_cutoff;
    bool double_sided;

    pbr_metallic_roughness pbr_metallic_roughness;

    normal_texture_info normal_texture;
    occlusion_texture_info occlusion_texture;
    texture_info emissive_texture;
};
StructuredBuffer<material_constants> materials : register(t0);

#define BASECOLOR_TEXTURE 0
#define METALLIC_ROUGHNESS_TEXTURE 1
#define NORMAL_TEXTURE 2
#define EMISSIVE_TEXTURE 3
#define OCCLUSION_TEXTURE 4

Texture2D<float4> material_textures[5] : register(t1);

//#define	Point_Clamp 0
//#define	Point_Wrap 1
//#define	Point_Mirror 2
//#define	Linear_Clamp 3
//#define	Linear_Wrap 4
//#define	Linear_Mirror 5
//#define	Anisotropic_Clamp 6
//#define	Anisotropic_Wrap 7
//#define	Anisotropic_Mirror 8

//SamplerState sampler_states[9] : register(s0);
//#include "../image_based_lighting.hlsli"
float4 CalcFog(in float4 color, float4 fog_color, float2 fog_range, float eye_length)
{
    float fogAlpha = saturate((eye_length - fog_range.x) / (fog_range.y - fog_range.x));
    return lerp(color, fog_color, fogAlpha);
}
struct FogParams
{
    float4 fog_color;
    float2 fog_range;

    float eye_length;
};



float4 main(VS_OUT pin, bool is_front_face : SV_IsFrontFace) : SV_TARGET
{

    const float GAMMA = 2.2;

    const material_constants m = materials[material];

    float4 basecolor_factor = m.pbr_metallic_roughness.basecolor_factor;
    const int basecolor_texture = m.pbr_metallic_roughness.basecolor_texture.index;
    if (basecolor_texture > -1)
    {
        float4 sampled = material_textures[BASECOLOR_TEXTURE].Sample(sampler_states[ANISOTROPIC_WRAP], pin.texcoord);
        sampled.rgb = pow(sampled.rgb, GAMMA);
        basecolor_factor *= sampled;
    }
#if 1
    clip(basecolor_factor.a - 0.25);
#endif

    float3 emmisive_factor = m.emissive_factor;
    const int emissive_texture = m.emissive_texture.index;
    if (emissive_texture > -1)
    {
        float4 sampled = material_textures[EMISSIVE_TEXTURE].Sample(sampler_states[ANISOTROPIC_WRAP], pin.texcoord);
        sampled.rgb = pow(sampled.rgb, GAMMA);
        emmisive_factor *= sampled.rgb;
    }
    float roughness_factor = m.pbr_metallic_roughness.roughness_factor;
    float metallic_factor = m.pbr_metallic_roughness.metallic_factor;
    const int metallic_roughness_texture = m.pbr_metallic_roughness.metallic_roughness_texture.index;
    if (metallic_roughness_texture > -1)
    {
        float4 sampled = material_textures[METALLIC_ROUGHNESS_TEXTURE].Sample(sampler_states[LINEAR_WRAP], pin.texcoord);
        roughness_factor *= sampled.g;
        metallic_factor *= sampled.b;
    }


    float occlusion_factor = 1.0;
    const int occlusion_texture = m.occlusion_texture.index;
    if (occlusion_texture > -1)
    {
        float4 sampled = material_textures[OCCLUSION_TEXTURE].Sample(sampler_states[LINEAR_WRAP], pin.texcoord);
        occlusion_factor *= sampled.r;
    }
    const float occlusion_strength = m.occlusion_texture.strength;

    const float3 f0 = lerp(0.04, basecolor_factor.rgb, metallic_factor);
    const float3 f90 = 1.0;
    const float alpha_roughness = roughness_factor * roughness_factor;
    const float3 c_diff = lerp(basecolor_factor.rgb, 0.0, metallic_factor);

    const float3 P = pin.w_position.xyz;
    const float3 V = normalize(camera_position.xyz - pin.w_position.xyz);

    float3 N = normalize(pin.w_normal.xyz);
    float3 T = has_tangent ? normalize(pin.w_tangent.xyz) : float3(1, 0, 0);
    float sigma = has_tangent ? pin.w_tangent.w : 1.0;
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(cross(N, T) * sigma);
#if 1
	// For a back-facing surface, the tangential basis vectors are negated.
    if (is_front_face == false)
    {
        T = -T;
        B = -B;
        N = -N;
    }
#endif

    const int normal_texture = m.normal_texture.index;
    if (normal_texture > -1)
    {
        float4 sampled = material_textures[NORMAL_TEXTURE].Sample(sampler_states[LINEAR_WRAP], pin.texcoord);
        float3 normal_factor = sampled.xyz;
        normal_factor = (normal_factor * 2.0) - 1.0;
        normal_factor = normalize(normal_factor * float3(m.normal_texture.scale, m.normal_texture.scale, 1.0));
        N = normalize((normal_factor.x * T) + (normal_factor.y * B) + (normal_factor.z * N));

    }
    float3 diffuse = 0;
    float3 specular = 0;

	// Loop for shading process for each light
    float3 L = normalize(-directional_light_direction.xyz);
    float3 Li = float3(1.0, 1.0, 1.0); // Radiance of the light
    const float NoL = max(0.0, dot(N, L));
    const float NoV = max(0.0, dot(N, V));
    if (NoL > 0.0 || NoV > 0.0)
    {
        const float3 R = reflect(-L, N);
        const float3 H = normalize(V + L);
		//return sample_specular_pmrem(R, 0.0);

        const float NoH = max(0.0, dot(N, H));
        const float HoV = max(0.0, dot(H, V));

        diffuse += Li * NoL * brdf_lambertian(f0, f90, c_diff, HoV);
        specular += Li * NoL * brdf_specular_ggx(f0, f90, alpha_roughness, HoV, NoL, NoV, NoH);
    }

	// UNIT.39
    diffuse += ibl_radiance_lambertian(N, V, roughness_factor, c_diff, f0);

    specular += ibl_radiance_ggx(N, V, roughness_factor, f0);
   // return float4(roughness_factor.rrr, 1.0);


    float3 emmisive = emmisive_factor;
    diffuse = lerp(diffuse, diffuse * occlusion_factor, occlusion_strength);
    specular = lerp(specular, specular * occlusion_factor, occlusion_strength);


    float3 Lo = diffuse + specular + emmisive;
//    //ガンマ補正
//    Lo = pow(Lo, 1.0 / GAMMA);
//     // ベースカラーのアルファ
//    float alpha = basecolor_factor.a;
//    FogParams fog;
//    fog.fog_color = float4(0, 0, 0, 0.5f);

//    fog.fog_range.xy = float2(20, 20);
//    fog.eye_length = length(pin.w_position.xyz - camera_position.xyz).x;

//    float dist = fog.eye_length;

//// Exp2 Fog
//    float fogDensity = 0.005; // 濃さ
//    float expFog = 1.0 - exp(-fogDensity * dist * dist);

//// Height Fog
//    float heightFog = saturate((50.0 - pin.w_position.y) / 20.0); // 高さ 30~50 に霧

//// Noise Fog（簡易版worldPos のランダムっぽい揺らぎ）
//    float noise = frac(sin(dot(pin.w_position.xyz, float3(12.9898, 78.233, 45.164))) * 43758.5453);
//    float noiseFog = lerp(1.0, noise, 0.3); // ノイズ影響 30%

//// 合成
//    float fogFactor = saturate(expFog * heightFog * noiseFog);

//// 最終カラー
//    float3 finalColor = lerp(Lo, fog.fog_color.rgb, fogFactor);
//    // フォグ適用（Lo に組み込み）
//   // float4 finalColor = CalcFog(float4(Lo, alpha), fog.fog_color, fog.fog_range, fog.eye_length);




//    return float4(finalColor, basecolor_factor.a);



    return float4(Lo, basecolor_factor.a);
}