  struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};
cbuffer OBJECT_CONSTANT_BUFFER : register(b0)
{
    row_major float4x4 world;
    float4 material_color;
};
cbuffer SCENE_CONSTANT_BUFFER : register(b1)
{
    row_major float4x4 view_projection;
    float4 options;
    float4 camera_position;
};

cbuffer LIGHT_CONSTANT_BUFFER : register(b2)
{
    float4 ambient_color;
    float4 directional_light_direction;
    float4 directional_light_color;
};
