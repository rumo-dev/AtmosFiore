#include "constants.hlsli"
#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"

// ==========================================================
// Constant Buffers (C++側から毎フレーム更新して渡す)
// ==========================================================
cbuffer PostProcessParams : register(b8)
{
    float gTime; // 経過時間 (iTime)
};

// ==========================================================
// Textures & Samplers
// ==========================================================
Texture2D tex : register(t0); // ShaderToyのiChannel0に相当するノイズテクスチャ
Texture2D depth : register(t1); // ShaderToyのiChannel0に相当するノイズテクスチャ

// ==========================================================
// Macros & Constants (元の数値を維持)
// ==========================================================
#define ORIG_CLOUD 0
#define ENABLE_RAIN 0
#define SIMPLE_SUN 0
#define NICE_HACK_SUN 1
#define SOFT_SUN 1
#define cloudy  1
#define haze    (0.01 * (cloudy * 20.0))
#define rainmulti 5.0
#define rainy   (10.0 - rainmulti)
#define fov     tan(radians(60.0))
#define cameraheight 5e1
#define mincloudheight 5e3
#define maxcloudheight 8e3
#define xaxiscloud (gTime * 5e2)
#define yaxiscloud 0.0
#define zaxiscloud (gTime * 6e2)
#define cloudnoise 2e-4

static const int steps = 16;
static const int stepss = 16;
static const float R0 = 6360e3;
static const float Ra = 6380e3;
static const float I = 10.0;
static const float SI = 5.0;
static const float g = 0.45;
static const float g2 = g * g;
static const float ts = (cameraheight / 2.5e5);
static const float s = 0.999;
static const float s2 = s; // SOFT_SUN = 1を前提
static const float Hr = 8e3;
static const float Hm = 1.2e3;

static const float3 bM = float3(21e-6, 21e-6, 21e-6);
static const float3 bR = float3(5.8e-6, 13.5e-6, 33.1e-6);
static const float3 C = float3(0.0, -R0, 0.0);

static const float cloudnear = 1.0;
static const float cloudfar = 70e3;

// ==========================================================
// Utility Functions
// ==========================================================
float2x2 mm2(float a)
{
    float c = cos(a), s_ = sin(a);
    return float2x2(c, -s_, s_, c); // HLSLは行優先を意識して転置気味に配置
}

float tri(float x)
{
    return clamp(abs(frac(x) - 0.5), 0.01, 0.49);
}

float2 tri2(float2 p)
{
    return float2(tri(p.x) + tri(p.y), tri(p.y + tri(p.x)));
}

float triNoise2d(float2 p, float spd)
{
    float z = 1.8;
    float z2 = 2.5;
    float rz = 0.0;
    p = mul(p, mm2(p.x * 0.06));
    float2 bp = p;
    float2x2 m2 = float2x2(0.95534, -0.29552, 0.29552, 0.95534);
    
    for (float i = 0.0; i < 5.0; i++)
    {
        float2 dg = tri2(bp * 1.85) * 0.75;
        dg = mul(dg, mm2(gTime * spd));
        p -= dg / z2;
        bp *= 1.3;
        z2 *= 1.45;
        z *= 0.42;
        p *= 1.21 + (rz - 1.0) * 0.02;
        rz += tri(p.x + tri(p.y)) * z;
        p = mul(p, -m2);
    }
    return clamp(1.0 / pow(rz * 29.0, 1.3), 0.0, 0.55);
}

float hash21(float2 n)
{
    return frac(sin(dot(n, float2(12.9898, 4.1414))) * 43758.5453);
}

float3 hash33(float3 p)
{
    p = frac(p * float3(443.8975, 397.2973, 491.1871));
    p += dot(p.zxy, p.yxz + 19.27);
    return frac(float3(p.x * p.y, p.z * p.x, p.y * p.z));
}

// ==========================================================
// Core Rendering Functions
// ==========================================================
float4 aurora(float3 ro, float3 rd, float2 fragCoord)
{
    float4 col = float4(0, 0, 0, 0);
    float4 avgCol = float4(0, 0, 0, 0);
    ro *= 1e-5;
    float mt = 10.0;
    
    for (float i = 0.0; i < 5.0; i++)
    {
        float of = 0.006 * hash21(fragCoord) * smoothstep(0.0, 15.0, i * mt);
        float pt = ((0.8 + pow((i * mt), 1.2) * 0.001) - rd.y) / (rd.y * 2.0 + 0.4);
        pt -= of;
        float3 bpos = ro + pt * rd;
        float2 p = bpos.zx;
        float rzt = triNoise2d(p, 0.1);
        float4 col2 = float4(0, 0, 0, rzt);
        col2.rgb = (sin(1.0 - float3(2.15, -0.5, 1.2) + (i * mt) * 0.053) * (0.5 * mt)) * rzt;
        avgCol = lerp(avgCol, col2, 0.5);
        col += avgCol * exp2((-i * mt) * 0.04 - 2.5) * smoothstep(0.0, 5.0, i * mt);
    }
    col *= (clamp(rd.y * 15.0 + 0.4, 0.0, 1.2));
    return col * 2.8;
}



float hash31(float3 p)
{
    p = frac(p * float3(123.34, 456.21, 789.31));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y * p.z);
}

// --- 2Dノイズ（テクスチャ不要版） ---
float noise(float2 v)
{
    float2 p = floor(v);
    float2 f = frac(v);
    f = f * f * (3.0 - 2.0 * f); // スムーズ補間（元のコードと同じ）
    
    // 周辺4点のランダム値を取得
    float v00 = hash21(p + float2(0, 0));
    float v10 = hash21(p + float2(1, 0));
    float v01 = hash21(p + float2(0, 1));
    float v11 = hash21(p + float2(1, 1));
    
    // 線形補間
    return lerp(lerp(v00, v10, f.x), lerp(v01, v11, f.x), f.y);
}

// --- 3Dノイズ（テクスチャ不要版） ---
float _Noise(float3 x)
{
    float3 p = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0 - 2.0 * f);

    // 3次元格子上の8点のランダム値を取得
    float v000 = hash31(p + float3(0, 0, 0));
    float v100 = hash31(p + float3(1, 0, 0));
    float v010 = hash31(p + float3(0, 1, 0));
    float v110 = hash31(p + float3(1, 1, 0));
    float v001 = hash31(p + float3(0, 0, 1));
    float v101 = hash31(p + float3(1, 0, 1));
    float v011 = hash31(p + float3(0, 1, 1));
    float v111 = hash31(p + float3(1, 1, 1));

    // 8点を3次元的に補間
    return lerp(
        lerp(lerp(v000, v100, f.x), lerp(v010, v110, f.x), f.y),
        lerp(lerp(v001, v101, f.x), lerp(v011, v111, f.x), f.y),
        f.z
    );
}

float fnoise(float3 p, float t)
{
    p *= 0.25;
    float f = 0.0;
    f += 0.5000 * _Noise(p);
    p = p * 3.02;
    p.y -= t * 0.1;
    f += 0.2500 * _Noise(p);
    p = p * 3.03;
    p.y += t * 0.06;
    f += 0.1250 * _Noise(p);
    p = p * 3.01;
    f += 0.0625 * _Noise(p);
    p = p * 3.03;
    f += 0.03125 * _Noise(p);
    p = p * 3.02;
    f += 0.015625 * _Noise(p);
    return f;
}

float cloud(float3 p, float t)
{
    float cld = fnoise(p * cloudnoise, t) + cloudy * 0.1;
    cld = smoothstep(0.44, 0.64, cld);
    cld *= cld * (5.0 * rainmulti);
    return cld + haze;
}

void densities(float3 pos, out float rayleigh, out float mie)
{
    float h = length(pos - C) - R0;
    rayleigh = exp(-h / Hr);
    
    float cld = 0.0;
    if (mincloudheight < h && h < maxcloudheight)
    {
        cld = cloud(pos + float3(xaxiscloud, yaxiscloud, zaxiscloud), gTime) * cloudy;
        cld *= sin(3.1415 * (h - mincloudheight) / mincloudheight) * cloudy;
    }

    float3 d = pos;
    d.y = 0.0;
    float dist = length(d);
    
    if (dist > cloudfar)
    {
        float factor = clamp(1.0 - ((dist - cloudfar) / (cloudfar - cloudnear)), 0.0, 1.0);
        cld *= factor;
    }

    mie = exp(-h / Hm) + cld + haze;
}

float escape(float3 p, float3 d, float R)
{
    float3 v = p - C;
    float b = dot(v, d);
    float c = dot(v, v) - R * R;
    float det2 = b * b - c;
    if (det2 < 0.0)
        return -1.0;
    float det = sqrt(det2);
    float t1 = -b - det, t2 = -b + det;
    return (t1 >= 0.0) ? t1 : t2;
}

void scatter(float3 o, float3 d, out float3 col, out float3 scat, float t, float3 Ds)
{
    float L = escape(o, d, Ra);
    float mu = dot(d, Ds);
    float opmu2 = 1.0 + mu * mu;
    float phaseR = 0.0596831 * opmu2;
    float phaseM = 0.1193662 * (1.0 - g2) * opmu2 / ((2.0 + g2) * pow(abs(1.0 + g2 - 2.0 * g * mu), 1.5));
    float phaseS = 0.1193662 * (1.0 - s2) * opmu2 / ((2.0 + s2) * pow(abs(1.0 + s2 - 2.0 * s * mu), 1.5));
    
    float depthR = 0.0, depthM = 0.0;
    float3 R = float3(0, 0, 0), M = float3(0, 0, 0);
    float dl = L / (float) steps;
    
    for (int i = 0; i < steps; ++i)
    {
        float l = (float) i * dl;
        float3 p = o + d * l;

        float dR, dM;
        densities(p, dR, dM);
        dR *= dl;
        dM *= dl;
        depthR += dR;
        depthM += dM;

        float Ls = escape(p, Ds, Ra);
        if (Ls > 0.0)
        {
            float dls = Ls / (float) stepss;
            float depthRs = 0.0, depthMs = 0.0;
            for (int j = 0; j < stepss; ++j)
            {
                float ls = (float) j * dls;
                float3 ps = p + Ds * ls;
                float dRs, dMs;
                densities(ps, dRs, dMs);
                depthRs += dRs * dls;
                depthMs += dMs * dls;
            }

            float3 A = exp(-(bR * (depthRs + depthR) + bM * (depthMs + depthM)));
            R += A * dR;
            M += A * dM;
        }
    }

    col = I * (M * bM * phaseM);
#if NICE_HACK_SUN
    col += SI * (M * bM * phaseS);
#endif
    col += I * (R * bR * phaseR);
    scat = 0.1 * (bM * depthM);
}

float3 stars(float3 p)
{
    float3 c = float3(0, 0, 0);
    uint width, height;
    tex.GetDimensions(width, height);
    
    float res = (float) width * 2.5;
    for (float i = 0.0; i < 4.0; i++)
    {
        float3 q = frac(p * (0.15 * res)) - 0.5;
        float3 id = floor(p * (0.15 * res));
        float2 rn = hash33(id).xy;
        float c2 = 1.0 - smoothstep(0.0, 0.6, length(q));
        c2 *= step(rn.x, 0.0005 + i * i * 0.001);
        c += c2 * (lerp(float3(1.0, 0.49, 0.1), float3(0.75, 0.9, 1.0), rn.y) * 0.1 + 0.9);
        p *= 1.3;
    }
    return c * c * 0.8;
}

float4 main(VS_OUT pin) : SV_Target
{
// 1. 現在のピクセルの深度値をサンプリング
    float depthVal = depth.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord).r;
    
    // 2. 手前にオブジェクトがある場合は、元のシーンの色をそのまま返す（またはフォグを混ぜる）
    if (depthVal < 0.999f)
    {
        return tex.Sample(sampler_states[LINEAR_CLAMP], pin.texcoord);
    }
    
    // -----------------------------------------------------------
    // 1. カメラレイの方向をビュー・プロジェクション逆行列から取得
    // -----------------------------------------------------------
    
    // UVをNDC（正規化デバイス座標系）に変換
    float2 ndc = pin.texcoord * 2.0 - 1.0;
    ndc.y = -ndc.y; // DirectXではY軸の向きが反転する

    // プロジェクション空間からビュー空間へ
    float4 clipPos = float4(ndc, 1.0, 1.0);
    float4 viewPos = mul(clipPos, inv_projection);
    viewPos.xyz /= viewPos.w;

    // ビュー空間からワールド空間へのレイ方向ベクトル
    float3 rayDir = normalize(mul(viewPos.xyz, (float3x3) inv_view));
    float3 D = rayDir;

    // -----------------------------------------------------------
    // 2. 太陽の方向（平行光源から取得）
    // -----------------------------------------------------------
    // 平行光源はカメラに向かってくる方向（ライトベクトル）の逆をとるか、
    // C++側で事前に「太陽の位置への方向ベクトル」を入れておいてください。
    float3 Ds = normalize(-directional_light_direction);

    // -----------------------------------------------------------
    // 3. レイの原点（カメラ位置ベース）
    // -----------------------------------------------------------
    // 雲や空の高さをShaderToyの設定に合わせるため、Y軸には元のオフセットを加味します
    float3 O = float3(camera_position.x, cameraheight + camera_position.y, camera_position.z);

    // --- 以下、元コードのレンダリングパイプライン ---
    float3 color = float3(0, 0, 0);
    float3 scat = float3(0, 0, 0);
    float att = 1.0;
    float sunHeight = saturate(Ds.y * 4.0);
    float staratt = 1.0 - sunHeight;
    float scatatt = 1.0 - sunHeight;
    float3 star = float3(0, 0, 0);
    float4 aur = float4(0, 0, 0, 0);
    
    float fade = smoothstep(0.0, 0.01, abs(D.y)) * 0.5 + 0.9;
    
    // Yがマイナス（地平線より下）の反射/屈折ハック
    if (D.y < -ts)
    {
        float L = -O.y / D.y;
        O = O + D * L;
        D.y = -D.y;
        D = normalize(D + float3(0.0, 0.003 * sin(gTime + 6.2831 * noise(O.xz + float2(0.0, -gTime * 1e3))), 0.0));
        att = 0.6;
        star = stars(D);
        // マウスYによる制限は除外し、常に地平線付近にオーロラを出すように調整
        aur = smoothstep(0.0, 2.5, aurora(O, D, pin.position.xy));
    }
    else
    {
        float L1 = O.y / max(D.y, 0.0001);
        float3 O1 = O + D * L1;
        float3 D1 = normalize(D + float3(1.0, 0.0009 * sin(gTime + 6.2831 * noise(O1.xz + float2(0.0, gTime * 0.8))), 0.0));
        star = stars(D1);
        aur = smoothstep(0.0, 1.5, aurora(O, D, pin.position.xy)) * fade;
    }

    star *= att;
    scatter(O, D, color, scat, gTime, Ds);
    
    color *= att;

    scat *= att;
    scat *= scatatt;

    color += scat;
    color += star;
    color += aur.rgb * scatatt;
    
    return float4(color, 1.0);
}