#include "dof_common.hlsli"

cbuffer ffs_constants : register(b10)
{
    int buf_width;
    int buf_height;
    int padding_x;
    int padding_y;
    int work_width_cb;
    int work_height_cb;
    float max_radius_work;
    float fixed_scale;
};

Texture2D<float4> source_color_map : register(t0);
Texture2D<float2> coc_map : register(t1);      // R=near, G=far CoC
RWBuffer<int> delta_buffer : register(u0);     // interleaved RGBA int32

static const int2 BARTLETT_OFFSETS[9] = {
    int2(-1, -1), int2( 0, -1), int2( 1, -1),
    int2(-1,  0), int2( 0,  0), int2( 1,  0),
    int2(-1,  1), int2( 0,  1), int2( 1,  1)
};
static const int BARTLETT_WEIGHTS[9] = { 1, -2, 1, -2, 4, -2, 1, -2, 1 };

int BufferIndex(int x, int y, int channel)
{
    return (y * buf_width + x) * 4 + channel;
}

void AddDelta(int2 p, int4 value)
{
    if (p.x < 0 || p.x >= buf_width || p.y < 0 || p.y >= buf_height)
        return;

    int base = BufferIndex(p.x, p.y, 0);
    InterlockedAdd(delta_buffer[base + 0], value.r);
    InterlockedAdd(delta_buffer[base + 1], value.g);
    InterlockedAdd(delta_buffer[base + 2], value.b);
    InterlockedAdd(delta_buffer[base + 3], value.a);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    int2 loc = int2(dispatch_id.xy);
    if (loc.x >= work_width_cb || loc.y >= work_height_cb)
        return;

    // Each input pixel contributes to the final image, including in-focus
    // pixels (radius zero). CoC controls only the scatter footprint.
    float2 uv = (float2(loc) + 0.5f) / float2(work_width_cb, work_height_cb);
    float3 color = source_color_map.SampleLevel(sampler_states[LINEAR_CLAMP], uv, 0).rgb;
    float2 coc = saturate(coc_map.SampleLevel(sampler_states[LINEAR_CLAMP], uv, 0).rg);
    int radius = (int)round(max(coc.x, coc.y) * max_radius_work);
    radius = clamp(radius, 0, (int)ceil(max_radius_work));

    // Normalize the Bartlett footprint by area squared. Alpha accumulates
    // the filter weights and is used to normalize color in the resolve pass.
    float4 payload = float4(log2(1.0f + max(color, 0.0f)), 1.0f);
    float r1 = (float)(radius + 1);
    float normalization = 1.0f / (r1 * r1 * r1 * r1);
    int4 fixed_color = int4(round(payload * (normalization * fixed_scale)));

    // The 9-point Bartlett difference kernel becomes a 2D tent after the
    // horizontal and vertical inclusive integrations.
    int2 center = loc + int2(padding_x + 1, padding_y + 1);
    int step = radius + 1;
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        AddDelta(center + BARTLETT_OFFSETS[i] * step,
            fixed_color * BARTLETT_WEIGHTS[i]);
    }
}
