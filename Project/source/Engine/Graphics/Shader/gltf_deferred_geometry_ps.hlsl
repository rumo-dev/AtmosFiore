// DEFERRED_RENDERING
#include "gltf_model.hlsli"
#include "samplers.hlsli"


// UNIT.35
// https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#reference-textureinfo
struct texture_info
{
    int index; // required.
    int texcoord; // The set index of texture's TEXCOORD attribute used for texture coordinate mapping.
};
// https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#reference-material-normaltextureinfo
struct normal_texture_info
{
    int index; // required
    int texcoord; // The set index of texture's TEXCOORD attribute used for texture coordinate mapping.
    float scale; // scaledNormal = normalize((<sampled normal texture value> * 2.0 - 1.0) * vec3(<normal scale>, <normal scale>, 1.0))
};
// https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#reference-material-occlusiontextureinfo
struct occlusion_texture_info
{
    int index; // required
    int texcoord; // The set index of texture's TEXCOORD attribute used for texture coordinate mapping.
    float strength; // A scalar parameter controlling the amount of occlusion applied. A value of `0.0` means no occlusion. A value of `1.0` means full occlusion. This value affects the final occlusion value as: `1.0 + strength * (<sampled occlusion texture value> - 1.0)`.
};
// https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#reference-material-pbrmetallicroughness
struct pbr_metallic_roughness
{
    float4 basecolor_factor; // len = 4. default [1,1,1,1]
    texture_info basecolor_texture;
    float metallic_factor; // default 1
    float roughness_factor; // default 1
    texture_info metallic_roughness_texture;
};
struct material_constants
{
    float3 emissive_factor; // length 3. default [0, 0, 0]
    int alpha_mode; // "OPAQUE" : 0, "MASK" : 1, "BLEND" : 2
    float alpha_cutoff; // default 0.5
    int double_sided; // default false;

    pbr_metallic_roughness pbr_metallic_roughness;

    normal_texture_info normal_texture;
    occlusion_texture_info occlusion_texture;
    texture_info emissive_texture;
};
StructuredBuffer<material_constants> materials : register(t0);

// UNIT.36
#define BASECOLOR_TEXTURE 0
#define METALLIC_ROUGHNESS_TEXTURE 1
#define NORMAL_TEXTURE 2
#define EMISSIVE_TEXTURE 3
#define OCCLUSION_TEXTURE 4
Texture2D<float4> material_textures[5] : register(t1);

/*
	G-Buffer Layout (Deferred Rendering, PBR-Compatible)

	This configuration uses 4 Render Targets + Depth Buffer.
	Designed for high-quality physically based rendering (PBR) pipelines.

	GBuffer0 : R16G16B16A16_FLOAT
		xyz = World-space normal vector
		w   = Surface roughness (0?1)

	GBuffer1 : R8G8B8A8_UNORM
		rgb = Base color (albedo, sRGB)
		a   = Metalness (0 for dielectric, 1 for metal)

	GBuffer2 : R16G16B16A16_FLOAT
		xyz = World-space position (high precision)
		w   = Ambient occlusion (AO)

	GBuffer3 : R16G16B16A16_FLOAT
		rgb = Emissive color
		w   = Subsurface / Clearcoat / Specular intensity (material-dependent)

	Depth Buffer : D32_FLOAT
		Stores per-pixel depth value used for position reconstruction,
		shadow mapping, and post-processing effects.

	Notes:
	- Memory cost at 1920x1080 ? 64 MB per frame.
	- For optimization, position may be reconstructed from depth instead of
		being stored directly in GBuffer2.
	- Optional extensions:
		* Velocity buffer (for motion blur / TAA)
		* Material ID buffer (for special effects)

	Example usage in lighting pass:
		float3 normal    = normalize(gbuffer0.xyz);
		float  roughness = gbuffer0.w;
		float3 albedo    = SRGBToLinear(gbuffer1.rgb);
		float  metalness = gbuffer1.a;
		float3 position  = gbuffer2.xyz;
		float  ao        = gbuffer2.w;
		float3 emissive  = gbuffer3.rgb;
*/
struct PS_OUT_GBUFFER
{
    float4 gbuffer0 : SV_TARGET0; // Normal + Roughness
    float4 gbuffer1 : SV_TARGET1; // Albedo + Metalness
    float4 gbuffer2 : SV_TARGET2; // World Position + AO
    float4 gbuffer3 : SV_TARGET3; // Emissive + Subsurface/Clearcoat
};

#if 1
// If you enable earlydepthstencil, 'clip', 'discard' and 'Alpha to coverage' won't work!
[earlydepthstencil]
#endif
PS_OUT_GBUFFER main(VS_OUT pin, bool is_front_face : SV_IsFrontFace)
{
	// DEFERRED_RENDERING
    PS_OUT_GBUFFER pout;
	
    const float GAMMA = 2.2;

    const material_constants m = materials[material];

    float4 basecolor_factor = m.pbr_metallic_roughness.basecolor_factor;
    const int basecolor_texture = m.pbr_metallic_roughness.basecolor_texture.index;
    if (basecolor_texture > -1)
    {
        float4 sampled = material_textures[BASECOLOR_TEXTURE].Sample(sampler_states[ANISOTROPIC_WRAP], pin.texcoord);
        sampled.rgb = pow(sampled.rgb, GAMMA);
        basecolor_factor *= sampled;
    }
    if (m.alpha_mode == 0 /*OPAQUE*/)
    {
        basecolor_factor.a = 1.0;
    }
#if 1
    else if (m.alpha_mode == 1 /*MASK*/ || m.alpha_mode == 2 /*BLEND*/)
    {
        clip(basecolor_factor.a - m.alpha_cutoff);
    }
#endif

    float3 emissive_factor = m.emissive_factor;
    const int emissive_texture = m.emissive_texture.index;
    if (emissive_texture > -1)
    {
        float4 sampled = material_textures[EMISSIVE_TEXTURE].Sample(sampler_states[ANISOTROPIC_WRAP], pin.texcoord);
        sampled.rgb = pow(sampled.rgb, GAMMA);
        emissive_factor *= sampled.rgb;
    }

    float roughness_factor = m.pbr_metallic_roughness.roughness_factor;
    float metallic_factor = m.pbr_metallic_roughness.metallic_factor;
    const int metallic_roughness_texture = m.pbr_metallic_roughness.metallic_roughness_texture.index;
    if (metallic_roughness_texture > -1)
    {
        float4 sampled = material_textures[METALLIC_ROUGHNESS_TEXTURE].Sample(sampler_states[LINEAR_WRAP], pin.texcoord);
        roughness_factor *= sampled.g;
        metallic_factor *= sampled.b;
    }

    float occlusion_factor = 1.0;
    const int occlusion_texture = m.occlusion_texture.index;
    if (occlusion_texture > -1)
    {
        float4 sampled = material_textures[OCCLUSION_TEXTURE].Sample(sampler_states[LINEAR_WRAP], pin.texcoord);
        occlusion_factor *= sampled.r;
    }
	//const float occlusion_strength = m.occlusion_texture.strength;


    float3 N = normalize(pin.w_normal.xyz);
    float3 T = has_tangent ? normalize(pin.w_tangent.xyz) : float3(1, 0, 0.0001);
    float sigma = has_tangent ? pin.w_tangent.w : 1.0;
    T = normalize(T - N * dot(N, T));
    float3 B = normalize(cross(N, T) * sigma);
#if 1
	// For a back-facing surface, the tangential basis vectors are negated.
    if (is_front_face == false)
    {
        T = -T;
        B = -B;
        N = -N;
    }
#endif

    const int normal_texture = m.normal_texture.index;
    if (normal_texture > -1)
    {
        float4 sampled = material_textures[NORMAL_TEXTURE].Sample(sampler_states[LINEAR_WRAP], pin.texcoord);
        float3 normal_factor = sampled.xyz;
        normal_factor = (normal_factor * 2.0) - 1.0;
        normal_factor = normalize(normal_factor * float3(m.normal_texture.scale, m.normal_texture.scale, 1.0));
        N = normalize((normal_factor.x * T) + (normal_factor.y * B) + (normal_factor.z * N));
    }

	
    pout.gbuffer0.xyz = N;
    pout.gbuffer0.w = roughness_factor;
	
    pout.gbuffer1.rgb = basecolor_factor.rgb;
    pout.gbuffer1.w = metallic_factor;

    pout.gbuffer2.xyz = pin.w_position.xyz;
    pout.gbuffer2.w = occlusion_factor;

    pout.gbuffer3.rgb = emissive_factor;
    pout.gbuffer3.w = 0;

    return pout;
}


