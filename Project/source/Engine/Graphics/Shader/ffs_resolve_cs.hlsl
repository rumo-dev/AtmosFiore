// =============================================================
// ffs_resolve_cs.hlsl
// Pass 3d: Fast Filter Spreading — リゾルブ
//
// near と far で正規化方法が異なる:
//
//   Near (is_near_field=1):
//     field_separation で rgb *= near_coc（premult済み）、alpha = near_coc。
//     scatter 後の累積値は:
//       val.rgb = Σ(color * coc * weight)
//       val.a   = Σ(coc * weight)
//     → val.rgb / val.a = 加重平均色（premult解除済み）
//     → composite では near_blur.rgb をそのまま使い、
//       blend率として near_blur.a を使う。
//
//   Far (is_near_field=0):
//     field_separation で alpha = far_coc（非premult）、rgb = color。
//     scatter 後の累積値は:
//       val.rgb = Σ(color * weight)
//       val.a   = Σ(coc * weight)
//     → val.rgb / val.a = 加重平均色
//     → composite では far_blur.rgb をそのまま使い、
//       blend率として far_blur.a を使う。
//
//   どちらも rgb /= alpha で正規化することは同じだが、
//   alpha の意味（near=CoC累積, far=CoC累積）は共通。
// =============================================================

#include "dof_common.hlsli"

cbuffer ffs_constants : register(b10)
{
    int buf_width;
    int buf_height;
    int padding_x;
    int padding_y;
    int work_width_cb;
    int work_height_cb;
    float ffs_pad0;
    float ffs_pad1;
};

// b9 の is_dof を流用して near/far フラグを取得
cbuffer dof_local_constants : register(b9)
{
    int is_dof; // ここでは is_near_field として使う（C++側で設定）
    float inv_buffer_width;
    float inv_buffer_height;
    float local_padding;
};

static const float INV_FIXED_SCALE = 1.0f / 65536.0f;

RWBuffer<int> delta_buffer : register(u0);
RWTexture2D<float4> blurred_tex : register(u1);

int BufferIndex(int x, int y, int ch)
{
    return (y * buf_width + x) * 4 + ch;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    int2 loc = int2(dispatch_id.xy);
    if (loc.x >= work_width_cb || loc.y >= work_height_cb)
        return;

    int bx = loc.x + padding_x;
    int by = loc.y + padding_y;

    float4 val;
    val.r = (float) delta_buffer[BufferIndex(bx, by, 0)] * INV_FIXED_SCALE;
    val.g = (float) delta_buffer[BufferIndex(bx, by, 1)] * INV_FIXED_SCALE;
    val.b = (float) delta_buffer[BufferIndex(bx, by, 2)] * INV_FIXED_SCALE;
    val.a = (float) delta_buffer[BufferIndex(bx, by, 3)] * INV_FIXED_SCALE;

    // near / far 共通: rgb が「色 × 重み」の累積、alpha が「重みの累積」
    // → rgb /= alpha で加重平均色に正規化
    // near は field_separation で既に premult(rgb*=coc)済みだが、
    // alpha も coc の累積なので割り算の結果は正しい加重平均色になる
    if (val.a > 0.0001f)
    {
        val.rgb /= val.a;
    }
    // alpha は blend 率として [0,1] に正規化
    // near: CoC の加重平均 → そのまま blend 率として有効
    // far:  同上
    val.a = saturate(val.a);

    blurred_tex[loc] = val;
}
