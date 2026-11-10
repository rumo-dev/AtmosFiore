#ifndef GLTF_PBR_DATA
#define GLTF_PBR_DATA
struct PbrTexture { float4 transform; float4 sampling; int4 info; };
struct PbrMaterial
{
    float4 base, emissive, surface, specular, coat, sheen, iridescence, transmission, attenuation, anisotropy;
    int4 flags;
    float4 alpha;
    PbrTexture textures[17];
};
StructuredBuffer<PbrMaterial> pbr_materials : register(t47);
Texture2D<float4> pbr_textures[17] : register(t48);
SamplerState pbr_samplers[15] : register(s0);
SamplerState pbr_environment_sampler : register(s15);
float2 PbrUV(PbrTexture t, float2 uv0, float2 uv1)
{
    float2 uv=(t.sampling.z>=0.5f?uv1:uv0)*t.transform.zw;
    return float2(t.sampling.x*uv.x-t.sampling.y*uv.y,t.sampling.y*uv.x+t.sampling.x*uv.y)+t.transform.xy;
}
float4 PbrTex(int slot,PbrTexture t,float2 uv0,float2 uv1,float4 fallback)
{
    float2 uv=PbrUV(t,uv0,uv1),dx=ddx(uv),dy=ddy(uv);
    if(t.info.x<0)return fallback;
    // SM5 resource selection is static; sampler selection is uniform per draw.
    switch(t.info.y)
    {
#define PBR_SAMPLE_CASE(n) case n: return pbr_textures[slot].SampleGrad(pbr_samplers[n],uv,dx,dy);
    PBR_SAMPLE_CASE(0) PBR_SAMPLE_CASE(1) PBR_SAMPLE_CASE(2) PBR_SAMPLE_CASE(3) PBR_SAMPLE_CASE(4)
    PBR_SAMPLE_CASE(5) PBR_SAMPLE_CASE(6) PBR_SAMPLE_CASE(7) PBR_SAMPLE_CASE(8) PBR_SAMPLE_CASE(9)
    PBR_SAMPLE_CASE(10) PBR_SAMPLE_CASE(11) PBR_SAMPLE_CASE(12) PBR_SAMPLE_CASE(13) PBR_SAMPLE_CASE(14)
#undef PBR_SAMPLE_CASE
    }
    return pbr_textures[slot].SampleGrad(pbr_samplers[0],uv,dx,dy);
}
float3 PbrLinear(float3 color)
{
    return lerp(color/12.92f,pow(max((color+0.055f)/1.055f,0),2.4f),step(0.04045f,color));
}
#endif
