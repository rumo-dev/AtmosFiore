float4 main(float4 pos : SV_POSITION) : SV_Target
{
    // 16px単位のチェッカー
    int checker = (int(pos.x / 16) ^ int(pos.y / 16)) & 1;

    float3 color = checker
        ? float3(1.0, 0.0, 1.0) // マゼンタ
        : float3(0.0, 0.0, 0.0); // 黒

    return float4(color, 1.0);
}
