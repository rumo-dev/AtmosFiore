struct POINT_SHADOW_VS_OUT
{
    float4 position : SV_POSITION;
    float light_z : TEXCOORD0;
};

void main(POINT_SHADOW_VS_OUT pin)
{
    clip(pin.light_z);
}
