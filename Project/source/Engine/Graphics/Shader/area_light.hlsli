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
    float range;
    uint pad2;

    float3 color;
    float pad3;
};

#define AREA_LIGHT_SHAPE_RECTANGLE 0
#define AREA_LIGHT_SHAPE_DISK 1
#define AREA_LIGHT_SHAPE_SPHERE 2

StructuredBuffer<AreaLight_GPU> areaLights : register(t10);
Texture2D<float4> ltcMatrixLut : register(t11);
Texture2D<float4> ltcAmplitudeLut : register(t12);

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
    float edgeIntegral = 0.0;
    float3 crossV = cross(v1, v2);
    if (dot(crossV, crossV) >= 1e-7)
    {
        float x = clamp(dot(v1, v2), -0.9999, 0.9999);
        float y = abs(x);

        float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
        float b = 3.4175940 + (4.1616724 + y) * y;

        float theta = 0.0;
        if (x > 0.0)
            theta = a / b;
        else
            theta = 0.5 * rsqrt(max(1.0 - x * x, 1e-6)) - a / b;

        edgeIntegral = crossV.z * theta;
    }
    return edgeIntegral;
}

//-----------------------------------------------------------------------------
// Polygon Integral (可変長対応版: Rectangle/Disk/Sphere共通化用)
//-----------------------------------------------------------------------------
float IntegratePolygon(float3 p[12], uint vertexCount)
{
    float polygonIntegral = 0.0;
    if (vertexCount >= 3u)
    {
        bool validPolygon = true;
        // 各頂点の正規化
        for (uint normalizeIndex = 0u; normalizeIndex < vertexCount; ++normalizeIndex)
        {
            float len = length(p[normalizeIndex]);
            if (len < 1e-6)
            {
                validPolygon = false;
                break;
            }
            p[normalizeIndex] /= len;
        }

        float sum = 0.0;
        if (validPolygon)
        {
            // 頂点の巻き方向に沿ってエッジ積分
            for (uint edgeIndex = 0u; edgeIndex < vertexCount; ++edgeIndex)
            {
                sum += IntegrateEdge(p[edgeIndex], p[(edgeIndex + 1u) % vertexCount]);
            }
            polygonIntegral = max(sum, 0.0);
        }
    }

    return polygonIntegral;
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
        for (uint zeroIndex = 0u; zeroIndex < 12u; ++zeroIndex)
            p[zeroIndex] = float3(0, 0, 0);
        return;
    }

    float3 localZ = toLight / distToLight;
    float3 localX = abs(localZ.z) < 0.99 ? normalize(cross(float3(0, 0, 1), localZ)) : normalize(cross(float3(1, 0, 0), localZ));
    float3 localY = cross(localZ, localX);

    [unroll]
    for (uint sphereVertexIndex = 0u; sphereVertexIndex < 12u; ++sphereVertexIndex)
    {
        float angle = 2.0 * PI * sphereVertexIndex / 12.0;
        p[sphereVertexIndex] = center + (localX * cos(angle) + localY * sin(angle)) * radius;
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
// Standard 64x64 LTC fit: roughness and sqrt(1 - NoV).
//-----------------------------------------------------------------------------
void LTC_Lookup(float roughness, float NoV, out float4 matrixData, out float4 amplitudeData)
{
	float2 uv = float2(saturate(roughness), sqrt(saturate(1.0 - NoV)));
	uv = uv * ((LTC_LUT_SIZE - 1.0) / LTC_LUT_SIZE) + (0.5 / LTC_LUT_SIZE);
	matrixData = ltcMatrixLut.SampleLevel(sampler_states[LINEAR_CLAMP], uv, 0);
	amplitudeData = ltcAmplitudeLut.SampleLevel(sampler_states[LINEAR_CLAMP], uv, 0);
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
    float evaluateResult = 0.0;
    float3 D = normalize(light.direction);
    float3x3 basis = BuildBasis(N, V);
    float3x3 TForm = mul(Minv, basis);

    // 共通の最大頂点数で配列を定義 (LTC空間変換用)
    float3 p[12];
    [unroll]
    for (uint initIndex = 0u; initIndex < 12u; ++initIndex)
        p[initIndex] = float3(0.0, 0.0, 0.0);
    uint vertexCount = 0u;

    if (light.shape == AREA_LIGHT_SHAPE_RECTANGLE)
    {
        float facing = dot(D, normalize(P - light.position));
        if (facing > 0.0)
        {
            float3 R = normalize(light.right - D * dot(light.right, D));
            float3 U = normalize(cross(R, D));

            float hw = light.width * 0.5;
            float hh = light.height * 0.5;

            // 反時計回りで頂点を定義
            p[0] = light.position - R * hw - U * hh;
            p[1] = light.position + R * hw - U * hh;
            p[2] = light.position + R * hw + U * hh;
            p[3] = light.position - R * hw + U * hh;
            vertexCount = 4u;
        }
    }
    else if (light.shape == AREA_LIGHT_SHAPE_DISK)
    {
        float facing = dot(D, normalize(P - light.position));
        if (facing > 0.0)
        {
            // 【修正】ライトの実際の方向 (R, U) からローカルディスクの基底を作る
            float3 R = normalize(light.right - D * dot(light.right, D));
            float3 U = normalize(cross(R, D));

            vertexCount = 8u;
            [unroll]
            for (uint diskVertexIndex = 0u; diskVertexIndex < 8u; ++diskVertexIndex)
            {
                // Rectangleの巻き方向と完全に一致させるため、
                // cos, sin の回転平面をライトの向き (R, U) にバインド
                float angle = 2.0 * PI * diskVertexIndex / 8.0;
                p[diskVertexIndex] = light.position + (R * cos(angle) + U * sin(angle)) * light.radius;
            }
        }
    }
    else if (light.shape == AREA_LIGHT_SHAPE_SPHERE)
    {
        // 球体はビルボードディスクのワールド頂点を生成
        vertexCount = 12u;
        GenerateSphereVertices(light.position, light.radius, P, p);
    }

    // すべての形状の頂点を一括で LTC（異方性ガウス）空間へ変換
    for (uint transformIndex = 0u; transformIndex < vertexCount; ++transformIndex)
    {
        p[transformIndex] -= P;
        p[transformIndex] = mul(TForm, p[transformIndex]);
    }

    evaluateResult = IntegratePolygon(p, vertexCount);
    return evaluateResult;
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

    float4 ltc;
    float4 ltcAmplitude;
    LTC_Lookup(roughness, NoV, ltc, ltcAmplitude);
    float3x3 Minv = float3x3(
        ltc.x, 0.0, ltc.z,
        0.0, 1.0, 0.0,
        ltc.y, 0.0, ltc.w);

    float integral = LTC_Evaluate(N, V, P, light, Minv);

    float3 F = f0 * ltcAmplitude.x + (f90 - f0) * ltcAmplitude.y;

    return light.color * light.intensity * integral * F;
}
