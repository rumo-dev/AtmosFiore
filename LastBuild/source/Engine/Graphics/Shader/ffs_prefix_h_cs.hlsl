// =============================================================
// ffs_prefix_h_cs.hlsl
// Pass 3b: Fast Filter Spreading — 水平方向プレフィックスサム
//
// GDC 2017 スライドに基づく 2D プレフィックスサムの第 1 パス。
//
// ffs_scatter_cs でデルタを書き込んだ delta_buffer に対して
// 行方向（X）の prefix sum を実行する。
//
// 実装方針:
//   各スレッドが 1 行を担当し、左から右へ累積加算する。
//   buf_width が大きい場合（> 1024）はスレッドグループ内で
//   分割して処理するが、通常の DoF 用途（half-res + padding）では
//   512〜256 列程度なので 1 スレッドグループで収まる。
//
// 入力/出力:
//   u0 = delta_buffer (RWBuffer<int>) ← scatter 後、同バッファを上書き
//
// ディスパッチ:
//   (1, buf_height, 1)  — 各スレッドグループが 1 行担当
//   numthreads(1, 1, 1) で 1 スレッドが行全体を走査（シンプル実装）
//   ※ 高解像度が必要な場合は Kogge-Stone / Blelloch に切り替える
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

int BufferIndex(int x, int y, int ch)
{
    return (y * buf_width + x) * 4 + ch;
}

// 各スレッドが y 行全体を水平スキャン
[numthreads(1, 1, 1)]
void main(uint3 group_id : SV_GroupID)
{
    int y = (int) group_id.y;
    if (y >= buf_height)
        return;

    // 4 チャンネルまとめてスキャン
    int4 acc = int4(0, 0, 0, 0);

    for (int x = 0; x < buf_width; ++x)
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
