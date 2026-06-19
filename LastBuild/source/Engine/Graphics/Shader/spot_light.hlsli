/**
 * @brief スポットライトGPU構造体
 */
struct SpotLight_GPU
{
    float3 position;
    float radius;

    float3 direction;
    float intensity;

    float3 color;
    float innerAngle;

    float outerAngle;
    float padding[3];
};

StructuredBuffer<SpotLight_GPU> spotLights : register(t8);
cbuffer CB_SpotLightCount : register(b6)
{
    uint numSpotLights;
    uint SLpadding[3];
};
