
#ifndef __TONEMAPPING__
#define __TONEMAPPING__

cbuffer ToneMappingConfig : register(b0)
{
    // 0:ACES
    // 1:Reinhard
    // 2:Unreal
    // 3:Neutral
    // 4:Linear
    // 5:Hable
    // 6:AgX
    // 7:GT(Uchimura-like)
    // 8:Drago
    // 9:Exponential
    // 10:Logarithmic
    // 11:Ward
    // 12:Lottes
    // 13:Hejl
    // 14:RomBinDaHouse
    // 15:ReinhardExtended
    // 16:FilmicSimple
    // 17:ACESApprox
    // 18:PBRNeutral
    // 19:Sigmoid
    // 20:Piecewise
    // 21:Cineon
    // 22:Exposure
    // 23:GammaOnly
    // 24:PQApprox
    // 25:HLGApprox
    // 26:OpenDRTLike
    // 27:CameraResponse
    // 28:Uchimura
    // 29:ClampOnly
    // 30:WhitePreservingLuma
    // 31:FilmicALU
    // 32:AgXPunchy
    // 33:CustomCurve
    int mapping_type;

    float exposure;
    float gamma;
    float m_param;
    float max_white;

    float shoulder;
    float linear_strength;
    float linear_angle;
    float toe_strength;

    bool is_enabled;
};

Texture2D<float4> adjusted_hdr_texture : register(t0);

// ------------------------------------------------------------
// Utility
// ------------------------------------------------------------

float GetLuma(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

float3 SafePow(float3 x, float p)
{
    return pow(max(x, 1e-6), p);
}

float3 ApplyGamma(float3 x)
{
    return SafePow(saturate(x), 1.0 / max(gamma, 1e-4));
}

// ------------------------------------------------------------
// Base Tone Mappers
// ------------------------------------------------------------

float3 ToneMap_ACES(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 ToneMap_Reinhard(float3 x)
{
    return x / (1.0 + x);
}

float3 ToneMap_ReinhardExtended(float3 x)
{
    return (x * (1.0 + x / (max_white * max_white))) / (1.0 + x);
}

float3 ToneMap_Unreal(float3 x)
{
    x = max(0.0, x - 0.004);
    return (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
}

float3 ToneMap_Neutral(float3 x)
{
    const float startCompress = 0.8;

    float3 xPass = max(0.0, x - startCompress);
    float3 offset = xPass * xPass * 0.1;

    return x - offset;
}

float3 ToneMap_Hable(float3 x)
{
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;

    return ((x * (A * x + C * B) + D * E) /
           (x * (A * x + B) + D * F)) - E / F;
}

float3 ToneMap_AgX(float3 x)
{
    x = max(0.0, x - 0.01);
    x = x / (x + 0.5);

    return SafePow(x, 1.1);
}

float3 ToneMap_GT(float3 x)
{
    float luma = GetLuma(x);

    float numerator = luma * (luma + 0.0);
    float denominator = luma * (luma + m_param) + 0.1;

    float weight = min(numerator / (denominator + 1e-6), max_white);

    return x * (weight / (luma + 1e-6));
}

// ------------------------------------------------------------
// Additional Tone Mappers
// ------------------------------------------------------------

float3 ToneMap_Drago(float3 x)
{
    float LdMax = max_white;
    float bias = 0.85;

    float3 numerator = log(1.0 + x);
    float3 denominator = log(2.0 + 8.0 * pow(x / max(LdMax, 1e-4), log(bias) / log(0.5)));

    return numerator / max(denominator, 1e-4);
}

float3 ToneMap_Exponential(float3 x)
{
    return 1.0 - exp(-exposure * x);
}

float3 ToneMap_Logarithmic(float3 x)
{
    return log(1.0 + x) / log(1.0 + max_white);
}

float3 ToneMap_Ward(float3 x)
{
    float ldmax = max_white;
    float scale = pow((1.219 + pow(ldmax * 0.5, 0.4)) /
                      (1.219 + pow(GetLuma(x), 0.4)), 2.5);

    return x * scale / ldmax;
}

float3 ToneMap_Lottes(float3 x)
{
    const float a = 1.6;
    const float d = 0.977;
    const float hdrMax = 8.0;
    const float midIn = 0.18;
    const float midOut = 0.267;

    float b = (-pow(midIn, a) + pow(hdrMax, a) * midOut) /
              ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

    float c = (pow(hdrMax, a * d) * pow(midIn, a) -
              pow(hdrMax, a) * pow(midIn, a * d) * midOut) /
              ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

    return pow(x, a) / (pow(x, a * d) * b + c);
}

float3 ToneMap_Hejl(float3 x)
{
    x = max(0.0, x - 0.004);
    return (x * (6.2 * x + 0.5)) /
           (x * (6.2 * x + 1.7) + 0.06);
}

float3 ToneMap_RomBinDaHouse(float3 x)
{
    x = exp(-1.0 / (2.72 * x + 0.15));
    return x;
}

float3 ToneMap_FilmicSimple(float3 x)
{
    x = max(0.0, x - 0.004);
    x = (x * (6.2 * x + 0.5)) /
        (x * (6.2 * x + 1.7) + 0.06);

    return SafePow(x, 2.2);
}

float3 ToneMap_ACESApprox(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return saturate((x * (a * x + b)) /
                    (x * (c * x + d) + e));
}

float3 ToneMap_PBRNeutral(float3 x)
{
    float luma = GetLuma(x);

    float compression = luma / (1.0 + luma);

    return x * (compression / max(luma, 1e-5));
}

float3 ToneMap_Sigmoid(float3 x)
{
    float k = 6.0;
    float midpoint = 0.5;

    return 1.0 / (1.0 + exp(-k * (x - midpoint)));
}

float3 ToneMap_Piecewise(float3 x)
{
    float3 result;

    result = (x < 0.5) ?
        (x * 0.8) :
        (1.0 - exp(-(x - 0.5) * 2.0));

    return result;
}

float3 ToneMap_Cineon(float3 x)
{
    x = max(0.0, x - 0.004);

    x = (x * (x * 6.2 + 0.5)) /
        (x * (x * 6.2 + 1.7) + 0.06);

    return SafePow(x, 2.2);
}

float3 ToneMap_Exposure(float3 x)
{
    return 1.0 - exp(-x * exposure);
}

float3 ToneMap_GammaOnly(float3 x)
{
    return ApplyGamma(x);
}

float3 ToneMap_PQApprox(float3 x)
{
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;

    float3 xm1 = pow(max(x, 0.0), m1);

    return pow((c1 + c2 * xm1) /
               (1.0 + c3 * xm1), m2);
}

float3 ToneMap_HLGApprox(float3 x)
{
    float3 low = sqrt(3.0 * x);
    float3 high = 0.17883277 * log(12.0 * x - 0.28466892) + 0.55991073;

    return (x <= 1.0 / 12.0) ? low : high;
}

float3 ToneMap_OpenDRTLike(float3 x)
{
    x = x / (1.0 + x);
    x = SafePow(x, 1.2);

    return saturate(x);
}

float3 ToneMap_CameraResponse(float3 x)
{
    return pow(x / (1.0 + x), 1.0 / 2.2);
}

float3 ToneMap_Uchimura(float3 x)
{
    const float P = 1.0;
    const float a = 1.0;
    const float m = 0.22;
    const float l = 0.4;
    const float c = 1.33;
    const float b = 0.0;

    float3 l0 = ((P - m) * l) / a;
    float3 L0 = m - m / a;
    float3 L1 = m + (1.0 - m) / a;
    float3 S0 = m + l0;
    float3 S1 = m + a * l0;
    float3 C2 = (a * P) / (P - S1);
    float3 CP = -C2 / P;

    float3 w0 = 1.0 - smoothstep(0.0, m, x);
    float3 w2 = step(m + l0, x);
    float3 w1 = 1.0 - w0 - w2;

    float3 T = m * pow(x / m, c) + b;
    float3 S = P - (P - S1) * exp(CP * (x - S0));
    float3 L = m + a * (x - m);

    return T * w0 + L * w1 + S * w2;
}

float3 ToneMap_ClampOnly(float3 x)
{
    return saturate(x);
}

float3 ToneMap_WhitePreservingLuma(float3 x)
{
    float luma = GetLuma(x);

    float toneMappedLuma = luma / (1.0 + luma);

    return x * toneMappedLuma / max(luma, 1e-5);
}

float3 ToneMap_FilmicALU(float3 x)
{
    x = max(0.0, x - 0.004);

    return (x * (6.2 * x + 0.5)) /
           (x * (6.2 * x + 1.7) + 0.06);
}

float3 ToneMap_AgXPunchy(float3 x)
{
    x = x / (x + 0.6);
    x = SafePow(x, 0.9);

    return saturate(x * 1.1);
}

float3 ToneMap_CustomCurve(float3 x)
{
    float A = shoulder;
    float B = linear_strength;
    float C = linear_angle;
    float D = toe_strength;

    return ((x * (A * x + C * B) + D) /
           (x * (A * x + B) + D));
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

float3 ApplyToneMapping(float3 color)
{
    if (!is_enabled)
        return color;

    color *= exposure;

    switch (mapping_type)
    {
        case 0:
            return ToneMap_ACES(color);
        case 1:
            return ToneMap_Reinhard(color);
        case 2:
            return ToneMap_Unreal(color);
        case 3:
            return ToneMap_Neutral(color);
        case 4:
            return color;
        case 5:
            return ToneMap_Hable(color);
        case 6:
            return ToneMap_AgX(color);
        case 7:
            return ToneMap_GT(color);
        case 8:
            return ToneMap_Drago(color);
        case 9:
            return ToneMap_Exponential(color);
        case 10:
            return ToneMap_Logarithmic(color);
        case 11:
            return ToneMap_Ward(color);
        case 12:
            return ToneMap_Lottes(color);
        case 13:
            return ToneMap_Hejl(color);
        case 14:
            return ToneMap_RomBinDaHouse(color);
        case 15:
            return ToneMap_ReinhardExtended(color);
        case 16:
            return ToneMap_FilmicSimple(color);
        case 17:
            return ToneMap_ACESApprox(color);
        case 18:
            return ToneMap_PBRNeutral(color);
        case 19:
            return ToneMap_Sigmoid(color);
        case 20:
            return ToneMap_Piecewise(color);
        case 21:
            return ToneMap_Cineon(color);
        case 22:
            return ToneMap_Exposure(color);
        case 23:
            return ToneMap_GammaOnly(color);
        case 24:
            return ToneMap_PQApprox(color);
        case 25:
            return ToneMap_HLGApprox(color);
        case 26:
            return ToneMap_OpenDRTLike(color);
        case 27:
            return ToneMap_CameraResponse(color);
        case 28:
            return ToneMap_Uchimura(color);
        case 29:
            return ToneMap_ClampOnly(color);
        case 30:
            return ToneMap_WhitePreservingLuma(color);
        case 31:
            return ToneMap_FilmicALU(color);
        case 32:
            return ToneMap_AgXPunchy(color);
        case 33:
            return ToneMap_CustomCurve(color);

        default:
            return ToneMap_ACES(color);
    }
}

#endif // __TONEMAPPING__