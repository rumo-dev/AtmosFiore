Texture2D<float4>
Src;

Texture2D<float>
CoC;

Texture2D<float>
Tile;

RWTexture2D<float4>
Out;

static const
float2 Disk[16] =
{
    { -.94, -.39 },
    { .94, -.76 },
    { -.09, -.92 },
    { .34, .29 },
    { -.91, .45 },
    { .78, .55 },
    { -.52, .84 },
    { .12, -.21 },
    { .53, -.57 },
    { -.76, -.13 },
    { .95, .21 },
    { -.35, .62 },
    { .15, .91 },
    { -.22, -.63 },
    { .63, .01 },
    { -.45, -.88 }
};

[numthreads(8, 8, 1)]

void main(
uint3 id
:
SV_DispatchThreadID)
{
    float r =
Tile[
id.xy / 8
];

    if (
r < 1
)
    {
        Out[id.xy] =
Src[id.xy];

        return;
    }

    float3 c = 0;

    float w = 0;

    for (
int i = 0;
i < 16;
i++
)
    {
        float2 uv =
id.xy
+
Disk[i]
*
r;

        float3 s =
Src[
uv
]
.rgb;

        c += s;

        w += 1;
    }

    Out[id.xy] =
float4(
c / w,
1
);
}