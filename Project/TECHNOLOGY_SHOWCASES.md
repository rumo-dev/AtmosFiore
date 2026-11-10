# 技術別ショーケース

Khronos PBR ページの公式モデルを使用する追加 17 シーンについては [GLTF_PBR_SUPPORT.md](GLTF_PBR_SUPPORT.md) を参照してください。以下の 32 シーンと合わせ、選択可能なシーンは 49 です。

ビルド後、デバッグ GUI の **Render Test Scenes** から選択します。専用のモデル、照明、開始カメラ、ポストプロセス設定を持つ32シーンです。`Find technique` で検索できます。

各シーンの説明に観察ポイントを表示します。`Technique enabled` で同じカメラのまま比較し、`Reset camera` で開始位置に戻せます。W/A/S/D・Q/E で移動、右ドラッグで視点変更、Shift で高速移動します。PBR は材質チャート自体が比較対象です。方向光の影は PCSS / PCF 比較です。

| シーン | 効果を観察する配置・操作 |
| --- | --- |
| PBR metal / roughness | 非金属・銀・金の球。右ほど粗い表面 |
| Image based lighting | 直射光を消した金属球１つで環境照明を比較 |
| SSAO contact occlusion | 箱の接地部と壁・床の角 |
| Screen space reflections | 鏡面床と暖色・寒色の発光物。横へ移動して画面外の制約も観察 |
| Local reflection probes | 生成したギャラリーを実際にキューブマップへキャプチャ。金属球１つで画面外の発光物の反射を比較。SSR は無効 |
| Screen space GI | 赤・青の壁、発光面、白い球への色移り |
| Voxel GI | 発光面と遮蔽板。環境照明と SSGI を除いたボリューム GI |
| DDGI surface probes | 同じ遮蔽板の裏の間接光とプローブ補間 |
| DDGI illuminated fog | 赤・青の GI が霧を照らす。GI のみの霧表示 |
| Directional PCSS | 高さの異なる柱と頭上の梁。接触影と半影を PCF と比較 |
| Point light shadows | 移動する点光源と柱の影 |
| Area lights | 大きな矩形光源と、少し粗い金属球１つのハイライト |
| Spot lights | 暖色・寒色の光の円錐と内外角の減衰 |
| Froxel volumetric fog | 柱・梁のある回廊と光芒 |
| Height fog | 柱の足元だけを覆う低い霧 |
| Distance fog | 等間隔の柱と線形の距離減衰 |
| Exponential fog | 長い回廊での指数的な減衰 |
| HDR bloom | 輝度が段階的に増える発光パネル |
| Physical exposure | 発光パネル。カメラ GUI の絞り・シャッター・ISO も操作可能 |
| Eye adaptation | 明暗が時間で切り替わる照明と順応の遅れ |
| Tone mapping | HDR 輝度の段階表示と明部の圧縮。OFF は線形クリッピング |
| Depth of field | 近・中・遠の球。中距離に合焦し、カメラ GUI で焦点距離を変更 |
| Chromatic aberration | 画面端の高コントラストな細線の色ずれ |
| Lens distortion | 柱と柵の直線が曲がる様子 |
| Vignetting | 白い球１つと床を使い、固定照明下での画面中央と四隅の明るさを比較 |
| Temporal anti-aliasing | 細い柵と生成した移動機体。ちらつきと履歴の残り方 |
| DLSS reconstruction | 同じ細線を DLSS Quality / ネイティブ TAA で比較。非対応 GPU は理由を表示 |
| Frustum culling | ７つの独立したコートを横に配置。視野外のインスタンス除外を比較。メッシュレット・遮蔽判定は無効 |
| Spatial meshlet culling | コート内の小柱を部分的に視野外へ出し、除外三角形数・描画範囲を確認。遮蔽判定は無効 |
| Hardware occlusion queries | 大きな遮蔽壁の後ろに208本の小柱。静止してクエリ結果を待ち、壁の横へ移動して比較 |
| Dynamic sky / sun | 時刻の進行による空・太陽方向・影の変化 |
| glTF node animation | 生成した機体・移動／回転キーフレームによる glTF アニメーション。Animate demonstration で一時停止 |

全ショーケースのモデルは生成した専用 glTF です。既存の Indoor や外部の機体モデルには依存しません。入場前のポストプロセス設定を一時ファイルに退避し、退出時に復元します。比較設定は `data/showcases/*_off.json` と通常の JSON に分離しています。各設定は対象の技術に不要なフォグやレンズ効果を止めています。

## 生成・検証

専用 glTF は外部ダウンロードを使わず Node.js で生成します。発光強度には既存ローダーが対応する `KHR_materials_emissive_strength` を使っています。

```powershell
node tools/generate_technology_showcases.js
node tools/validate_technology_showcases.js
```

生成物は同梱済みです。実行時に Node.js は不要です。DLSS と各 GI の GPU 初期化に失敗した場合は、対象シーンに理由を表示します。反射プローブはキャプチャ完了まで数フレーム必要です。

## 全シーンの描画検証

```powershell
.\x64\Release\AtmosFiore.exe --showcase-audit
```

GUI とカメラ入力を止めた固定条件で全シーンの ON/OFF を90フレームずつ描画し、`data/showcases/audit/` に PNG と `report.json` を出力して終了します。記録内容は画像保存の HRESULT、モデルの画面占有率、明るさ、ON/OFF の平均画素差、初期化エラーです。DLSS 非対応時は通常描画で継続します。画像による確認も併用してください。
