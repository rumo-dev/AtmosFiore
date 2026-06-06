cbuffer FOG_CONSTANT_BUFFER : register(b8)
{
    float4 FogColorNear; // xyz: color, w: unused

    float4 FogColorFar;

    float FogNear;
    float FogFar;
    float FogDensity; // 距離フォグ
    float HeightDensity; // 高さフォグ

    float FogHeight; // 基準高さ

    float LightScatter; // 散乱強度


    float Time;
    float NoiseScale;

    float NoiseAmount;
    int is_enabled;
    float2 pad;
};