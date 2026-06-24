
#include "constants.hlsli"
#include "samplers.hlsli"
#include "fullscreen_quad.hlsli"
//----------------------------------------------------------
// b9 : 露出専用定数バッファ
//----------------------------------------------------------
cbuffer ExposureConstants : register(b9)
{
    int IsEnabled; // 有効フラグ
    float3 pad;
}

Texture2D ColorMap : register(t0);

//----------------------------------------------------------
// T値 → EV100
//   EV100 = log2( T² / shutterSpeed ) - log2( ISO / 100 )
//----------------------------------------------------------
float ComputeEV100(float tStop, float shutterSpeed, float iso)
{
    return log2((tStop * tStop) / max(shutterSpeed, 1e-6f))
         - log2(max(iso, 1.0f) / 100.0f);
}

//----------------------------------------------------------
// EV100 → 線形露出スケール
//   L_max = 1.2 * 2^EV100  (ACES 基準 18% グレー)
//   exposure = 1 / L_max
//----------------------------------------------------------
float EV100ToExposure(float ev100)
{
    return 1.0f / (1.2f * exp2(ev100));
}

//----------------------------------------------------------
// Main
//----------------------------------------------------------
float4 main(VS_OUT pin) : SV_TARGET
{
    float3 hdr = ColorMap.Sample(sampler_states[POINT_CLAMP], pin.texcoord).rgb;

    [branch]
    if (!IsEnabled)
        return float4(hdr, 1.0f); // パススルー

    float ev100 = ComputeEV100(TStop, ShutterSpeed, ISO);
    ev100 += EV_Compensation;
    float exposure = EV100ToExposure(ev100);

    // 線形スケールのみ適用。出力は依然 HDR。
    return float4(hdr * exposure, 1.0f);
}