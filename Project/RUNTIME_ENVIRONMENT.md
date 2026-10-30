# AtmosFiore 動作環境（`exe` と `data` のみで配布する場合）

この文書は、`Release|x64` でビルドした `AtmosFiore.exe` と `data` フォルダーだけを配布する場合の条件を示します。開発環境の構築手順は [README.md](README.md) を参照してください。

## 配布物の配置

```text
AtmosFiore/
├─ AtmosFiore.exe
└─ data/
   ├─ AtmosFioreLogo.png
   ├─ shaders/
   │  └─ cso/
   └─ ...
```

`data` は中身を省かずにフォルダーごとコピーしてください。起動時に `data/shaders/cso` と `data/AtmosFioreLogo.png` を確認します。実行ファイルの隣に `data` があれば、別の作業フォルダーから起動してもそこを使用します。

## 配布先の動作条件

| 項目 | 条件 |
| --- | --- |
| OS | Windows 10 / 11、64 ビット |
| GPU | Direct3D 11 に対応する GPU とドライバー |
| ランタイム | Microsoft Visual C++ 再頒布可能パッケージ（x64）。`MSVCP140.dll`、`VCRUNTIME140.dll`、`VCRUNTIME140_1.dll` などを提供するもの |
| Windows コンポーネント | Direct3D 11、DXGI、DirectWrite、Direct2D、XAudio2.9、`D3DCOMPILER_47.dll` が使用可能であること |

Visual Studio や Windows SDK は配布先には不要です。Visual C++ ランタイムが入っていない PC では、アプリを起動する前に Microsoft の x64 再頒布可能パッケージをインストールしてください。`exe` と `data` の二点だけで配布するため、ランタイム DLL を配布フォルダーに同梱する手順は含めません。

## DLSS について

`exe` と `data` のみの配布には `nvngx_dlss.dll` が含まれません。そのため、この構成で DLSS が使えることは保証できません。DLSS を必須機能として提供する場合は、DLSS の必要ファイルを含む別の配布構成を用意してください。通常の描画には DLSS 対応 GPU は必須ではありません。

## GPU の選択

新しい Release ビルドでは、複数の GPU がある PC で高性能 GPU を優先して Direct3D 11 デバイスを作成します。利用できない場合は、ほかの GPU、最後に Windows の既定の GPU を試します。変更を配布先に反映するには、新しくビルドした `AtmosFiore.exe` に差し替えてください。

配布先でも Intel GPU が選ばれる場合は、Windows の「設定」→「システム」→「ディスプレイ」→「グラフィック」で `AtmosFiore.exe` を追加し、「オプション」から「高パフォーマンス」を選択してください。NVIDIA GPU が無効、ドライバー未導入、または Direct3D 11 デバイスを作成できない状態では使用できません。

## 配布前の確認

1. `x64/Release/AtmosFiore.exe` と `data` フォルダー全体を、上記の配置で別フォルダーにコピーします。
2. Visual C++ 再頒布可能パッケージ（x64）を備えた Windows 10 / 11 の PC で起動します。
3. タイトル画面と描画、音声、シーン遷移を確認します。起動できない場合は、不足 DLL の表示、GPU ドライバー、`data` の配置を確認します。

この文書の依存 DLL は `x64/Release/AtmosFiore.exe` の PE インポート情報を基にしています。実際の配布先での動作確認は別途必要です。
