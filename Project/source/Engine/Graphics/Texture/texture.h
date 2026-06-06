#pragma once
#include <WICTextureLoader.h>
using namespace DirectX;

#include <wrl.h>
using namespace Microsoft::WRL;

#include <string>
#include <sstream>
#include <map>
using namespace std;

#include "texture.h"
#include "Engine/Utilities/misc.h"

/**
 * @brief ファイルからテクスチャをロード
 *
 * WICを使用して画像ファイル（PNG, JPG, BMP等）を読み込み、
 * ShaderResourceViewを生成する。
 *
 * @param device D3D11デバイス
 * @param filename ファイルパス
 * @param shader_resource_view 出力SRV
 * @param texture2d_desc テクスチャ情報（任意）
 * @return HRESULT 成功/失敗コード
 *
 * @remark
 * - 内部でキャッシュされる可能性あり（実装依存）
 * - sRGB変換はファイルや実装に依存
 *
 * @warning
 * SRVとして使用中のテクスチャをRTV/UAVとして同時に使用してはならない（D3D11制約）
 *
 * @todo
 * - DDS対応（DirectXTex）
 * - sRGB指定ロード
 * - MipMap自動生成
 */
HRESULT load_texture_from_file(
	ID3D11Device* device,
	const wchar_t* filename,
	ID3D11ShaderResourceView** shader_resource_view,
	D3D11_TEXTURE2D_DESC* texture2d_desc);

/**
 * @brief 全テクスチャの解放
 *
 * 内部キャッシュされたテクスチャをすべて解放する。
 *
 * @note
 * アプリケーション終了時やリソースリロード時に使用
 */
void release_all_textures();

/**
 * @brief ダミーテクスチャ生成
 *
 * 単色テクスチャを生成する。
 * テクスチャ未ロード時のフォールバックとして使用可能。
 *
 * @param device D3D11デバイス
 * @param shader_resource_view 出力SRV
 * @param value 色（0xAABBGGRR形式）
 * @param dimension テクスチャサイズ（正方形）
 *
 * @note
 * 小さいサイズ（例: 1x1, 4x4）で十分
 */
HRESULT make_dummy_texture(
	ID3D11Device* device,
	ID3D11ShaderResourceView** shader_resource_view,
	DWORD value,
	UINT dimension);

/**
 * @brief メモリからテクスチャをロード
 *
 * バイナリデータ（画像ファイル内容）からテクスチャを生成する。
 *
 * @param device D3D11デバイス
 * @param data 画像データ
 * @param size データサイズ
 * @param shader_resource_view 出力SRV
 * @return HRESULT 成功/失敗コード
 *
 * @remark
 * - glTFやアーカイブ展開時に使用される
 *
 * @warning
 * データは有効な画像フォーマットである必要がある
 */
HRESULT load_texture_from_memory(
	ID3D11Device* device,
	const void* data,
	size_t size,
	ID3D11ShaderResourceView** shader_resource_view);