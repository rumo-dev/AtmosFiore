#include "samplers.hlsli"
#include "fullscreen_quad.hlsli"
#include "constants.hlsli"
Texture2D HDR : t0;

Texture2D Near : t1;

Texture2D Far : t2;

Texture2D CoC : t3;

float4 main(VS_OUT pin
)
:
SV_TARGET
{
    float coc =
CoC.Sample(
sampler_states[LINEAR_CLAMP],
    pin.texcoord
);

    float3 sharp =
HDR.Sample(
sampler_states[LINEAR_CLAMP],
    pin.texcoord);

    float3 blur =
(
coc < 0
)
?
Near.Sample(
sampler_states[LINEAR_CLAMP],
    pin.texcoord * 0.5
)
:
Far.Sample(

sampler_states[LINEAR_CLAMP],
    pin.texcoord * 0.5
);

    return
float4(
lerp(
sharp,
blur,
smoothstep(
0,
1,
abs(coc)
)
),
1
);
}