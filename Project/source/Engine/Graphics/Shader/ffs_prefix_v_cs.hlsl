// =============================================================
// ffs_prefix_v_cs.hlsl
// Pass 3c: Fast Filter Spreading — 垂直方向プレフィックスサム
//
// GDC 2017 スライドに基づく 2D プレフィックスサムの第 2 パス。
//
// ffs_prefix_h_cs（水平スキャン済み）の delta_buffer に対して
// 列方向（Y）の prefix sum を実行する。
//
// 2 回の 1D プレフィックスサムにより 2D プレフィックスサムが完成し、
// デルタバッファが積分済みの結果バッファに変換される。
//
// 入力/出力:
//   u0 = delta_buffer (RWBuffer<int>) ← 同バッファを上書き
//
// ディスパッチ:
//   (buf_width, 1, 1) — 各スレッドグループが 1 列担当
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

RWBuffer<int> delta_buffer : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 group_id : SV_GroupID)
{
    int x = (int) group_id.x;
    if (x >= buf_width)
        return;

    int4 acc = int4(0, 0, 0, 0);

    for (int y = 0; y < buf_height; ++y)
    {
        int base = (y * buf_width + x) * 4;
        acc.r += delta_buffer[base + 0];
        acc.g += delta_buffer[base + 1];
        acc.b += delta_buffer[base + 2];
        acc.a += delta_buffer[base + 3];
        delta_buffer[base + 0] = acc.r;
        delta_buffer[base + 1] = acc.g;
        delta_buffer[base + 2] = acc.b;
        delta_buffer[base + 3] = acc.a;
    }
}
