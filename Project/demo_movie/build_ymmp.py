"""Build the AtmosFiore job portfolio edit for YukkuriMovieMaker 4."""
from __future__ import annotations

import json
import uuid
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = Path(__file__).resolve().parent / "AtmosFiore_就活デモ.ymmp"
CAP = ROOT / "Data" / "capture" / "screenshots"
FPS = 30
SECTION = 12 * FPS


def anim(value: float) -> dict:
    return {"Values": [{"Value": float(value)}], "Span": 0.0, "AnimationType": "なし"}


def common(frame: int, length: int, layer: int, remark: str = "") -> dict:
    return {
        "X": anim(0), "Y": anim(0), "Z": anim(0),
        "Opacity": anim(100), "Zoom": anim(100), "Rotation": anim(0),
        "FadeIn": 0.25, "FadeOut": 0.25, "Blend": "Normal",
        "IsInverted": False, "IsClippingWithObjectAbove": False,
        "IsAlwaysOnTop": False, "IsZOrderEnabled": False,
        "VideoEffects": [], "Group": 0, "Frame": frame, "Layer": layer,
        "KeyFrames": {"Frames": [], "Count": 0}, "Length": length,
        "PlaybackRate": 100.0, "ContentOffset": "00:00:00",
        "Remark": remark, "IsLocked": False, "IsHidden": False,
    }


def image(path: Path, frame: int, length: int, layer: int, remark: str = "", zoom: int = 100) -> dict:
    if not path.exists():
        raise FileNotFoundError(path)
    item = {"$type": "YukkuriMovieMaker.Project.Items.ImageItem, YukkuriMovieMaker",
            "FilePath": str(path), **common(frame, length, layer, remark)}
    item["Zoom"] = anim(zoom)
    return item


def video(path: Path, frame: int, length: int, layer: int, remark: str = "") -> dict:
    if not path.exists():
        raise FileNotFoundError(path)
    return {
        "$type": "YukkuriMovieMaker.Project.Items.VideoItem, YukkuriMovieMaker",
        "IsWaveformEnabled": False, "FilePath": str(path),
        "AudioTrackIndex": 0, "Volume": anim(0), "Pan": anim(0),
        "IsLooped": True, "EchoIsEnabled": False, "EchoInterval": 0.1,
        "EchoAttenuation": 40.0, "AudioEffects": [],
        **common(frame, length, layer, remark),
    }


def text_item(value: str, frame: int, length: int, layer: int,
              y: int, size: int, color: str = "#FFFFFFFF", remark: str = "") -> dict:
    item = {
        "$type": "YukkuriMovieMaker.Project.Items.TextItem, YukkuriMovieMaker",
        "Text": value, "Decorations": [], "Font": "メイリオ",
        "FontSize": anim(size), "LineHeight2": anim(100),
        "LetterSpacing2": anim(0), "DisplayInterval": 0.0,
        "WordWrap": "NoWrap", "MaxWidth": anim(1800),
        "BasePoint": "CenterTop" if y < 0 else "CenterBottom", "FontColor": color,
        "Style": "Border", "StyleColor": "#FF101820",
        "Bold": True, "Italic": False, "IsTrimEndSpace": False,
        "IsDevidedPerCharacter": False,
        **common(frame, length, layer, remark),
    }
    item["Y"] = anim(y)
    return item


def audio(path: Path, frame: int, length: int) -> dict:
    if not path.exists():
        raise FileNotFoundError(path)
    return {
        "$type": "YukkuriMovieMaker.Project.Items.AudioItem, YukkuriMovieMaker",
        "FilePath": str(path), "IsWaveformEnabled": False,
        "Volume": anim(20), "Pan": anim(0),
        "PlaybackRate": 100.0, "ContentOffset": "00:00:00",
        "IsLooped": False, "EchoIsEnabled": False,
        "EchoInterval": 0.1, "EchoAttenuation": 40.0,
        "AudioEffects": [], "FadeIn": 1.0, "FadeOut": 1.0,
        "Group": 0, "Frame": frame, "Layer": 5,
        "KeyFrames": {"Frames": [], "Count": 0},
        "Length": length, "Remark": "BGM / Data内の音源",
        "IsLocked": False, "IsHidden": False,
    }


# 12 seconds each. Compare source images are from the individual capture folder;
# the older overview contact sheets are intentionally excluded.
SECTIONS = [
    ("完成シーン", "Indoor：統合した描画結果", "Indoor", None),
    ("PBR", "金属・粗さ・光の相互作用", "pbr", None),
    ("IBL", "環境光による自然な反射と陰影", "ibl", None),
    ("ローカル反射", "Reflection Probes", "reflection_probes", None),
    ("太陽光の影", "方向光源のシャドウ", "sun_shadow", None),
    ("点光源の影", "全方向へ広がる光と影", "point_shadow", "point_shadow"),
    ("スポットライト", "照射範囲を制御したライティング", "spot", None),
    ("エリアライト", "面から広がる柔らかな照明", "area", None),
    ("Voxel GI", "間接光による色の回り込み", "voxel", None),
    ("DDGI", "プローブを用いた動的な間接光", "ddgi", None),
    ("SSGI", "画面空間から補う間接光", "ssgi", None),
    ("SSAO", "接地感を高める遮蔽表現", "ssao", None),
    ("SSR", "画面空間の反射表現", "ssr", None),
    ("Sky", "時刻とともに変化する空", "sky", "sky"),
    ("Volumetric Fog", "光の筋と空気感を描く", "volumetric", "volumetric"),
    ("Bloom", "高輝度部分から広がる光", "bloom", None),
    ("Exposure", "明るさの変化に順応する", "adaptation", "adaptation"),
    ("Depth of Field", "視線を導く被写界深度", "dof", None),
    ("TAA", "時間方向のアンチエイリアス", "taa", "taa"),
    ("DLSS", "アップスケーリングによる描画", "dlss", "dlss"),
    ("アニメーション", "3Dシーン内の動きを再生", "animation", "animation"),
    ("Frustum Culling", "視錐台の外側を描画対象から除外", "frustum", "frustum"),
    ("Meshlets", "小さなメッシュ単位で描画を制御", "meshlets", "meshlets"),
    ("Occlusion Culling", "遮蔽物の背後を判定して除外", "occlusion", "occlusion"),
    ("完成シーン", "複数の技術を一つの画に統合", "Indoor", None),
]


def main() -> None:
    items: list[dict] = []
    logo = ROOT / "Data" / "AtmosFioreLogo.png"
    items.append(image(logo, 0, 10 * FPS, 0, "作品ロゴ / オープニング", 65))
    items.append(text_item("就職作品  |  リアルタイム3Dレンダリング", 0, 10 * FPS, 2, 440, 45))
    items.append(text_item("AtmosFiore", 3 * FPS, 5 * FPS, 3, -430, 82, "#FFFFD99A"))

    start = 10 * FPS
    for index, (title, subtitle, stem, clip) in enumerate(SECTIONS, 1):
        frame = start + (index - 1) * SECTION
        off = CAP / f"{stem}_off.png"
        on = CAP / f"{stem}_on.png"
        # Four seconds of the baseline, then the improved result. If a motion
        # capture exists, the last four seconds use it as a looped clip.
        items.append(image(off, frame, 4 * FPS, 0, f"{index:02d} {title} / OFF"))
        items.append(image(on, frame + 4 * FPS, 8 * FPS if clip is None else 4 * FPS,
                           0, f"{index:02d} {title} / ON"))
        if clip:
            items.append(video(CAP / "video" / f"{clip}.mp4", frame + 8 * FPS,
                               4 * FPS, 0, f"{index:02d} {title} / 動画"))
        items.append(text_item(f"{index:02d}  {title}", frame, SECTION, 2, -445, 68,
                               "#FFFFDA9C", f"章タイトル {index:02d}"))
        items.append(text_item(subtitle, frame, SECTION, 3, 445, 48,
                               "#FFFFFFFF", f"説明 {index:02d}"))
        items.append(text_item("OFF", frame, 4 * FPS, 4, 345, 48, "#FFFF8F8F"))
        items.append(text_item("ON", frame + 4 * FPS, 8 * FPS, 4, 345, 48, "#FF9CFFCA"))

    end = start + len(SECTIONS) * SECTION
    items.append(image(logo, end, 12 * FPS, 0, "エンディング / 作品ロゴ", 65))
    items.append(text_item("AtmosFiore", end, 12 * FPS, 2, -430, 82, "#FFFFD99A"))
    items.append(text_item("C++  /  Direct3D 11  /  リアルタイムレンダリング",
                           end + 2 * FPS, 8 * FPS, 3, 440, 46))

    # The supplied game BGM runs 129.62 s. Three non-overlapping sections
    # cover the 5m22s edit without referring to any external music.
    bgm = ROOT / "Data" / "Audio" / "BGM" / "game.wav"
    for frame, length in [(0, 108 * FPS), (108 * FPS, 108 * FPS),
                          (216 * FPS, end + 12 * FPS - 216 * FPS)]:
        items.append(audio(bgm, frame, length))

    total = end + 12 * FPS
    project = {
        "FilePath": str(OUT), "SelectedTimelineIndex": 0,
        "Timelines": [{
            "ID": str(uuid.uuid4()), "Name": "AtmosFiore 就活デモ",
            "VideoInfo": {"FPS": FPS, "Hz": 48000, "Width": 1920, "Height": 1080},
            "VerticalLine": {
                "IsEnabled": False, "StartFrame": 0, "LineType": "BPM",
                "Line": {"$type": "YukkuriMovieMaker.Project.VerticalBPMLine, YukkuriMovieMaker.Plugin",
                         "BPM": 100.0}, "Group": 4,
            },
            "Items": items, "LayerSettings": {"Items": []},
            "CurrentFrame": 0, "Length": total, "MaxLayer": 5,
        }],
        "Characters": [], "CollapsedGroups": [],
    }
    OUT.write_text(json.dumps(project, ensure_ascii=False, indent=2), encoding="utf-8-sig")
    print(f"{OUT}\n{total / FPS:.1f} seconds; {len(items)} timeline items")


if __name__ == "__main__":
    main()
