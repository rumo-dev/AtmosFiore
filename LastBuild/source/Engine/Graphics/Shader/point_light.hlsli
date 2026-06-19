struct PointLight_GPU
{
    float3 position;
    float radius;

    float3 color;
    float intensity;
};

StructuredBuffer<PointLight_GPU> pointLights : register(t7);
cbuffer CB_PointLightCount : register(b4)
{
    uint numPointLights;
    uint padding[3];
};
