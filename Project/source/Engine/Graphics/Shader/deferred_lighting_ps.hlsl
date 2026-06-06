#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"
#include "bidirectional_reflectance_distribution_function.hlsli"
#include "shadow_directional.hlsli"
#include "point_light.hlsli"
#include "shadow_point.hlsli"
#include "spot_light.hlsli"
#include "shadow_spot.hlsli"
#include "area_light.hlsli"
#include "shadow_area.hlsli"

Texture2D<float4> gbuffer0 : register(t0);
Texture2D<float4> gbuffer1 : register(t1);
Texture2D<float4> gbuffer2 : register(t2);
Texture2D<float4> gbuffer3 : register(t3);
Texture2D<float> point_shadow_front_map : register(t4);
Texture2D<float> point_shadow_back_map : register(t5);
Texture2D<float> directional_shadow_map : register(t6);



float4 main(VS_OUT pin) : SV_TARGET
{
    float4 sampled = gbuffer0.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float3 normal = normalize(sampled.xyz);
    //float3 normal = sampled.xyz;
    float roughness = sampled.w;

    sampled = gbuffer1.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float3 basecolor = sampled.xyz;
    float metallic = sampled.w;

    sampled = gbuffer2.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float3 position = sampled.xyz;
    float occlusion = sampled.w;

    sampled = gbuffer3.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float3 emissive = sampled.xyz;

    const float3 f0 = lerp(0.04f, basecolor, metallic);
    const float3 f90 = 1.0f;
    const float alpha_roughness = roughness * roughness;
    const float3 c_diff = lerp(basecolor, 0.0f, metallic);

    const float3 N = normal;
    const float3 V = normalize(camera_position.xyz - position);
    const float NoV = max(0.0f, dot(N, V));

    //float3 diffuse = ambient_color.rgb * c_diff;
    float3 diffuse = 0;
    float3 specular = 0.0f;

    float3 L = normalize(-directional_light_direction.xyz);
    float3 Li = directional_light_color.rgb * 5;
    float NoL = max(0.0f, dot(N, L));
    if (NoL > 0.0f)
    {
        float3 H = normalize(V + L);
        float NoH = max(0.0f, dot(N, H));
        float HoV = max(0.0f, dot(H, V));
        float directional_shadow = SampleDirectionalShadow(directional_shadow_map, position, N, L);
        

        diffuse += Li * directional_shadow * NoL * brdf_lambertian(f0, f90, c_diff, HoV);
        specular += Li * directional_shadow * NoL * brdf_specular_ggx(f0, f90, alpha_roughness, HoV, NoL, NoV, NoH);
    }

    for (int i = 0; i < numPointLights; ++i)
    {
        if (pointLights[i].intensity <= 0.0f || pointLights[i].radius <= 0.0f)
        {
            continue;
        }

        float3 light_vector = pointLights[i].position - position;
        float dist = length(light_vector);
        if (dist >= pointLights[i].radius)
        {
            continue;
        }

        float3 Lp = light_vector / max(dist, 0.0001f);
        float pointNoL = max(0.0f, dot(N, Lp));
        if (pointNoL <= 0.0f)
        {
            continue;
        }

        float attenuation = pow(max(0.0f, 1.0f - (dist / pointLights[i].radius)), 2.0f);
        float3 pointLi = pointLights[i].color * pointLights[i].intensity * attenuation;
        float3 H = normalize(V + Lp);
        float NoH = max(0.0f, dot(N, H));
        float HoV = max(0.0f, dot(H, V));
        float point_shadow = SamplePointLightShadow(point_shadow_front_map, point_shadow_back_map, position, N, Lp, i);

        diffuse += pointLi * point_shadow * pointNoL * brdf_lambertian(f0, f90, c_diff, HoV);
        specular += pointLi * point_shadow * pointNoL * brdf_specular_ggx(f0, f90, alpha_roughness, HoV, pointNoL, NoV, NoH);
    }

    // スポットライト
    for (int i = 0; i < numSpotLights; ++i)
    {
        if (spotLights[i].intensity <= 0.0f || spotLights[i].radius <= 0.0f)
        {
            continue;
        }

        float3 light_vector = spotLights[i].position - position;
        float dist = length(light_vector);
        if (dist >= spotLights[i].radius)
        {
            continue;
        }

        float3 Ls = light_vector / max(dist, 0.0001f);
        float spotNoL = max(0.0f, dot(N, Ls));
        if (spotNoL <= 0.0f)
        {
            continue;
        }

        // 角度減衰の計算
        float3 lightDir = normalize(spotLights[i].direction);
        float cosAngle = dot(-Ls, lightDir);
        float cosInner = cos(spotLights[i].innerAngle);
        float cosOuter = cos(spotLights[i].outerAngle);
        
        float spotAttenuation = 1.0f;
        if (cosAngle < cosOuter)
        {
            spotAttenuation = 0.0f;
        }
        else if (cosAngle < cosInner)
        {
            spotAttenuation = smoothstep(cosOuter, cosInner, cosAngle);
        }

        float distanceAttenuation = pow(max(0.0f, 1.0f - (dist / spotLights[i].radius)), 2.0f);
        float totalAttenuation = spotAttenuation * distanceAttenuation;
        
        if (totalAttenuation <= 0.0f)
        {
            continue;
        }

        float3 spotLi = spotLights[i].color * spotLights[i].intensity * totalAttenuation;
        float3 H = normalize(V + Ls);
        float NoH = max(0.0f, dot(N, H));
        float HoV = max(0.0f, dot(H, V));

        diffuse += spotLi * spotNoL * brdf_lambertian(f0, f90, c_diff, HoV);
        specular += spotLi * spotNoL * brdf_specular_ggx(f0, f90, alpha_roughness, HoV, spotNoL, NoV, NoH);
    }

   // エリアライト (LTC - Linearly Transformed Cosines)
// Area Light
    for (int i = 0; i < numAreaLights; ++i)
    {
        if (areaLights[i].intensity <= 0)
            continue;


    // 面の向きだけ判定
        if (dot(N, -normalize(areaLights[i].direction)) <= 0)
            continue;

        float3 ltcDiffuse = LTC_Diffuse(N, V, position, areaLights[i], c_diff, f0, f90);

        float3 ltcSpecular = LTC_Specular(N, V, position, areaLights[i], roughness, f0, f90);


    // Directional と揃える
        const float exposure = 5.0;


        diffuse += ltcDiffuse * exposure;


        specular += ltcSpecular * exposure;
    }

    diffuse += ibl_radiance_lambertian(N, V, roughness, c_diff, f0) * 0.2;
    specular += ibl_radiance_ggx(N, V, roughness, f0) * 0.2;

    diffuse *= occlusion;
    specular *= occlusion;

    float3 Lo = diffuse + specular + emissive;
    return float4(Lo, 1.0f);
}
//todo順応
