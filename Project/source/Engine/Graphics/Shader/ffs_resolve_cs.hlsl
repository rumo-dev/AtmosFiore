// =============================================================
// ffs_resolve_cs.hlsl
// Pass 3d: Fast Filter Spreading — リゾルブ（固定小数点 → float テクスチャ）
//
// 2D プレフィックスサム完了後の delta_buffer から
// 各ピクセルの最終ブラー値を読み出し、
// float16 RWTexture2D に書き込む。
//
// 正規化:
//   Bartlett フィルタは 2 回積分により面積 = (R+1)^2 * 4
//   → alpha チャンネルに蓄積した重み合計で除算して正規化。
//   (GDC スライド "Weights and Normalization" 節参照)
//
// 出力:
//   u1 = blurred_tex (RWTexture2D<float4>) — ボケ完成テクスチャ
//        RGB = ボケカラー（near は premult alpha のまま保持）
//        A   = 重み付き平均 CoC（composite_ps の blend 比率に使用）
//
// 定数バッファ:
//   b10: ffs_constants → buf_*, padding_*, work_*
// =============================================================

#include "dof_common.hlsli"

cbuffer ffs_constants : register(b10)
{
    int   buf_width;
    int   buf_height;
    int   padding_x;
    int   padding_y;
    int   work_width_cb;
    int   work_height_cb;
    float ffs_pad0;
    float ffs_pad1;
};

// 固定小数点スケール（scatter CS と同値）
static const float INV_FIXED_SCALE = 1.0f / 65536.0f;

RWBuffer<int> delta_buffer   : register(u0);
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

    // パディング込みバッファ上のピクセル位置
    int bx = loc.x + padding_x;
    int by = loc.y + padding_y;

    // 固定小数点値を読み出し → float 変換
    float4 val;
    val.r = (float) delta_buffer[BufferIndex(bx, by, 0)] * INV_FIXED_SCALE;
    val.g = (float) delta_buffer[BufferIndex(bx, by, 1)] * INV_FIXED_SCALE;
    val.b = (float) delta_buffer[BufferIndex(bx, by, 2)] * INV_FIXED_SCALE;
    val.a = (float) delta_buffer[BufferIndex(bx, by, 3)] * INV_FIXED_SCALE;

    // alpha（重み合計）で正規化
    // near_field は premult alpha なので RGB / alpha は行わない
    // （composite_ps 側で premult を解除する）
    // alpha には累積重みが格納されているので [0,1] にクランプ
    val.a = saturate(val.a);

    blurred_tex[loc] = val;
}
