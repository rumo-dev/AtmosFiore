# glTF PBR の対応と確認シーン

対象は [Khronos glTF PBR](https://www.khronos.org/gltf/pbr/) に掲載されている基本 7 項目と材質拡張 10 項目、計 17 項目です。拡張の factor、色、IOR、厚さ、距離、回転と、それぞれのテクスチャーを読み込み、描画に反映します。glTF 全拡張への対応や適合認証を示すものではありません。

ビルド後、デバッグ GUI の **Render Test Scenes** にある `glTF PBR / Core` と `glTF PBR / Extensions` から選択します。`Find technique` に `glTF` を入力すると絞り込めます。`Technique enabled` は対象項目だけを無効化して比較し、`Reset camera` で開始位置へ戻します。

17 シーンはページ内リンク先の公式比較モデルをそのまま使用します。比較に必要な 2～3 個の球や背景板は公式モデルの構成です。21 個の球を追加してはいません。モデルの実寸を保ち、境界サイズから開始カメラ・クリップ距離を設定しています。

| 項目 / 公式モデル | 読み込む材質パラメーター・テクスチャー | 省略時 |
| --- | --- | --- |
| Alpha Coverage / CompareAlphaCoverage | alphaMode、alphaCutoff、base color の A、doubleSided | OPAQUE、cutoff 0.5、片面 |
| Ambient Occlusion / CompareAmbientOcclusion | occlusionTexture の R、strength | 遮蔽なし、strength 1 |
| Base Color / CompareBaseColor | baseColorFactor、baseColorTexture、COLOR_0 | 白 RGBA、テクスチャーなし |
| Emissive / CompareEmissiveStrength | emissiveFactor、emissiveTexture | 黒、テクスチャーなし |
| Metallic / CompareMetallic | metallicFactor、metallicRoughnessTexture の B | factor 1 |
| Normal / CompareNormal | normalTexture、scale | 頂点法線、scale 1 |
| Roughness / CompareRoughness | roughnessFactor、metallicRoughnessTexture の G | factor 1 |
| Anisotropy / CompareAnisotropy | anisotropyStrength、anisotropyRotation、anisotropyTexture の RG 方向 / B 強度 | strength 0、rotation 0 |
| Clearcoat / CompareClearcoat | clearcoatFactor / Texture(R)、clearcoatRoughnessFactor / Texture(G)、clearcoatNormalTexture / scale | coat 0、roughness 0、normal scale 1 |
| Dispersion / CompareDispersion | dispersion | 0 |
| Emissive Strength / CompareEmissiveStrength | emissiveStrength | 1 |
| IOR / CompareIor | ior（0 の特別な Fresnel 設定を含む） | 1.5 |
| Iridescence / CompareIridescence | iridescenceFactor / Texture(R)、iridescenceIor、iridescenceThicknessMinimum / Maximum、iridescenceThicknessTexture(G) | factor 0、IOR 1.3、100 / 400 nm |
| Sheen / CompareSheen | sheenColorFactor / Texture(RGB)、sheenRoughnessFactor / Texture(A) | 黒、roughness 0 |
| Specular / CompareSpecular | specularFactor / Texture(A)、specularColorFactor / Texture(RGB) | strength 1、白 |
| Transmission / CompareTransmission | transmissionFactor / Texture(R) | 0 |
| Volume / CompareVolume | thicknessFactor / Texture(G)、attenuationDistance、attenuationColor | thickness 0、無限距離、白 |

材質・拡張・個別プロパティーがない場合は既定値を使用します。テクスチャーが指定されない場合は中立値を使用し、不正な参照を配列アクセスしません。primitive の material 自体がない場合にも既定材質を割り当てます。色テクスチャーは sRGB から線形に変換し、法線・強度・粗さ・厚さなどのデータは線形値として扱います。`KHR_materials_unlit` も読み込みます。

テクスチャーでは UV0 / UV1、`KHR_texture_transform` の offset・scale・rotation・texCoord、glTF の min/mag filter と wrapS/wrapT を使用します。モデル全体で大きな sampler インデックスを使用しても、材質単位に再割当します。現在の D3D11 実装は、環境用とは別に材質当たり最大 15 種類の sampler state を使用でき、それを超える組み合わせは既定 sampler にフォールバックします。UV2 以降や圧縮テクスチャー拡張は今回の対象外です。

## モデルの出典

`data/gltf_pbr/manifest.json` にページ URL、公式リポジトリ、固定コミット、ファイル URL、サイズ、SHA-256 を記録しています。取得コミットは `8f492390ddc7a2ae221a1369ef560589cf8c70fb`、モデルは重複を除き 16 種類です。各モデルの LICENSE.md、README、metadata を同梱しています。原本 GLB は改変していません。

`*_missing.glb` は個別パラメーター省略の検証用派生ファイルです。公式モデルのメッシュとバイナリーデータを保ち、対象の材質情報だけを除いています。`default_material_missing.glb` は材質自体を省略した検証用です。これらにも元モデルのライセンスが適用されます。

## 再生成と検証

モデルは同梱済みのため、実行時にダウンロードや Node.js は不要です。再取得する場合のみネットワークを使用します。

```powershell
node tools/fetch_khronos_pbr_samples.js
node tools/prepare_khronos_pbr_scenes.js
node tools/validate_khronos_pbr_models.js
```

再取得スクリプトは取得時点の公式 main を固定し直します。既存の固定モデルを再現する場合は同梱 manifest と原本を使用してください。既存の `generate_technology_showcases.js` は追加した公式モデルのシーン登録を保持します。

```powershell
.\x64\PbrVerify\AtmosFiore.exe --pbr-audit
node tools/check_gltf_pbr_audit.js
powershell -File tools/create_showcase_contact_sheets.ps1 -AuditDirectory data/gltf_pbr/audit

.\x64\PbrVerify\AtmosFiore.exe --showcase-audit
node tools/check_showcase_audit.js
```

監査実行は完了すると自動終了します。`--pbr-audit` は公式 17 シーン・省略 17 ケース・材質なし 1 ケースをそれぞれ ON/OFF で撮影します。70 枚の画像と `data/gltf_pbr/audit/report.json` によって、読み込みエラー、キャプチャー結果、露出、描画差を検証します。透過モデルは不透明 G-buffer の面積が小さくなるため、最終画像も確認してください。`--showcase-audit` は既存 32 シーンを含む 49 シーン、98 枚を撮影します。

`tools/test_gltf_pbr_material.cpp` は CPU 側の既定値・空拡張・未指定テクスチャー・UV transform と、公式／省略モデルの材質情報を検査する小さな独立テストです。

Visual Studio の Developer PowerShell から次のように再実行できます。

```powershell
New-Item -ItemType Directory -Force x64/PbrVerify | Out-Null
cl /nologo /std:c++20 /EHsc /MD /I source tools/test_gltf_pbr_material.cpp /Fox64/PbrVerify/test_material.obj /Fex64/PbrVerify/test_material.exe
.\x64\PbrVerify\test_material.exe
```

## 描画方式と制約

拡張材質は不透明の deferred lighting 後に forward で評価し、透過材質は専用パスで評価します。通常の G-buffer にも UV 選択・変換、頂点色、alpha、AO 強度を反映します。不透明の再描画には同一深度を許可し、透過用背景には別の HDR mip chain を使うため、書き込み中のレンダーターゲットを読みません。

屈折は画面内の不透明背景を参照し、画面外は環境マップにフォールバックします。多重屈折・透過物同士の正確な重なりを解く方式ではありません。Volume はモデルの実寸とワールドスケールを使った Beer–Lambert 減衰です。Dispersion は RGB で異なる IOR を使います。薄膜干渉は代表 RGB 波長による近似、sheen の環境応答も近似です。拡張材質の forward パスでは面光源を立体角で近似し、点光源の影は未対応です。パラメーターが描画へ反映されることと、Khronos の参照画像とピクセル単位で一致することは区別してください。
