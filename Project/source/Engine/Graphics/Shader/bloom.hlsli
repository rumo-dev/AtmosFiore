// BLOOM
cbuffer BLOOM_CONSTANT_BUFFER : register(b8)
{
    int is_enabled;
    float bloom_extraction_threshold;
    float bloom_intensity;
    float something;
};