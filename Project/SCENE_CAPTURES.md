# 全シーンの自動撮影

デバッグ GUI の **Render Test Scenes → Capture all scenes (no ImGui)** で開始します。撮影中は ImGui を強制非表示にし、各シーンを順に読み込んで、対象機能 ON/OFF の PNG を保存します。完了後は Indoor に戻ります。

コマンドラインでは `AtmosFiore.exe --capture-scenes` で同じ処理を実行し、完了すると終了します。ビルド済み exe を使用する PowerShell ランチャーも用意しています。

Indoor だけ撮り直す場合は `AtmosFiore.exe --capture-indoor` を使用できます。この場合は `data/capture/indoor/` に2枚を保存し、全シーンの撮影結果を保持します。

```powershell
powershell -File tools/capture_all_scenes.ps1
# 任意のビルドを使う場合
powershell -File tools/capture_all_scenes.ps1 -Executable x64/Release/AtmosFiore.exe
node tools/check_scene_captures.js
```

保存先は `data/capture/screenshots/` です。現在は技術・glTF PBR の 49 シーンと Indoor の計 50 シーン、100 枚を撮影します。名前は機能 ID ＋ `_on.png` / `_off.png`（例: `ssao_on.png`、`gltf_pbr_clearcoat_off.png`）です。`report.json` に表示名、ファイル名、機能状態、保存結果、明るさ、ON/OFF の画像差を記録します。同名ファイルは次回撮影時に上書きします。

通常の確認シーンは 90 フレーム、Indoor は 180 フレーム描画して履歴・GI・反射の更新を待ちます。確認シーン内の説明 GUI も画像には入りません。カリングなど描画結果を保ったまま処理量を減らす機能では、ON/OFF が同じ画像になる場合があります。PBR 材質チャートは、ON の PBR シェーディングと、OFF のベースカラーのみの描画を比較します。

## Indoor の例外

Indoor は個々の機能ごとの撮影を行わず、同じ固定カメラから次の 2 枚を保存します。構図は `data/capture/camera.json` で指定します。キャラクター、シーン名テキスト、ライトのデバッグ形状、GI のデバッグ表示も除きます。

| ファイル | 内容 |
| --- | --- |
| `Indoor_off.png` | 材質と直接照明による描画。影、IBL、ローカル反射、Voxel GI、DDGI、SSAO、SSR、SSGI、霧、レンズ効果、ブルーム、順応、DoF、TAA、DLSS を無効化 |
| `Indoor_on.png` | Indoor 用に調整した全ポストエフェクト、影、IBL、ローカル反射、Voxel GI、DDGI、SSAO、SSR、SSGI、カリングを有効化した完成シーン |

HDR を見られる画像へ変換するため、固定露出とトーン変換は描画のみの画像にも残します。ON では DLSS Quality を利用可能なら使用し、非対応 GPU では TAA を使用します。この 2 方式は同時適用しません。実際の DLSS の利用状態はレポートに記録します。点光源の影は既存エンジンのアトラス上限内で有効になります。設定は `data/capture/indoor_on.json` と `indoor_off.json` に分け、通常の Indoor 設定は書き換えません。
