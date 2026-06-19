#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"
#include "constants.hlsli"
#include "bidirectional_reflectance_distribution_function.hlsli"
#include "shadow_directional.hlsli"
#include "point_light.hlsli"
#include "shadow_point.hlsli"
#include "spot_light.hlsli"
#include "shadow_spot.hlsli"
#include "area_light.hlsli"
#include "shadow_area.hlsli"

Texture2D<float4> gbuffer0 : register(t0);
Texture2D<float4> gbuffer1 : register(t1);
Texture2D<float4> gbuffer2 : register(t2);
Texture2D<float4> gbuffer3 : register(t3);
Texture2D<float> point_shadow_front_map : register(t4);
Texture2D<float> point_shadow_back_map : register(t5);
Texture2D<float> directional_shadow_map : register(t6);

// ==========================================
// 3Dノイズ生成関数 (Value Noise & FBM)
// ==========================================
float hash(float3 p)
{
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise3D(float3 x)
{
    float3 i = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0 - 2.0 * f); // スムーズな補間

    float v000 = hash(i + float3(0, 0, 0));
    float v100 = hash(i + float3(1, 0, 0));
    float v010 = hash(i + float3(0, 1, 0));
    float v110 = hash(i + float3(1, 1, 0));
    float v001 = hash(i + float3(0, 0, 1));
    float v101 = hash(i + float3(1, 0, 1));
    float v011 = hash(i + float3(0, 1, 1));
    float v111 = hash(i + float3(1, 1, 1));

    return lerp(
        lerp(lerp(v000, v100, f.x), lerp(v010, v110, f.x), f.y),
        lerp(lerp(v001, v101, f.x), lerp(v011, v111, f.x), f.y), f.z);
}

// FBM (フラクタルブラウン運動) を使って雲のようなディテールを作る
float fbm(float3 p)
{
    float f = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 3; i++)
    {
        f += amplitude * noise3D(p);
        p *= 2.02;
        amplitude *= 0.5;
    }
    return f;
}
// ==========================================
// ボリュームフォグ計算関連
// ==========================================

// Henyey-Greenstein 位相関数
float HGPhase(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * 3.14159 * pow(max(denom, 0.0001), 1.5));
}
float4 ComputeVolumetricFog(float2 screenPos, float3 rayStart, float3 rayEnd, float3 dirLightDir, float3 dirLightColor)
{
    const int STEP_COUNT = 16;
    float3 rayVector = rayEnd - rayStart;
    float rayLength = length(rayVector);
    if (rayLength < 0.0001f)
        return float4(0, 0, 0, 1);

    float3 rayDir = rayVector / rayLength;
    float stepSize = rayLength / STEP_COUNT;
    float3 step = rayDir * stepSize;

    // ディザー (Interleaved Gradient Noise)
    float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
    float dither = frac(magic.z * frac(dot(screenPos, magic.xy)));
    float3 currentPos = rayStart + step * dither;

    // 室内向け調整パラメータ
    const float fogDensityBase = 0.02f;
    const float fogScattering = 0.8f;
    const float fogAbsorption = 0.002f;
    const float anisotropy = 0.65f;

    float3 scatteredLight = 0.0f;
    float transmittance = 1.0f;

    for (int i = 0; i < STEP_COUNT; i++)
    {
        // ノイズ密度と高さフォグ
        float noiseVal = saturate(lerp(0.4f, 1.6f, fbm(currentPos * 0.5f)));
        float heightFog = exp(-max(currentPos.y, 0.0f) * 0.15f);
        float density = fogDensityBase * noiseVal * heightFog;

        float extinction = density * (fogScattering + fogAbsorption);
        float stepTrans = exp(-extinction * stepSize);

        float3 totalLight = 0;

        // 1. Directional
        {
            float cosTheta = dot(rayDir, -dirLightDir);
            float phase = HGPhase(cosTheta, anisotropy);
            float shadow = SampleDirectionalShadow(directional_shadow_map, currentPos, dirLightDir, dirLightDir);
            totalLight += dirLightColor * phase * shadow;
        }

        // 2. Point Lights
        for (int p = 0; p < numPointLights; ++p)
        {
            if (pointLights[p].intensity <= 0 || pointLights[p].radius <= 0)
                continue;
            float3 lightVec = pointLights[p].position - currentPos;
            float dist = length(lightVec);
            if (dist >= pointLights[p].radius)
                continue;

            float3 L = lightVec / max(dist, 0.0001f);
            float attenuation = pow(saturate(1.0f - dist / pointLights[p].radius), 2.0f);
            
            // ライトシャフト強調用ブースト
            float intensityBoost = min(1.0f / (dist + 0.1f), 5.0f);
            float phase = HGPhase(dot(-rayDir, L), anisotropy);
            float shadow = SamplePointLightShadow(point_shadow_front_map, point_shadow_back_map, currentPos, L, L, p);

            totalLight += pointLights[p].color * pointLights[p].intensity * (attenuation * intensityBoost) * phase * shadow;
        }

        // 3. Spot Lights
        for (int s = 0; s < numSpotLights; ++s)
        {
            if (spotLights[s].intensity <= 0 || spotLights[s].radius <= 0)
                continue;
            float3 lightVec = spotLights[s].position - currentPos;
            float dist = length(lightVec);
            if (dist >= spotLights[s].radius)
                continue;

            float3 L = lightVec / max(dist, 0.0001f);
            float cosAngle = dot(-L, normalize(spotLights[s].direction));
            float spot = smoothstep(cos(spotLights[s].outerAngle), cos(spotLights[s].innerAngle), cosAngle);
            if (spot <= 0)
                continue;

            float distanceAtt = pow(saturate(1.0f - dist / spotLights[s].radius), 2.0f);
            float phase = HGPhase(dot(-rayDir, L), anisotropy);
            
            totalLight += spotLights[s].color * spotLights[s].intensity * (spot * distanceAtt * 2.0f) * phase;
        }
        
        for (uint a = 0; a < numAreaLights; ++a)
        {
            AreaLight_GPU light = areaLights[a];
            if (light.intensity <= 0.0f)
                continue;

        // 1. LTC評価用の基底と逆行列
        // フォグ内では明確な法線がないため、ライトの中心を向くベクトルを法線とする
            float3 N_approx = normalize(light.position - currentPos);
            float3 V = -rayDir; // レイの進行方向の逆
        
        // フォグは拡散体なので roughness は大きめに設定（散乱の広がり）
            float roughness = 0.8f;
            float NoV = saturate(dot(N_approx, V));
            float3x3 Minv = LTC_MatrixInv(roughness, NoV);

        // 2. 形状を考慮した正確な積分 (LTC_Evaluate)
        // ここで提示いただいたコードのLTCロジックをフル実行します
            float integral = LTC_Evaluate(N_approx, V, currentPos, light, Minv);

        // 3. 散乱位相関数 (HG) の適用
        // ライトの向きと視線方向の位相差を計算
            float3 L_dir = normalize(light.position - currentPos);
            float phase = HGPhase(dot(-rayDir, L_dir), anisotropy);

        // 4. 加算
        // integralには面光源の形状を考慮した輝度が格納されている
            totalLight += light.color * light.intensity * integral * phase;
        }
        // 積分
        float3 scattering = totalLight * density * fogScattering;
        float integralWeight = (extinction > 0.0001f) ? (1.0f - stepTrans) / extinction : stepSize;
        scatteredLight += scattering * transmittance * integralWeight;
        transmittance *= stepTrans;

        if (transmittance < 0.01f)
            break;
        currentPos += step;
    }

    return float4(scatteredLight, transmittance);
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 sampled = gbuffer0.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float3 normal = normalize(sampled.xyz);
    //float3 normal = sampled.xyz;
    float roughness = sampled.w;

    sampled = gbuffer1.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float3 basecolor = sampled.xyz;
    float metallic = sampled.w;

    sampled = gbuffer2.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float3 position = sampled.xyz;
    float occlusion = sampled.w;

    sampled = gbuffer3.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    float3 emissive = sampled.xyz;

    const float3 f0 = lerp(0.04f, basecolor, metallic);
    const float3 f90 = 1.0f;
    const float alpha_roughness = roughness * roughness;
    const float3 c_diff = lerp(basecolor, 0.0f, metallic);

    const float3 N = normal;
    const float3 V = normalize(camera_position.xyz - position);
    const float NoV = max(0.0f, dot(N, V));

    //float3 diffuse = ambient_color.rgb * c_diff;
    float3 diffuse = 0;
    float3 specular = 0.0f;

    float3 L = normalize(-directional_light_direction.xyz);
    float3 Li = directional_light_color.rgb * 5;
    float NoL = max(0.0f, dot(N, L));
    if (NoL > 0.0f)
    {
        float3 H = normalize(V + L);
        float NoH = max(0.0f, dot(N, H));
        float HoV = max(0.0f, dot(H, V));
        float directional_shadow = SampleDirectionalShadow(directional_shadow_map, position, N, L);
        

        diffuse += Li * directional_shadow * NoL * brdf_lambertian(f0, f90, c_diff, HoV);
        specular += Li * directional_shadow * NoL * brdf_specular_ggx(f0, f90, alpha_roughness, HoV, NoL, NoV, NoH);
    }

    for (int i = 0; i < numPointLights; ++i)
    {
        if (pointLights[i].intensity <= 0.0f || pointLights[i].radius <= 0.0f)
        {
            continue;
        }

        float3 light_vector = pointLights[i].position - position;
        float dist = length(light_vector);
        if (dist >= pointLights[i].radius)
        {
            continue;
        }

        float3 Lp = light_vector / max(dist, 0.0001f);
        float pointNoL = max(0.0f, dot(N, Lp));
        if (pointNoL <= 0.0f)
        {
            continue;
        }

        float attenuation = pow(max(0.0f, 1.0f - (dist / pointLights[i].radius)), 2.0f);
        float3 pointLi = pointLights[i].color * pointLights[i].intensity * attenuation;
        float3 H = normalize(V + Lp);
        float NoH = max(0.0f, dot(N, H));
        float HoV = max(0.0f, dot(H, V));
        float point_shadow = SamplePointLightShadow(point_shadow_front_map, point_shadow_back_map, position, N, Lp, i);

        diffuse += pointLi * point_shadow * pointNoL * brdf_lambertian(f0, f90, c_diff, HoV);
        specular += pointLi * point_shadow * pointNoL * brdf_specular_ggx(f0, f90, alpha_roughness, HoV, pointNoL, NoV, NoH);
    }

    // スポットライト
    for (int i = 0; i < numSpotLights; ++i)
    {
        if (spotLights[i].intensity <= 0.0f || spotLights[i].radius <= 0.0f)
        {
            continue;
        }

        float3 light_vector = spotLights[i].position - position;
        float dist = length(light_vector);
        if (dist >= spotLights[i].radius)
        {
            continue;
        }

        float3 Ls = light_vector / max(dist, 0.0001f);
        float spotNoL = max(0.0f, dot(N, Ls));
        if (spotNoL <= 0.0f)
        {
            continue;
        }

        // 角度減衰の計算
        float3 lightDir = normalize(spotLights[i].direction);
        float cosAngle = dot(-Ls, lightDir);
        float cosInner = cos(spotLights[i].innerAngle);
        float cosOuter = cos(spotLights[i].outerAngle);
        
        float spotAttenuation = 1.0f;
        if (cosAngle < cosOuter)
        {
            spotAttenuation = 0.0f;
        }
        else if (cosAngle < cosInner)
        {
            spotAttenuation = smoothstep(cosOuter, cosInner, cosAngle);
        }

        float distanceAttenuation = pow(max(0.0f, 1.0f - (dist / spotLights[i].radius)), 2.0f);
        float totalAttenuation = spotAttenuation * distanceAttenuation;
        
        if (totalAttenuation <= 0.0f)
        {
            continue;
        }

        float3 spotLi = spotLights[i].color * spotLights[i].intensity * totalAttenuation;
        float3 H = normalize(V + Ls);
        float NoH = max(0.0f, dot(N, H));
        float HoV = max(0.0f, dot(H, V));

        diffuse += spotLi * spotNoL * brdf_lambertian(f0, f90, c_diff, HoV);
        specular += spotLi * spotNoL * brdf_specular_ggx(f0, f90, alpha_roughness, HoV, spotNoL, NoV, NoH);
    }

   // エリアライト (LTC - Linearly Transformed Cosines)
// Area Light
    for (int i = 0; i < numAreaLights; ++i)
    {
        if (areaLights[i].intensity <= 0)
            continue;


    // 面の向きだけ判定
        if (dot(N, -normalize(areaLights[i].direction)) <= 0)
            continue;

        float3 ltcDiffuse = LTC_Diffuse(N, V, position, areaLights[i], c_diff, f0, f90);

        float3 ltcSpecular = LTC_Specular(N, V, position, areaLights[i], roughness, f0, f90);


    // Directional と揃える
        const float exposure = 5.0;


        diffuse += ltcDiffuse * exposure;


        specular += ltcSpecular * exposure;
    }

    diffuse += ibl_radiance_lambertian(N, V, roughness, c_diff, f0) * 0.2;
    specular += ibl_radiance_ggx(N, V, roughness, f0) * 0.2;

    diffuse *= occlusion;
    specular *= occlusion;

    float3 Lo = diffuse + specular + emissive;
    float4 fog = ComputeVolumetricFog(pin.position.xy, camera_position.xyz, position, L, Li);

    float fogIntensityMultiplier = 1;
    
    // 元のピクセルカラーに透過率を掛け、フォグを加算
    Lo = Lo * fog.a + (fog.rgb * fogIntensityMultiplier);
    return float4(Lo, 1.0f);
}