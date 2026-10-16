struct PointLight_GPU
{
    float3 position;
    float radius;

    float3 color;
    float intensity;

    int shadow_tile;
    uint3 padding;
};

StructuredBuffer<PointLight_GPU> pointLights : register(t8);
cbuffer CB_PointLightCount : register(b4)
{
    uint numPointLights;
    uint padding[3];
};
