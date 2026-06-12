Texture2D<float4>
HDR;

Texture2D<float>
CoC;

RWTexture2D<float4>
Near;

RWTexture2D<float4>
Far;

[numthreads(8, 8, 1)]

void main(
uint3 id
:
SV_DispatchThreadID)
{
    float2 p =
id.xy *
2;

    float coc =
CoC[p];

    float4 c =
HDR[p];

    if (
coc < 0
)
        Near[id.xy] = c;
    else
        Far[id.xy] = c;
}