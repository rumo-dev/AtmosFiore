# 全シーンの自動撮影

デバッグ GUI の **Render Test Scenes** から撮影します。シーン一覧の各行にある **Capture** は、そのシーンの ON/OFF だけを保存します。シーンを開いた状態なら **Capture current scene ON/OFF** でも同じ操作ができます。完了後は元のシーンに戻ります。**Capture all scenes (no ImGui)** は全件、**Capture Indoor** は Indoor だけです。撮影中は ImGui を強制非表示にして PNG を保存します。

コマンドラインでは `AtmosFiore.exe --capture-scenes` で同じ処理を実行し、完了すると終了します。ビルド済み exe を使用する PowerShell ランチャーも用意しています。
ランチャーはプロジェクト内の `x64/Release/AtmosFiore.exe` を優先します。古い確認用ビルドや配布パッケージの exe は最新のシーン定義を含まないことがあります。
個別確認は `AtmosFiore.exe --capture-scene=gltf_pbr_emissive` で、指定シーンの ON/OFF だけを撮影します。

Indoor だけ撮り直す場合は `AtmosFiore.exe --capture-indoor` を使用できます。この場合は `data/capture/indoor/` に2枚を保存し、全シーンの撮影結果を保持します。

```powershell
powershell -File tools/capture_all_scenes.ps1
# 任意のビルドを使う場合
powershell -File tools/capture_all_scenes.ps1 -Executable x64/Release/AtmosFiore.exe
# ffmpeg の場所を指定して MP4 に変換する場合
powershell -File tools/capture_all_scenes.ps1 -FFmpeg C:/tools/ffmpeg/bin/ffmpeg.exe
node tools/check_scene_captures.js
```

この環境では ffmpeg 9.0.2 Essentials を Scoop に導入済みで、ランチャーは `ffmpeg` を自動検出します。再インストール用のマニフェストは `tools/ffmpeg-essentials.json` です。

保存先は `data/capture/screenshots/` です。現在は技術・glTF PBR の 49 シーンと Indoor の計 50 シーン、100 枚を撮影します。名前は機能 ID ＋ `_on.png` / `_off.png`（例: `ssao_on.png`、`gltf_pbr_clearcoat_off.png`）です。`report.json` に表示名、ファイル名、機能状態、保存結果、明るさ、ON/OFF の画像差を記録します。同名ファイルは次回撮影時に上書きします。

`catalog.json` の `capture` が `slider` のシーンは、カメラを固定した ON/OFF をサイトの比較スライダーに使います。全 glTF PBR シーンはここに含まれます。`video` のシーン（ポイントライトの影、順応、空、アニメーション、TAA、DLSS、フラスタム、メッシュレット、オクルージョン、ボリューメトリックフォグ）は、ON のウォームアップ後に 30 fps × 60 フレームも `video/<id>/0001.png` から保存し、ImGui の Capture ボタンからでも ffmpeg で `video/<id>.mp4` に変換します。変換結果は `report.json` の `video_encode_exit_code` に記録します。動きは 2 秒周期で、サイト側で MP4 をループ再生できます。メッシュレットとオクルージョンは横からの観察カメラを固定し、独立した判定カメラを動かします。オクルージョンの GPU 深度クエリ用カメラは、クエリ結果が有効になるように移動先で一定時間停止します。

技術シーンでは ImGui の **Show technique geometry** でデバッグ形状を切り替えられます。点光源は中心と半径方向、スポット光は内外の円錐断面、エリア光は発光面、DDGI はプローブ領域、オクルージョンは遮蔽物、フラスタムとメッシュレットは実際の判定カメラの位置と範囲を表示します。光源の正確な位置・範囲は同じ ImGui パネルにも表示します。デバッグ形状は自動撮影にも写ります。

ボリュームフォグでは通路の各柱対に暖色と寒色のスポットライトを設置し、双方を中央通路へ向けています。光の円錐デバッグ形状は初期状態で非表示ですが、上記のチェックボックスから確認できます。

Sky はカメラを囲むスカイボックスメッシュへ空、大気散乱、雲、太陽、月、星を描きます。ImGui の **Sky mood** で色のプリセットを、**Sky time (hours)** で時刻を選択できます。空だけに適用するビネット、方角別の色、地形フォグとの色合わせも調整できます。撮影動画は夜を除いた朝から夕方までの往復でループします。

フラスタム、メッシュレット、オクルージョンでは、右上に判定カメラからの描画結果と実際に除外したインスタンス・メッシュレット・GPU 群の数を表示します。メッシュレットとオクルージョンのメイン画面では、判定で消えた部分をマゼンタのワイヤー枠で示します。オクルージョンでは判定カメラの位置と視錐台をシアンで常時表示します。壁の背後を横から見て、GPU クエリで除外された群と実際に残った形状を同時に確認できます。`report.json` の `culling` に判定数、発行した GPU クエリ数、スキップした三角形数を記録します。

エミッシブとエミッシブ強度の比較では、ON/OFF ともブルームを有効にしています。比較対象の材質機能だけを切り替えるためです。Khronos の glTF 比較アセットにはライセンスと出典を各モデルのディレクトリに同梱しています。
PBR の展示には、原本を残したまま右側の被写体だけを参照する `*Right.glb` を使います。サイトのスライダーでは、同じ被写体に対する機能 ON/OFF を比較できます。

通常の確認シーンは 90 フレーム、Indoor は 180 フレーム描画して履歴・GI・反射の更新を待ちます。確認シーン内の説明 GUI も画像には入りません。カリングなど描画結果を保ったまま処理量を減らす機能では、ON/OFF が同じ画像になる場合があります。PBR 材質チャートは、ON の PBR シェーディングと、OFF のベースカラーのみの描画を比較します。

## Indoor の例外

Indoor は個々の機能ごとの撮影を行わず、同じ固定カメラから次の 2 枚を保存します。構図は `data/capture/camera.json` で指定します。キャラクター、シーン名テキスト、ライトのデバッグ形状、GI のデバッグ表示も除きます。

| ファイル | 内容 |
| --- | --- |
| `Indoor_off.png` | 材質と直接照明による描画。影、IBL、ローカル反射、Voxel GI、DDGI、SSAO、SSR、SSGI、霧、レンズ効果、ブルーム、順応、DoF、TAA、DLSS を無効化 |
| `Indoor_on.png` | Indoor 用に調整した全ポストエフェクト、影、IBL、ローカル反射、Voxel GI、DDGI、SSAO、SSR、SSGI、カリングを有効化した完成シーン |

HDR を見られる画像へ変換するため、固定露出とトーン変換は描画のみの画像にも残します。ON では DLSS Quality を利用可能なら使用し、非対応 GPU では TAA を使用します。この 2 方式は同時適用しません。実際の DLSS の利用状態はレポートに記録します。点光源の影は既存エンジンのアトラス上限内で有効になります。設定は `data/capture/indoor_on.json` と `indoor_off.json` に分け、通常の Indoor 設定は書き換えません。
