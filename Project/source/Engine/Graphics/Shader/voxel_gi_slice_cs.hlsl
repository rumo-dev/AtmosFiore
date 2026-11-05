Texture3D<float4> volume : register(t0);
RWTexture2D<float4> slice_output : register(u0);

cbuffer VOXEL_GI_CONSTANTS : register(b0)
{
    float4 volume_min;
    float4 inv_extent;
    uint resolution;
    uint slice;
    float strength;
    float padding;
};

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= resolution || id.y >= resolution) return;
    float4 value = volume.Load(int4(id.x, slice, id.y, 0));
    slice_output[id.xy] = float4(value.a > 0 ? max(value.rgb, 0.15) : 0, 1);
}
