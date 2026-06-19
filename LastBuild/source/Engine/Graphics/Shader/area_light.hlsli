#include "common.hlsli"

#define LTC_LUT_SIZE 64

//=============================================================================
// Area Light
//=============================================================================
struct AreaLight_GPU
{
    float3 position;
    float width;

    float3 direction;
    float height;

    float3 right;
    float radius;

    uint shape;
    float intensity;
    uint pad1;
    uint pad2;

    float3 color;
    float pad3;
};

#define AREA_LIGHT_SHAPE_RECTANGLE 0
#define AREA_LIGHT_SHAPE_DISK 1
#define AREA_LIGHT_SHAPE_SPHERE 2

StructuredBuffer<AreaLight_GPU> areaLights : register(t9);

cbuffer CB_AreaLightCount : register(b7)
{
    uint numAreaLights;
    uint pad0;
    uint pad1;
    uint pad2;
};

//-----------------------------------------------------------------------------
// Sphere Edge Integral
//-----------------------------------------------------------------------------
float IntegrateEdge(float3 v1, float3 v2)
{
    float3 crossV = cross(v1, v2);
    if (dot(crossV, crossV) < 1e-7)
        return 0.0;

    float x = clamp(dot(v1, v2), -0.9999, 0.9999);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
    float b = 3.4175940 + (4.1616724 + y) * y;

    float theta = (x > 0)
        ? (a / b)
        : (0.5 * rsqrt(max(1.0 - x * x, 1e-6)) - a / b);

    return crossV.z * theta;
}

//-----------------------------------------------------------------------------
// Polygon Integral (可変長対応版: Rectangle/Disk/Sphere共通化用)
//-----------------------------------------------------------------------------
float IntegratePolygon(float3 p[12], int numVertices)
{
    // 各頂点の正規化
    for (int i = 0; i < numVertices; i++)
    {
        float len = length(p[i]);
        if (len < 1e-6)
            return 0.0;
        p[i] /= len;
    }

    float sum = 0.0;
    // 頂点の巻き方向に沿ってエッジ積分
    for (int i = 0; i < numVertices; i++)
    {
        sum += IntegrateEdge(p[i], p[(i + 1) % numVertices]);
    }

    return max(sum, 0.0);
}

//-----------------------------------------------------------------------------
// Sphere Integral (球形の積分 - 受動点に追従するビルボードディスク近似)
//-----------------------------------------------------------------------------
void GenerateSphereVertices(float3 center, float radius, float3 P, out float3 p[12])
{
    float3 toLight = center - P;
    float distToLight = length(toLight);
    if (distToLight < 1e-5)
    {
        [unroll]
        for (int i = 0; i < 12; i++)
            p[i] = float3(0, 0, 0);
        return;
    }
    
    float3 localZ = toLight / distToLight;
    float3 localX = abs(localZ.z) < 0.99 ? normalize(cross(float3(0, 0, 1), localZ)) : normalize(cross(float3(1, 0, 0), localZ));
    float3 localY = cross(localZ, localX);

    [unroll]
    for (int i = 0; i < 12; i++)
    {
        float angle = 2.0 * PI * i / 12.0;
        p[i] = center + (localX * cos(angle) + localY * sin(angle)) * radius;
    }
}

//-----------------------------------------------------------------------------
// Basis
//-----------------------------------------------------------------------------
float3x3 BuildBasis(float3 N, float3 V)
{
    float3 T = V - N * dot(V, N);

    if (dot(T, T) < 1e-6)
    {
        T = abs(N.z) < 0.99
            ? cross(float3(0, 0, 1), N)
            : cross(float3(1, 0, 0), N);
    }
    T = normalize(T);

    float3 B = normalize(cross(N, T));

    return float3x3(T, B, N);
}

//-----------------------------------------------------------------------------
// LTC Approximation (Textureless)
//-----------------------------------------------------------------------------
float3x3 LTC_MatrixInv(float roughness, float NoV)
{
    roughness = max(roughness, 0.04);

    float r2 = roughness * roughness;
    float stretchX = 1.0 / r2;
    float stretchY = 1.0 / roughness;

    return float3x3(
        stretchX, 0.0, 0.0,
        0.0, stretchY, 0.0,
        0.0, 0.0, 1.0
    );
}

//-----------------------------------------------------------------------------
// Evaluate
//-----------------------------------------------------------------------------
float LTC_Evaluate(
    float3 N,
    float3 V,
    float3 P,
    AreaLight_GPU light,
    float3x3 Minv)
{
    float3 D = normalize(light.direction);
    float3x3 basis = BuildBasis(N, V);
    float3x3 TForm = mul(Minv, basis);

    // 共通の最大頂点数で配列を定義 (LTC空間変換用)
    float3 p[12];
    int numVertices = 0;

    if (light.shape == AREA_LIGHT_SHAPE_RECTANGLE)
    {
        float facing = dot(D, normalize(P - light.position));
        if (facing <= 0)
            return 0.0;

        float3 R = normalize(light.right - D * dot(light.right, D));
        float3 U = normalize(cross(R, D));

        float hw = light.width * 0.5;
        float hh = light.height * 0.5;

        // 反時計回りで頂点を定義
        p[0] = light.position - R * hw - U * hh;
        p[1] = light.position + R * hw - U * hh;
        p[2] = light.position + R * hw + U * hh;
        p[3] = light.position - R * hw + U * hh;
        numVertices = 4;
    }
    else if (light.shape == AREA_LIGHT_SHAPE_DISK)
    {
        float facing = dot(D, normalize(P - light.position));
        if (facing <= 0)
            return 0.0;

        // 【修正】ライトの実際の方向 (R, U) からローカルディスクの基底を作る
        float3 R = normalize(light.right - D * dot(light.right, D));
        float3 U = normalize(cross(R, D));

        numVertices = 8;
        [unroll]
        for (int i = 0; i < 8; i++)
        {
            // Rectangleの巻き方向と完全に一致させるため、
            // cos, sin の回転平面をライトの向き (R, U) にバインド
            float angle = 2.0 * PI * i / 8.0;
            p[i] = light.position + (R * cos(angle) + U * sin(angle)) * light.radius;
        }
    }
    else if (light.shape == AREA_LIGHT_SHAPE_SPHERE)
    {
        // 球体はビルボードディスクのワールド頂点を生成
        numVertices = 12;
        GenerateSphereVertices(light.position, light.radius, P, p);
    }

    // すべての形状の頂点を一括で LTC（異方性ガウス）空間へ変換
    for (int i = 0; i < numVertices; i++)
    {
        p[i] -= P;
        p[i] = mul(TForm, p[i]);
    }

    return IntegratePolygon(p, numVertices);
}

//-----------------------------------------------------------------------------
// Diffuse
//-----------------------------------------------------------------------------
float3 LTC_Diffuse(
    float3 N,
    float3 V,
    float3 P,
    AreaLight_GPU light,
    float3 c_diff,
    float3 f0,
    float3 f90)
{
    float integral = LTC_Evaluate(
        N, V, P, light,
        float3x3(
            1, 0, 0,
            0, 1, 0,
            0, 0, 1
        )
    );

    return light.color * light.intensity * integral * c_diff / PI;
}

//-----------------------------------------------------------------------------
// Specular
//-----------------------------------------------------------------------------
float3 LTC_Specular(
    float3 N,
    float3 V,
    float3 P,
    AreaLight_GPU light,
    float roughness,
    float3 f0,
    float3 f90)
{
    float NoV = saturate(dot(N, V));

    float3x3 Minv = LTC_MatrixInv(roughness, NoV);

    float integral = LTC_Evaluate(N, V, P, light, Minv);

    float ltcNorm = 1.0 / (Minv[0][0] * Minv[1][1]);
    integral *= ltcNorm;

    float3 F = f0 + (f90 - f0) * pow(1.0 - NoV, 5.0);

    return light.color * light.intensity * integral * F;
}