#include "constants.hlsli"
Texture2D<float>
Depth
:
register(t0);

RWTexture2D<float>
CoC
:
register(u0);

float CalcCoC(
float z)
{
    float f =
FocalLength *
0.001;

    float A =
f /
FNumber;

    float coc =
A *
(
z -
FocusDist
)
/
z;

    coc /=
SensorSize *
0.001;

    return
coc;
}

[numthreads(16, 16, 1)]

void main(
uint3 id
:
SV_DispatchThreadID)
{
    float z =
Depth[id.xy];

    float coc =
CalcCoC(
z
);

    CoC[id.xy] =
clamp(
coc,
-1,
1
)
*
MaxBlurRadius;
}