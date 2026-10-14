#ifndef DOF_COMMON_HLSLI
#define DOF_COMMON_HLSLI

#include "constants.hlsli"
#include "samplers.hlsli"

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

// Kept for the legacy separable-integration pixel shaders, which remain in
// the project even though the active path uses compute shaders.
static const int SAMPLE_COUNT = 7;

// b9: C++ DepthOfField::dof_local_constants と同じ並び。
cbuffer dof_local_constants : register(b9)
{
    int is_dof;
    float inv_buffer_width;
    float inv_buffer_height;
    int debug_view;
    float inv_full_width;
    float inv_full_height;
    float work_scale;
    float local_padding;
};

float LinearizeDepth(float raw_depth)
{
    float depth = isReversed != 0 ? 1.0f - raw_depth : raw_depth;
    float n = max(near_z, 0.0001f);
    float f = max(far_z, n + 0.0001f);
    return (n * f) / max(f - depth * (f - n), 0.0001f);
}

// Thin lens CoC in pixels, split by the focus plane and normalized to
// CAMERA_CONSTANT_BUFFER::MaxBlurRadius. The scatter pass converts the
// normalized radius back to work-resolution pixels.
float2 ComputeCoC(float linear_depth_m)
{
    float d = max(abs(linear_depth_m), 0.0001f) * 1000.0f;
    float focus = max(FocusDist, 0.0001f) * 1000.0f;
    float focal = max(FocalLength, 0.001f);
    float aperture = focal / max(FNumber, 0.001f);
    float sensor_width = max(SensorSize, 0.001f);
    float pixel_pitch = max(sensor_width * inv_full_width, 0.000001f);

    // c = (f / N) * f * |z - s| / (z * |s - f|)
    float denominator = max(d * abs(focus - focal), 0.000001f);
    float coc_pixels = aperture * focal * abs(d - focus) / denominator / pixel_pitch;
    float normalized_coc = saturate(coc_pixels / max(MaxBlurRadius, 0.001f));

    float near_coc = d < focus ? normalized_coc : 0.0f;
    float far_coc = d > focus ? normalized_coc : 0.0f;
    return float2(near_coc, far_coc);
}

#endif
