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

RWBuffer<int> delta_buffer : register(u0);
RWTexture2D<float4> blurred_tex : register(u1);

int BufferIndex(int x, int y, int channel)
{
    return (y * buf_width + x) * 4 + channel;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    int2 loc = int2(dispatch_id.xy);
    if (loc.x >= work_width_cb || loc.y >= work_height_cb)
        return;

    int2 p = loc + int2(padding_x, padding_y);
    int base = BufferIndex(p.x, p.y, 0);
    float4 value = float4(
        delta_buffer[base + 0],
        delta_buffer[base + 1],
        delta_buffer[base + 2],
        delta_buffer[base + 3]) / max(fixed_scale, 1.0f);

    float3 color = 0.0f;
    if (value.a > 0.000001f)
    {
        float3 average_log_color = max(value.rgb / value.a, 0.0f);
        color = max(exp2(average_log_color) - 1.0f, 0.0f);
    }

    // Resolve is the complete output image, not a layer to lerp over the
    // original. Accumulated alpha is only the normalization weight.
    blurred_tex[loc] = float4(color, 1.0f);
}
