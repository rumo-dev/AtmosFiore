#include "fullscreen_quad.hlsli"
#include "fog.hlsli"
#include "constants.hlsli"

//====================================================
// Sampler / Texture 定義（ユーザー指定準拠）
//====================================================
#define Point_Clamp        0
#define Point_Wrap         1
#define Point_Mirror       2
#define Linear_Clamp       3
#define Linear_Wrap        4
#define Linear_Mirror      5
#define Anisotropic_Clamp  6
#define Anisotropic_Wrap   7
#define Anisotropic_Mirror 8

SamplerState sampler_states[5] : register(s0);
Texture2D texture_maps[2] : register(t0);


// texture_maps
#define TEX_SCENE_COLOR 0
#define TEX_SCENE_DEPTH 1


//====================================================
// Utility
//====================================================
float LinearizeDepth(float depth)
{
    // 通常Z（逆Zなら式変更）
    float z = depth * 2.0f - 1.0f;
    return view_projection._43 / (z - view_projection._33);
}
float3 ReconstructWorldPos(float2 uv, float depth)
{
    float4 ndc;
    ndc.xy = uv * 2.0f - 1.0f;
    ndc.z = depth * 2.0f - 1.0f;
    ndc.w = 1.0f;

    float4 world = mul(ndc, inv_view_projection);
    return world.xyz / world.w;
}

//==============================================

//====================================================
// Pixel Shader
//====================================================
float4 main(VS_OUT input) : SV_Target
{
    float4 sceneColor =
        texture_maps[TEX_SCENE_COLOR].Sample(
            sampler_states[Linear_Clamp],
            input.texcoord
        );

    if (is_enabled == 0)
        return sceneColor;

    float depth =
        texture_maps[TEX_SCENE_DEPTH].Sample(
            sampler_states[Point_Clamp],
            input.texcoord
        ).r;
    if (depth >= 0.9999)
        return sceneColor; // 空にはフォグを掛けない


    // ワールド座標 & 距離
    float3 worldPos = ReconstructWorldPos(input.texcoord, depth);
    float3 toCamera = camera_position.xyz - worldPos;
    float distance = length(toCamera);
    float3 viewDir = toCamera / max(distance, 1e-4);

    // 距離フォグ

    float fogDist = 1.0f - exp(-distance * FogDensity);
    // 高さフォグ

    float fogHeight =
    1.0f - exp(-(FogHeight - camera_position.y) * HeightDensity);

    float fogFactor = saturate(fogDist + fogHeight);


    float noise = frac(
    sin(dot(worldPos.xz, float2(12.9898, 78.233)))
    * 43758.5453 * Time);

    noise = noise * 2.0f - 1.0f;

    float noisyFogFactor =
    fogFactor * (1.0f + noise * NoiseAmount * fogFactor);
   // return float4(noisyFogFactor, noisyFogFactor, noisyFogFactor, 1);
   // noisyFogFactor = saturate(noisyFogFactor);


    // フォグ色
    float3 fogColor = lerp(
        FogColorNear.rgb,
        FogColorFar.rgb,
        saturate(distance * 0.01)
    );

    //  正しいライト散乱（向き依存バグなし）
    float3 lightDir = normalize(-directional_light_direction.xyz);
    float phase = saturate(dot(viewDir, lightDir));
    float scatter = pow(phase, 8.0) * LightScatter * fogFactor;

    fogColor += directional_light_color.rgb * scatter;
    fogColor += ambient_color.rgb * fogFactor;

    // 合成
    float3 finalColor = lerp(sceneColor.rgb, fogColor, noisyFogFactor * NoiseAmount);
    return float4(finalColor, sceneColor.a);
}


