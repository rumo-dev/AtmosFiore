#include "obj_model.hlsli"

Texture2D color_map : register(t0);
Texture2D normal_map : register(t1);
#define	Point_Clamp 0
#define	Point_Wrap 1
#define	Point_Mirror 2
#define	Linear_Clamp 3
#define	Linear_Wrap 4
#define	Linear_Mirror 5
#define	Anisotropic_Clamp 6
#define	Anisotropic_Wrap 7
#define	Anisotropic_Mirror 8

SamplerState sampler_states[9] : register(s0);

//float4 main(VS_OUT pin) : SV_TARGET
//{
//
//    float4 color = color_map.Sample(sampler_states[Anisotropic_Wrap], pin.texcoord);
//    return color;
//    float alpha = color.a;
//    float3 N = normalize(pin.world_normal.xyz);
//#if 1
//    float3 T = float3(1.0001, 0, 0);
//    float3 B = normalize(cross(N, T));
//    T = normalize(cross(B, N));
//    float4 normal = normal_map.Sample(sampler_states[Linear_Wrap], pin.texcoord);
//    normal = (normal * 2.0) - 1.0;
//    normal.w = 0;
//    N = normalize((normal.x * T) + (normal.y * B) + (normal.z * N));
//#endif
//    float3 L = normalize(-directional_light_direction.xyz);
//    float3 diffuse = color.rgb * max(0, dot(N, L));
//    float3 V = normalize(camera_position.xyz - pin.world_position.xyz);
//    float3 specular = pow(max(0, dot(N, normalize(V + L))), 128);
//    return float4(diffuse + specular, alpha) * pin.color;
//}
//

float4 main(VS_OUT pin) : SV_TARGET
{
    // カラーマップのサンプリング
    float4 diffuse_color = color_map.Sample(sampler_states[Anisotropic_Clamp], pin.texcoord);

    // カメラ方向・ライト方向
    float3 E = normalize(pin.world_position.xyz - camera_position.xyz);
    float3 L = normalize(directional_light_direction.xyz);

    // --- ベース法線 ---
    float3 N = normalize(pin.world_normal.xyz);

    // --- 擬似TBNを生成（Tangent/Bitangentを使わない） ---
    float3 T = float3(1.0001, 0, 0);
    float3 B = normalize(cross(N, T));
    T = normalize(cross(B, N));

    // --- ノーマルマップ適用 ---
    float4 normal = normal_map.Sample(sampler_states[Linear_Wrap], pin.texcoord);
    normal = (normal * 2.0f) - 1.0f;
    normal.w = 0;

    // Tangent Space → World Space に変換
    N = normalize((normal.x * T) + (normal.y * B) + (normal.z * N));

    // --- 照明計算 ---
    float3 ambient = ambient_color.rgb * ka.rgb;

    // 拡散反射（Half-Lambert）
    float3 directional_diffuse = ClacHalfLambert(N, L, directional_light_color.rgb, kd.rgb);

    // 鏡面反射（Phong）
    float3 directional_specular = CalcPhongSpecular(N, L, E, directional_light_color.rgb, ks.rgb);

    // リムライト
    float3 rim_color = CalcRimLight(N, E, L, directional_light_color.rgb);

    // 合成
    float4 color = float4(diffuse_color.rgb * (ambient + directional_diffuse), diffuse_color.a);
    color.rgb += directional_specular;
    color.rgb += rim_color;

    return color;
}
