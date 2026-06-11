#include "fullscreen_quad.hlsli"
#include "constants.hlsli"
#include "samplers.hlsli"
Texture2D texSceneColor : register(t0); // ライティング済みシーン
Texture2D texGBuffer2 : register(t1); // ワールド座標(xyz)が格納されたGBuffer
float GetCoC(float3 worldPos, float3 camPos)
{
    float dist = length(worldPos - camPos);
    float f = FocalLength * 0.001;
    // 物理ベースの錯乱円計算
    float coc = (f * f) / (FNumber * (dist - f)) * ((dist - FocusDist) / dist);
    return clamp(coc / (SensorSize * 0.001), -1.0, 1.0);
}

// 8サンプルのポアソンディスク分布
static const float2 disk[8] =
{
    float2(0.0, 0.0), float2(0.5, 0.0), float2(0.25, 0.433),
    float2(-0.25, 0.433), float2(-0.5, 0.0), float2(-0.25, -0.433),
    float2(0.25, -0.433), float2(0.0, 0.0)
};

float4 main(VS_OUT pin) : SV_Target
{
    float3 worldPos = texGBuffer2.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord).xyz;
    float centerCoC = GetCoC(worldPos, camera_position.xyz);
    
    float4 color = texSceneColor.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord);
    float4 blurred = 0;
    float totalWeight = 0;

    // 
    [unroll]
    for (int i = 0; i < 8; i++)
    {
        float2 offset = disk[i] * abs(centerCoC) * MaxBlurRadius;
        float2 sampleUV = pin.texcoord + offset;
        
        float3 sampleWorldPos = texGBuffer2.Sample(sampler_states[LINEAR_CLAMP], sampleUV).xyz;
        float sampleCoC = GetCoC(sampleWorldPos, camera_position.xyz);
        
        // リーク防止：手前の被写体が奥を覆うための重み付け
        float weight = (sampleCoC > centerCoC) ? 1.0 : 0.5;
        
        blurred += texSceneColor.Sample(sampler_states[LINEAR_CLAMP], sampleUV) * weight;
        totalWeight += weight;
    }
    
    blurred /= totalWeight;
    
    // CoCに基づき、ボケていない部分とボケた部分を合成
    return lerp(color, blurred, smoothstep(0.0, 0.1, abs(centerCoC)));
}