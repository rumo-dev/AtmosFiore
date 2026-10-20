#include "fullscreen_quad.hlsli"
#include "samplers.hlsli"

Texture2D<float4> current_color : register(t0);
Texture2D<float4> history_color : register(t1);
Texture2D<float2> motion_vectors : register(t2);

cbuffer TAA_CONSTANT_BUFFER : register(b12)
{
    float history_weight;
    float history_valid;
    float inverse_width;
    float inverse_height;
};

float4 main(VS_OUT pin) : SV_TARGET
{
    const float4 current = current_color.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    if (history_valid < 0.5f)
        return current;

    const float2 velocity = motion_vectors.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord, 0);
    const float2 previous_uv = pin.texcoord + velocity;
    if (any(previous_uv < 0.0f) || any(previous_uv > 1.0f))
        return current;

    // A five-tap cross neighborhood limits history ghosting at lower cost than
    // a full 3x3 gather on every output pixel.
    float3 neighborhood_min = current.rgb;
    float3 neighborhood_max = current.rgb;
    const float2 pixel_x = float2(inverse_width, 0.0f);
    const float2 pixel_y = float2(0.0f, inverse_height);
    const float3 left_color = current_color.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord - pixel_x, 0).rgb;
    const float3 right_color = current_color.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord + pixel_x, 0).rgb;
    const float3 up_color = current_color.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord - pixel_y, 0).rgb;
    const float3 down_color = current_color.SampleLevel(sampler_states[POINT_CLAMP], pin.texcoord + pixel_y, 0).rgb;
    neighborhood_min = min(neighborhood_min, min(min(left_color, right_color), min(up_color, down_color)));
    neighborhood_max = max(neighborhood_max, max(max(left_color, right_color), max(up_color, down_color)));

    float3 history = history_color.SampleLevel(
        sampler_states[LINEAR_CLAMP], previous_uv, 0).rgb;
    history = clamp(history, neighborhood_min, neighborhood_max);
    const float motion_pixels = length(velocity / float2(inverse_width, inverse_height));
    const float weight = history_weight * (1.0f - saturate(motion_pixels / 32.0f));
    return float4(lerp(current.rgb, history, weight), current.a);
}
