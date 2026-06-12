Texture2D<float> CoC;
RWTexture2D<float> Tile;

// 1. Change to uint to satisfy InterlockedMax requirements
groupshared uint sharedMax;

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
    // Initialize the shared memory (only once per group)
    if (gtid.x == 0 && gtid.y == 0)
        sharedMax = 0;

    GroupMemoryBarrierWithGroupSync();

    // 2. Convert float to uint for the interlocked operation
    uint val = asuint(abs(CoC[id.xy]));
    InterlockedMax(sharedMax, val);

    GroupMemoryBarrierWithGroupSync();

    // 3. Write out the result
    if (gtid.x == 0 && gtid.y == 0)
    {
        Tile[gid.xy] = asfloat(sharedMax);
    }
}