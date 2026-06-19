#include "sprite.hlsli"
Texture2D color_map : register(t0);
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
float4 main(VS_OUT pin) : SV_TARGET
{
    return color_map.Sample(sampler_states[Anisotropic_Wrap], pin.texcoord) * pin.color;
}
