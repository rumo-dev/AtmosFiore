// =============================================================
// ffs_scatter_cs.hlsl
// Pass 3: Fast Filter Spreading — Scatter（デルタ書き込み）
//

//
// アルゴリズム概要:
//   各スレッドが 1 ピクセルを担当し、
//   そのピクセルの CoC サイズに応じた矩形四隅に
//   Bartlett フィルタのデルタ値を InterlockedAdd で書き込む。
//   → その後 ffs_prefix_sum_cs.hlsl が 2D プレフィックスサムを実行
//
// Bartlett（テント）カーネルのデルタパターン（9 点）:
//   各入力 pixel の CoC 半径を R とすると、
//   出力バッファの以下 9 点に delta を加算する:
//     (cx-R-1, cy-R-1): +w
//     (cx,     cy-R-1): -2w
//     (cx+R+1, cy-R-1): +w
//     (cx-R-1, cy    ): -2w
//     (cx,     cy    ): +4w
//     (cx+R+1, cy    ): -2w
//     (cx-R-1, cy+R+1): +w
//     (cx,     cy+R+1): -2w
//     (cx+R+1, cy+R+1): +w
//
// 固定小数点スケール:
//   float → int 変換時に FIXED_SCALE を掛けてスケールアップ。
//   32bit int → 上位ビットをフィルタ面積分、下位ビットを色精度に使う。
//   64×64 Bartlett なら 面積 = 64*64 = 4096 → 12bit 消費 → 20bit 残
//
// 入力:
//   t0 = near_field または far_field (RGB = color [near は premult], A = CoC [0,1])
//
// 出力:
//   u0 = delta_buffer  RWBuffer<int> (RGBA を 4 要素として連続格納)a
//        サイズ = (work_width + 2*MAX_RADIUS + 2) * (work_height + 2*MAX_RADIUS + 2) * 4
//        ※ 境界ガード用にパディングを追加
//
// 定数バッファ:
//   b1: CAMERA_CONSTANT_BUFFER  → MaxBlurRadius
//   b9: dof_local_constants     → inv_buffer_width/height
// =============================================================

#include "dof_common.hlsli"

// b9
cbuffer dof_local_constants : register(b9)
{
    int is_dof;
    float inv_buffer_width;
    float inv_buffer_height;
    float local_padding;
};

cbuffer ffs_constants : register(b10)
{
    int buf_width; // パディング込みバッファ幅  = work_width  + 2*MAX_R + 2
    int buf_height; // パディング込みバッファ高さ = work_height + 2*MAX_R + 2
    int padding_x; // MAX_RADIUS + 1
    int padding_y; // MAX_RADIUS + 1
    int work_width_cb; // ワーク解像度幅
    int work_height_cb; // ワーク解像度高さ
    float ffs_pad0;
    float ffs_pad1;
};

Texture2D<float4> source_tex : register(t0);

// 固定小数点デルタバッファ (R,G,B,A が連続 int として格納)
RWBuffer<int> delta_buffer : register(u0);

// 固定小数点スケール係数
// 精度設計: 最大 CoC = MaxBlurRadius (最大 64px) → 面積 64^2 = 4096 → 12bit
// float [0,1] を 16bit 整数精度で保持 → 2^16 = 65536
// 12 + 16 = 28bit < 31bit (int の有効符号ビット) → 安全
static const int FIXED_SCALE = 65536;

// バッファの線形インデックス計算（R,G,B,A それぞれが要素）
int BufferIndex(int x, int y, int channel)
{
    return (y * buf_width + x) * 4 + channel;
}

// デルタバッファへのアトミック加算
void AddDelta(int x, int y, int4 delta_int)
{
    // 境界チェック（パディング込みバッファ範囲内か確認）
    if (x < 0 || x >= buf_width || y < 0 || y >= buf_height)
        return;

    InterlockedAdd(delta_buffer[BufferIndex(x, y, 0)], delta_int.r);
    InterlockedAdd(delta_buffer[BufferIndex(x, y, 1)], delta_int.g);
    InterlockedAdd(delta_buffer[BufferIndex(x, y, 2)], delta_int.b);
    InterlockedAdd(delta_buffer[BufferIndex(x, y, 3)], delta_int.a);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    int2 loc = int2(dispatch_id.xy);

    if (loc.x >= work_width_cb || loc.y >= work_height_cb)
        return;

    // 入力サンプル
    float4 src = source_tex.Load(int3(loc, 0));
    float coc = src.a; // [0, 1]

    // CoC が極小ならスキップ（フォーカス内）
    if (coc < 0.001f)
        return;

    // CoC [0,1] → ピクセル半径
    int R = (int) round(coc * MaxBlurRadius);
    R = clamp(R, 0, (int) MaxBlurRadius);

    if (R == 0)
        return;

    // 固定小数点変換（色 + α）
    int4 intColor;
    intColor.r = (int) (src.r * FIXED_SCALE);
    intColor.g = (int) (src.g * FIXED_SCALE);
    intColor.b = (int) (src.b * FIXED_SCALE);
    intColor.a = (int) (src.a * FIXED_SCALE);

    // パディングオフセット後の出力バッファ上の中心座標
    int cx = loc.x + padding_x;
    int cy = loc.y + padding_y;

    // Bartlett デルタ 9 点書き込み
    // (GDC スライド p.xx "Adding Deltas" のパターン)
    //
    //  delta_value の符号:
    //   +1  (四隅)  → +w
    //   -2  (辺中)  → -2w
    //   +4  (中心)  → +4w
    //
    // GDC では (R+1) を使って「ちょうど R px 幅のテント」を生成する。
    // 境界を (cx ± (R+1)) にすることで 2 回プレフィックスサム後に
    // 幅 2R+1 の Bartlett カーネルが得られる。

    // Bartlett デルタ: 4隅のみに書き込む（正しい実装）
    //
    // 2D プレフィックスサム（H→V の 2 回）を経ることで
    // 幅 2R+1 の Bartlett（テント）カーネルが得られる。
    //
    // 正しくは 2D の差分演算子として 4 隅のみ:
    //   +w at (cx-(R+1), cy-(R+1))  TL
    //   -w at (cx+(R+1), cy-(R+1))  TR
    //   -w at (cx-(R+1), cy+(R+1))  BL
    //   +w at (cx+(R+1), cy+(R+1))  BR
    //
    // H サムで各行が幅 2R+1 のボックス積分になり、
    // V サムでさらに積分されて 2D Bartlett（テント）カーネルが完成する。
    // 9 点パターンに辺中・中心の余分なデルタを加えると
    // プレフィックスサム後に格子状アーティファクトが生じる（旧バグ）。

    // inclusive prefix sum の正しいオフセット:
    //
    // H 方向: +w at a, -w at b → sum = +w for [a, b-1]
    // 欲しい範囲 [cx-R, cx+R] → a = cx-R, b = cx+R+1
    // V 方向も同様: a = cy-R, b = cy+R+1
    //
    // よって 4 隅は非対称:
    //   TL (cx-R,   cy-R  ) +w
    //   TR (cx+R+1, cy-R  ) -w
    //   BL (cx-R,   cy+R+1) -w
    //   BR (cx+R+1, cy+R+1) +w
    //
    // (R+1) を両辺に使う対称版は 1px ズレて同心円縞が出る原因になる。

    int4 neg = int4(-intColor.r, -intColor.g, -intColor.b, -intColor.a);

    AddDelta(cx - R, cy - R, intColor); // TL +w
    AddDelta(cx + R + 1, cy - R, neg); // TR -w
    AddDelta(cx - R, cy + R + 1, neg); // BL -w
    AddDelta(cx + R + 1, cy + R + 1, intColor); // BR +w
}
