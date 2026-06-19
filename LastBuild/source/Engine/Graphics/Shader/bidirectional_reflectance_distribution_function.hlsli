// UNIT.38
#ifndef __BIDIRECTIONAL_REFLECTANCE_DISTRIBUTION_FUNCTION_HLSL__
#define __BIDIRECTIONAL_REFLECTANCE_DISTRIBUTION_FUNCTION_HLSL__

//UNIT.39
#include "image_based_lighting.hlsli"

// Fresnel
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// https://github.com/wdas/brdf/tree/master/src/brdfs
// https://google.github.io/filament/Filament.md.html
// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
float3 f_schlick(float3 f0, float3 f90, float VoH)
{
    return f0 + (f90 - f0) * pow(clamp(1.0 - VoH, 0.0, 1.0), 5.0);
}
// Smith Joint GGX
// Note: Vis = G / (4 * NoL * NoV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// see Real-Time Rendering. Page 331 to 336.
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float v_ggx(float NoL, float NoV, float alpha_roughness)
{
    float alpha_roughness_sq = alpha_roughness * alpha_roughness;

    float ggxv = NoL * sqrt(NoV * NoV * (1.0 - alpha_roughness_sq) + alpha_roughness_sq);
    float ggxl = NoV * sqrt(NoL * NoL * (1.0 - alpha_roughness_sq) + alpha_roughness_sq);

    float ggx = ggxv + ggxl;
    return (ggx > 0.0) ? 0.5 / ggx : 0.0;
}
// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float d_ggx(float NoH, float alpha_roughness)
{
    const float PI = 3.14159265358979;
    float alpha_roughness_sq = alpha_roughness * alpha_roughness;
    float f = (NoH * NoH) * (alpha_roughness_sq - 1.0) + 1.0;
    return alpha_roughness_sq / (PI * f * f);
}

//  https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
float3 brdf_specular_ggx(float3 f0, float3 f90, float alpha_roughness, float VoH, float NoL, float NoV, float NoH)
{
    float3 F = f_schlick(f0, f90, VoH);
    float Vis = v_ggx(NoL, NoV, alpha_roughness);
    float D = d_ggx(NoH, alpha_roughness);

    return F * Vis * D;
}

//https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
float3 brdf_lambertian(float3 f0, float3 f90, float3 diffuse_color, float VoH)
{
    const float PI = 3.14159265358979;
	// see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
    return (1.0 - f_schlick(f0, f90, VoH)) * (diffuse_color / PI);
}


// UNIT.39
float3 ibl_radiance_lambertian(float3 N, float3 V, float roughness, float3 diffuse_color, float3 f0)
{
    float NoV = clamp(dot(N, V), 0.0, 1.0);

    float2 brdf_sample_point = clamp(float2(NoV, roughness), 0.0, 1.0);
    float2 f_ab = sample_lut_ggx(brdf_sample_point).rg;

    float3 irradiance = sample_diffuse_iem(N).rgb;

	// see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
	// Roughness dependent fresnel, from Fdez-Aguera
    float3 fr = max(1.0 - roughness, f0) - f0;
    float3 k_s = f0 + fr * pow(1.0 - NoV, 5.0);
    float3 fss_ess = k_s * f_ab.x + f_ab.y; // <--- GGX / specular light contribution (scale it down if the specularWeight is low)

	// Multiple scattering, from Fdez-Aguera
    float ems = (1.0 - (f_ab.x + f_ab.y));
    float3 f_avg = (f0 + (1.0 - f0) / 21.0);
    float3 fms_ems = ems * fss_ess * f_avg / (1.0 - f_avg * ems);
    float3 k_d = diffuse_color * (1.0 - fss_ess + fms_ems); // we use +FmsEms as indicated by the formula in the blog post (might be a typo in the implementation)

    return (fms_ems + k_d) * irradiance;
}
float3 ibl_radiance_ggx(float3 N, float3 V, float roughness, float3 f0)
{
    float NoV = clamp(dot(N, V), 0.0, 1.0);

    float2 brdf_sample_point = clamp(float2(NoV, roughness), 0.0, 1.0);
    float2 f_ab = sample_lut_ggx(brdf_sample_point).rg;

    float3 R = normalize(reflect(-V, N));
    float3 specular_light = sample_specular_pmrem(R, roughness).rgb;

	// see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
	// Roughness dependent fresnel, from Fdez-Aguera
    float3 fr = max(1.0 - roughness, f0) - f0;
    float3 k_s = f0 + fr * pow(1.0 - NoV, 5.0);
    float3 fss_ess = k_s * f_ab.x + f_ab.y;

    return specular_light * fss_ess;
}

#endif // __BIDIRECTIONAL_REFLECTANCE_DISTRIBUTION_FUNCTION_HLSL__