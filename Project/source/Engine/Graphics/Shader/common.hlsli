#ifndef __COMMON_HLSL__
#define __COMMON_HLSL__


#define PI 3.141592653
#define FLT_EPSILON 1.192092896e-07

float max3(float3 v)
{
    return max(max(v.x, v.y), v.z);
}

// Using pow often result to a warning like this
// "pow(f, e) will not work for negative f, use abs(f) or conditionally handle negative values if you expect them"
// PositivePow remove this warning when you know the value is positive and avoid inf/NAN.
float positive_pow(float base, float power)
{
    return pow(max(abs(base), float(FLT_EPSILON)), power);
}

float2 positive_pow(float2 base, float2 power)
{
    return pow(max(abs(base), float2(FLT_EPSILON, FLT_EPSILON)), power);
}

float3 positive_pow(float3 base, float3 power)
{
    return pow(max(abs(base), float3(FLT_EPSILON, FLT_EPSILON, FLT_EPSILON)), power);
}

float4 positive_pow(float4 base, float4 power)
{
    return pow(max(abs(base), float4(FLT_EPSILON, FLT_EPSILON, FLT_EPSILON, FLT_EPSILON)), power);
}

// Gamma decoding / De-gamma
float3 srgb_to_linear(float3 c)
{
#if USE_VERY_FAST_SRGB
    return c * c;
#elif USE_FAST_SRGB
    return c * (c * (c * 0.305306011 + 0.682171111) + 0.012522878);
#else
    float3 lo = c / 12.92;
    float3 hi = positive_pow((c + 0.055) / 1.055, float3(2.4, 2.4, 2.4));
    return (c <= 0.04045) ? lo : hi;
#endif
}
float4 srgb_to_linear(float4 c)
{
    return float4(srgb_to_linear(c.rgb), c.a);
}
// Gamma encoding / Gamma correction
float3 linear_to_srgb(float3 c)
{
#if USE_VERY_FAST_SRGB
    return sqrt(c);
#elif USE_FAST_SRGB
    return max(1.055 * positive_pow(c, 0.416666667) - 0.055, 0.0);
#else
    float3 lo = c * 12.92;
    float3 hi = (positive_pow(c, float3(1.0 / 2.4, 1.0 / 2.4, 1.0 / 2.4)) * 1.055) - 0.055;
    return (c <= 0.0031308) ? lo : hi;
#endif
}
float4 linear_to_srgb(float4 c)
{
    return float4(linear_to_srgb(c.rgb), c.a);
}

// Z buffer to linear 0..1 depth
float linear_01_depth(float z, float near, float far)
{
    return 1.0 / ((1 - far / near) * z + (far / near));
}

void near_far_from_projection(in float4x4 projection, out float near, out float far)
{
    near = -projection._43 / projection._33;
    far = -projection._33 / (1 - projection._33) * near;
}



float2 uv_to_ndc(float2 uv)
{
    float2 ndc;
    ndc.x = 2.0 * uv.x - 1.0;
    ndc.y = 1.0 - 2.0 * uv.y;
    return ndc;
}
float4 ndc_to_uv(float4 ndc)
{
    float4 uv;
    uv.x = 0.5 + 0.5 * ndc.x;
    uv.y = 0.5 - ndc.y * 0.5;
    uv.z = ndc.z;
    uv.w = ndc.w;
    return uv;
}
float4 view_to_uv(float3 pos, column_major float4x4 projection_transform)
{
    float4 ndc = mul(float4(pos, 1.0), projection_transform);
    ndc /= ndc.w;
    return ndc_to_uv(ndc);
}

// Spheremap Transform
// https://aras-p.info/texts/CompactNormalStorage.html
float2 encode_normal(float3 n)
{
    float p = sqrt(n.z * 8 + 8);
    return float2(n.xy / p + 0.5);
}
float3 decode_normal(float2 enc)
{
    float2 fenc = enc * 4 - 2;
    float f = dot(fenc, fenc);
    float g = sqrt(1 - f / 4);
    float3 n;
    n.xy = fenc * g;
    n.z = 1 - f / 2;
    return n;
}


float sq(float t)
{
    return t * t;
}
float2 sq(float2 t)
{
    return t * t;
}
float3 sq(float3 t)
{
    return t * t;
}
float4 sq(float4 t)
{
    return t * t;
}

float apply_ior_to_roughness(float roughness, float ior)
{
    // Scale roughness with IOR so that an IOR of 1.0 results in no microfacet refraction and
    // an IOR of 1.5 results in the default amount of microfacet refraction.
    return roughness * clamp(ior * 2.0 - 2.0, 0.0, 1.0);
}
float3 rgb_mix(float3 base, float3 layer, float3 rgb_alpha)
{
    float rgb_alpha_max = max(rgb_alpha.r, max(rgb_alpha.g, rgb_alpha.b));
    return (1.0 - rgb_alpha_max) * base + rgb_alpha * layer;
}

#endif // __COMMON_HLSL__

