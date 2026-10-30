# AtmosFiore 開発環境

AtmosFiore は Windows 向けの C++ / Direct3D 11 アプリケーションです。この文書は `Project` フォルダーにあるソリューションをビルドして実行するための手順です。

## 必要な環境

- Windows 10 または 11（64 ビット）
- Visual Studio 2022 の「C++ によるデスクトップ開発」ワークロード（MSVC v143 と Windows 10/11 SDK を含む）
- DirectX 11 対応 GPU。DLSS を使う場合は対応する NVIDIA GPU

ソリューションは Visual Studio 17 形式です。x64 構成は C++20 を使用し、HLSL シェーダーを Shader Model 5.0 でコンパイルします。Windows SDK の指定は `10.0` で、インストール済みの対応バージョンが選択されます。

## プロジェクトの構成

| パス | 内容 |
| --- | --- |
| `AtmosFiore.sln` / `AtmosFiore.vcxproj` | アプリ本体の Visual Studio ソリューションとプロジェクト |
| `source/` | C++ ソースと HLSL シェーダー |
| `data/` | 実行時に読み込むモデル、画像、音声、コンパイル済みシェーダーなど |
| `extentions/DirectXTK-main/` | ソリューションに含まれる DirectX Tool Kit |
| `extentions/NVIDIA-DLSS/` | x64 ビルドで参照する DLSS SDK のヘッダー、ライブラリ、DLL |
| `x64/<構成>/` | x64 ビルドの実行ファイル出力先 |

`extentions` はリポジトリ内の実際のディレクトリ名です。参照パスを変更する際もこの綴りを使用してください。

## ビルドと実行

1. Visual Studio 2022 で `AtmosFiore.sln` を開きます。
2. ソリューション構成を `Debug`、プラットフォームを `x64` にします。
3. ソリューションをビルドします。`DirectXTK_Desktop_2019` はアプリの依存プロジェクトとして先にビルドされます。
4. Visual Studio からデバッグ実行するか、`x64/Debug/AtmosFiore.exe` を起動します。

Visual Studio の Developer PowerShell からは、`Project` フォルダーで次のようにビルドできます。

```powershell
MSBuild .\AtmosFiore.sln /p:Configuration=Debug /p:Platform=x64 /m
```

実行時には `data/shaders/cso` と `data/AtmosFioreLogo.png` が必要です。アプリは実行ファイルの隣を先に確認し、見つからなければ開発用の親フォルダーを探索します。配布用に実行ファイルを移動する場合は、`data` フォルダーを実行ファイルと同じ階層に配置してください。x64 ビルドでは DLSS の `nvngx_dlss.dll` が出力先にコピーされます。

## ビルド構成

| 構成 | 用途 |
| --- | --- |
| `Debug|x64` | 通常の開発とデバッグ |
| `Release|x64` | 最適化した配布用ビルド |
| `RenderDog|x64` | 描画調査用。シェーダーのデバッグ情報を有効化 |
| `Tracy|x64` | `TRACY_ENABLE` を定義するプロファイル用ビルド |

ソリューションには x86 構成もありますが、DLSS SDK と C++20 の設定、および主要なリンク設定は x64 側に記載されています。まず x64 でビルドしてください。

`Release|x64` のビルド後イベントは、`Project` の親フォルダーに `LastBuild/` と `LastBuild.zip` を作成します。既存の同名出力を削除してから `data` と実行ファイルをコピーするため、Release ビルド時はそのフォルダーへの書き込み権限が必要です。このイベントは DLL を配布物へコピーしないため、配布前に必要な DLL を確認してください。Tracy 構成のアプリ側ビルド後イベントは無効です。

## 主な依存関係

- Windows SDK: Direct3D 11、DXGI、Direct2D、DirectWrite
- DirectX Tool Kit: 同梱の `DirectXTK_Desktop_2019.vcxproj` をソリューションからビルド
- NVIDIA DLSS SDK: 同梱の `extentions/NVIDIA-DLSS/` を x64 で参照
- ImGui、Tracy など: ソースが `source/` 以下に含まれる

`DirectXTK.lib` が見つからない場合は、ソリューション全体をビルドし、DirectXTK プロジェクトの同じ構成とプラットフォームのビルド結果を確認してください。`data` が見つからないエラーでは、上記の必須ファイルと実行ファイルからのフォルダー配置を確認してください。

## リリース ZIP の作成

`Release|x64` をビルドした後、`package_release.bat` を実行します。バッチはビルド済みの `x64/Release/AtmosFiore.exe`、現在の `data/`、DLSS DLL、Visual Studio の x64 再頒布用 DLL を集め、`ReleasePackage/AtmosFiore_Release_<日時>_<識別子>.zip` を作成します。バッチ自体はビルドしません。ZIP には PDB や開発用ソースは含めません。
