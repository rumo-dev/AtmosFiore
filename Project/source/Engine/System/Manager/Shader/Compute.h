#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>

#include "Engine/system/graphics_core.h"
#include "Engine/utilities/misc.h"
#include "shader_base.h"

/**
 * @brief コンピュートシェーダークラス
 *
 * Direct3D11のCompute Shaderをロード・生成・管理する。
 *
 * @remark
 * - デバッグビルド時はHLSLからコンパイル
 * - リリースビルド時は事前コンパイル済みCSOをロード
 *
 * @warning
 * - Dispatch前にSRV/UAV/CBVの設定が必要
 * - UAVとSRVの同時バインドは禁止（D3D11制約）
 *
 * @todo
 * - リフレクション対応（リソース自動バインド）
 * - シェーダーキャッシュ機構
 * - 非同期コンパイル
 */
class Compute_Shader : public Shader_Base {
public:

	/**
	 * @brief シェーダーロード
	 *
	 * @param hlslPath HLSLファイルパス（デバッグ用）
	 * @param csoPath  コンパイル済みバイナリ（リリース用）
	 * @param entry    エントリーポイント
	 * @param target   シェーダーモデル（例: cs_5_0）
	 *
	 * @return 成功した場合 true
	 *
	 * @note
	 * デバッグ時はD3DCompileFromFileを使用
	 * リリース時はD3DReadFileToBlobを使用
	 */
	bool load(
		const std::wstring& hlslPath,
		const std::wstring& csoPath,
		const std::string& entry,
		const std::string& target) override
	{
		ID3D11Device* device = Graphics_Core::instance().get_device();
		if (!device) return false;

		m_hlslPath = hlslPath;
		m_csoPath = csoPath;
		m_entry = entry;
		m_target = target;

		HRESULT hr;

#ifdef _DEBUG
		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG;
		ComPtr<ID3DBlob> error;

		hr = D3DCompileFromFile(
			hlslPath.c_str(),
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entry.c_str(),
			target.c_str(),
			flags,
			0,
			&m_blob,
			&error
		);

		if (FAILED(hr)) {
			if (error) OutputCompileErrors(error.Get());
			return false;
		}
#else
		hr = D3DReadFileToBlob(csoPath.c_str(), &m_blob);
		if (FAILED(hr)) return false;
#endif

		m_cs.Reset();

		hr = device->CreateComputeShader(
			m_blob->GetBufferPointer(),
			m_blob->GetBufferSize(),
			nullptr,
			&m_cs
		);

		return SUCCEEDED(hr);
	}

	/**
	 * @brief コンピュートシェーダー取得
	 *
	 * @return ID3D11ComputeShader*
	 */
	ID3D11ComputeShader* get() const { return m_cs.Get(); }

private:
	/// コンピュートシェーダー本体
	ComPtr<ID3D11ComputeShader> m_cs;

	/// シェーダーバイナリ
	ComPtr<ID3DBlob> m_blob;
};