#ifndef __CONSTANS__
#define __CONSTANS__
//cbuffer SCENE_CONSTANT_BUFFER : register(b1)
//{
    //row_major float4x4 view_projection;
   // row_major float4x4 inv_view_projection;
    //row_major float4x4 light_view_projection;
//};

cbuffer LIGHT_CONSTANT_BUFFER : register(b2)
{
    float4 ambient_color;
    float4 directional_light_direction;
    float4 directional_light_color;
};
cbuffer CAMERA_CONSTANT_BUFFER : register(b1)
{
    float4 camera_position;
    row_major float4x4 view_projection; //ビュー・プロジェクション変換行列;
    row_major float4x4 inv_view_projection; //ビュー・プロジェクション変換行列;
    row_major float4x4 view; //ビュー変換行列;
    row_major float4x4 projection; //プロジェクション変換行列;
    row_major float4x4 inv_view; //ビュー変換行列;
    row_major float4x4 inv_projection; //プロジェクション変換行列;
    row_major float4x4 light_view_projection; // ライトのビュー・プロジェクション行列（シャドウマッピング用）;
    
    
};
#endif