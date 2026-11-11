"""Build a more focused, narrated-by-captions YMM4 portfolio edit.

This creates a second project. The original AtmosFiore_就活デモ.ymmp is untouched.
"""
from __future__ import annotations

import json
import uuid
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

from build_ymmp import ROOT, CAP, FPS, anim, audio, image, text_item, video


HERE = Path(__file__).resolve().parent
OUT = HERE / "AtmosFiore_就活デモ_技術ストーリー.ymmp"
ASSETS = HERE / "assets"
BLUE = "#FFC9F4FF"
GOLD = "#FFFFD69B"
WHITE = "#FFFFFFFF"


CHAPTERS = [
    ("01  完成画から逆算する", "完成シーンを分解し、画を支える実装へ", [
        ("Indoor / 完成シーン", "材質と直接光だけでは、空間の奥行きが伝わらない。",
         "反射・間接光・影・後処理を統合した画を比較する。", "Indoor", None),
        ("PBR と環境照明", "金属と粗さの違いが光の返り方を変える。",
         "PBR材質チャートとIBLで、材質と環境の関係を確認。", "pbr", "ibl"),
        ("影と光源", "立体感は、光源の形と影の柔らかさで変わる。",
         "方向光・点光・面光源を別シーンで検証。", "sun_shadow", "point_shadow"),
    ]),
    ("02  光を回す", "直接照明だけでは届かない場所へ", [
        ("SSGI", "赤と青の壁から、周囲へ色が移る。",
         "画面内の情報を使う間接光。", "ssgi", None),
        ("Voxel GI", "遮蔽板の裏にも、間接光の変化をつくる。",
         "ボリューム表現で光の広がりを扱う。", "voxel", None),
        ("DDGI", "プローブ補間で、奥まった場所の明るさを表現。",
         "赤と青の光が床・球・背面へ回り込む。", "ddgi", "ddgi_fog"),
    ]),
    ("03  空間を感じさせる", "反射・空・霧でシーンの印象を変える", [
        ("SSR と反射プローブ", "床の反射と、画面外の環境反射を見比べる。",
         "SSRとローカルプローブを用途別に用意。", "ssr", "reflection_probes"),
        ("Dynamic Sky", "空の色と太陽方向が時刻とともに変わる。",
         "朝から夕方までの変化を動画で確認。", "sky", "sky"),
        ("Volumetric Fog", "暖色と寒色の光が、霧の中に筋を描く。",
         "光の円錐と空気の密度を描画。", "volumetric", "volumetric"),
    ]),
    ("04  画を整える", "明るさ・時間方向・再構成を制御する", [
        ("露出と順応", "明暗が変わった後、目が慣れる時間を表現。",
         "静止画と2秒周期の変化を組み合わせて確認。", "adaptation", "adaptation"),
        ("TAA と DLSS", "細線のちらつきを、時間方向の情報で抑える。",
         "DLSSはQualityとネイティブTAAの比較。", "taa", "dlss"),
        ("HDR 仕上げ", "高輝度を階調として収め、光をにじませる。",
         "トーンマップとブルームを分けて検証。", "tone", "bloom"),
    ]),
    ("05  描かずに成立させる", "見える結果を保ち、処理対象を絞る", [
        ("Frustum Culling", "視野の外にあるインスタンスを除外。",
         "右上の判定カメラと除外数を見る。", "frustum", "frustum"),
        ("Meshlets", "小さな単位で可視範囲を判定。",
         "マゼンタの枠は描画を省いた部分。", "meshlets", "meshlets"),
        ("Occlusion Queries", "壁の後ろに隠れた群をGPUで判定。",
         "画面の見た目と除外結果を同時に示す。", "occlusion", "occlusion"),
    ]),
]


def make_assets() -> tuple[Path, Path]:
    ASSETS.mkdir(exist_ok=True)
    source = Image.open(CAP / "Indoor_on.png").convert("RGB").resize((1920, 1080))
    blurred = source.filter(ImageFilter.GaussianBlur(26))
    tint = Image.new("RGB", (1920, 1080), (6, 14, 24))
    card = Image.blend(blurred, tint, 0.82)
    draw = ImageDraw.Draw(card)
    draw.rectangle((90, 118, 101, 962), fill=(228, 183, 116))
    draw.rectangle((112, 118, 114, 962), fill=(80, 131, 150))
    for y in (117, 962):
        draw.line((90, y, 1815, y), fill=(76, 105, 119), width=2)
    card_path = ASSETS / "chapter_card.png"
    card.save(card_path)

    split = Image.new("RGB", (1920, 1080), (7, 17, 27))
    draw = ImageDraw.Draw(split)
    draw.rounded_rectangle((24, 256, 943, 782), radius=14, outline=(63, 85, 97), width=3)
    draw.rounded_rectangle((977, 256, 1896, 782), radius=14, outline=(63, 85, 97), width=3)
    draw.rectangle((954, 270, 966, 770), fill=(221, 177, 110))
    split_path = ASSETS / "compare_frame.png"
    split.save(split_path)
    return card_path, split_path


def item_image(path: Path, start: int, length: int, layer: int,
               label: str, x: int = 0, y: int = 0, zoom: int = 100) -> dict:
    result = image(path, start, length, layer, label, zoom)
    result["X"], result["Y"] = anim(x), anim(y)
    result["FadeIn"] = result["FadeOut"] = 0.2
    return result


def caption(value: str, start: int, length: int, layer: int,
            x: int, y: int, size: int, color: str = WHITE) -> dict:
    result = text_item(value, start, length, layer, y, size, color, value)
    result["X"] = anim(x)
    result["FadeIn"] = 0.25
    result["FadeOut"] = 0.25
    return result


def item_video(stem: str, start: int, length: int, label: str) -> dict:
    result = video(CAP / "video" / f"{stem}.mp4", start, length, 0, label)
    result["FadeIn"] = result["FadeOut"] = 0.2
    return result


def add_segment(items: list[dict], start: int, title: str,
                observation: str, method: str, stem: str, second: str | None,
                split_bg: Path) -> None:
    sec = FPS
    on = CAP / f"{stem}_on.png"
    off = CAP / f"{stem}_off.png"
    # First show the result at full size, then reveal the baseline, then hold
    # an editable side-by-side comparison. The final shot changes per topic.
    items.append(item_image(on, start, 5 * sec, 0, f"{title} / result"))
    items.append(item_image(off, start + 5 * sec, 3 * sec, 0, f"{title} / baseline"))
    items.append(item_image(split_bg, start + 8 * sec, 7 * sec, 0, "比較背景"))
    items.append(item_image(off, start + 8 * sec, 7 * sec, 1,
                            f"{title} / OFF", -480, 0, 48))
    items.append(item_image(on, start + 8 * sec, 7 * sec, 2,
                            f"{title} / ON", 480, 0, 48))
    if second in {"point_shadow", "sky", "volumetric", "adaptation", "frustum", "meshlets", "occlusion"}:
        items.append(item_video(second, start + 15 * sec, 5 * sec, f"{title} / 動き"))
    elif second == "dlss":
        items.append(item_video("dlss", start + 15 * sec, 5 * sec, f"{title} / DLSS"))
    elif second:
        items.append(item_image(CAP / f"{second}_on.png", start + 15 * sec,
                                5 * sec, 0, f"{title} / related"))
    else:
        items.append(item_image(on, start + 15 * sec, 5 * sec, 0,
                                f"{title} / detail", 105))

    items.append(caption(title, start, 20 * sec, 4, 0, -470, 64, GOLD))
    items.append(caption(observation, start, 8 * sec, 5, 0, 465, 46))
    items.append(caption("BASELINE", start + 5 * sec, 3 * sec, 6, 0, 375, 42, "#FFFFAFAF"))
    items.append(caption("OFF", start + 8 * sec, 7 * sec, 5, -480, 380, 40, "#FFFFAFAF"))
    items.append(caption("ON", start + 8 * sec, 7 * sec, 5, 480, 380, 40, "#FFB6FFE0"))
    items.append(caption(method, start + 8 * sec, 12 * sec, 6, 0, 465, 43, BLUE))


def main() -> None:
    card_bg, split_bg = make_assets()
    items: list[dict] = []
    logo = ROOT / "Data" / "AtmosFioreLogo.png"
    sec = FPS
    # 18-second opening: fast, image-first, with the result visible immediately.
    for frame, length, source, line in [
        (0, 4, CAP / "Indoor_on.png", "一枚の完成画に、複数の描画技術を重ねる。"),
        (4, 3, CAP / "ddgi_on.png", "光が、届かない場所へ回る。"),
        (7, 3, CAP / "ssr_on.png", "材質と環境が、互いを映す。"),
        (10, 3, CAP / "volumetric_on.png", "空気そのものを描く。"),
    ]:
        items.append(item_image(source, frame * sec, length * sec, 0, "オープニング"))
        items.append(caption(line, frame * sec, length * sec, 4, 0, 465, 51))
    items.append(item_image(card_bg, 13 * sec, 5 * sec, 0, "作品タイトル背景"))
    items.append(item_image(logo, 13 * sec, 5 * sec, 1, "作品ロゴ", 35))
    items.append(caption("AtmosFiore", 13 * sec, 5 * sec, 3, 0, -443, 88, GOLD))
    items.append(caption("C++  /  Direct3D 11  /  Real-time Rendering",
                         13 * sec, 5 * sec, 4, 0, 452, 42))

    cursor = 18 * sec
    for chapter_title, chapter_subtitle, segments in CHAPTERS:
        items.append(item_image(card_bg, cursor, 5 * sec, 0, chapter_title))
        items.append(caption(chapter_title, cursor, 5 * sec, 3, 0, -125, 75, GOLD))
        items.append(caption(chapter_subtitle, cursor, 5 * sec, 4, 0, 110, 48))
        cursor += 5 * sec
        for title, observation, method, stem, second in segments:
            add_segment(items, cursor, title, observation, method, stem, second, split_bg)
            cursor += 20 * sec

    items.append(item_image(CAP / "Indoor_on.png", cursor, 6 * sec, 0, "最終完成画"))
    items.append(caption("AtmosFiore", cursor, 6 * sec, 3, 0, -455, 86, GOLD))
    items.append(caption("49の技術シーンと統合シーンで、機能ごとに比較・検証。",
                         cursor, 6 * sec, 4, 0, 460, 45))
    cursor += 6 * sec
    items.append(item_image(card_bg, cursor, 12 * sec, 0, "エンドカード"))
    items.append(item_image(logo, cursor, 12 * sec, 1, "作品ロゴ", 48))
    items.append(caption("AtmosFiore", cursor, 12 * sec, 3, 0, -440, 86, GOLD))
    items.append(caption("リアルタイム描画の設計・実装・検証", cursor, 12 * sec, 4, 0, 455, 48))
    cursor += 12 * sec

    bgm = ROOT / "Data" / "Audio" / "BGM" / "game.wav"
    for start in range(0, cursor, 108 * sec):
        clip = audio(bgm, start, min(108 * sec, cursor - start))
        clip["Layer"] = 7
        clip["Volume"] = anim(18)
        items.append(clip)

    project = {
        "FilePath": str(OUT), "SelectedTimelineIndex": 0,
        "Timelines": [{
            "ID": str(uuid.uuid4()), "Name": "AtmosFiore 技術ストーリー",
            "VideoInfo": {"FPS": FPS, "Hz": 48000, "Width": 1920, "Height": 1080},
            "VerticalLine": {"IsEnabled": False, "StartFrame": 0,
                             "LineType": "BPM",
                             "Line": {"$type": "YukkuriMovieMaker.Project.VerticalBPMLine, YukkuriMovieMaker.Plugin",
                                      "BPM": 100.0}, "Group": 4},
            "Items": items, "LayerSettings": {"Items": []},
            "CurrentFrame": 0, "Length": cursor, "MaxLayer": 7,
        }], "Characters": [], "CollapsedGroups": [],
    }
    OUT.write_text(json.dumps(project, ensure_ascii=False, indent=2), encoding="utf-8-sig")
    print(f"{OUT}\n{cursor / FPS:.1f} seconds; {len(items)} items")


if __name__ == "__main__":
    main()
