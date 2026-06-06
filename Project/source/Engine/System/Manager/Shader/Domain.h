#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>

#include "Engine/system/graphics_core.h"
#include "Engine/utilities/misc.h"
#include "shader_base.h"

/**
 * @brief ドメインシェーダークラス（テッセレーション）
 *
 * Hull Shaderによって分割されたパッチを元に、
 * 最終頂点位置を生成するシェーダー。
 *
 * @remark
 * パイプライン構成：
 * Vertex Shader → Hull Shader → Domain Shader → Pixel Shader
 *
 * @note
 * - Domain Shader単体では動作せず、Hull Shaderとセットで使用する必要がある
 * - 主に曲面分割（Bezier / PN Triangle / Displacement）で使用される
 *
 * @warning
 * - HS/DSの不一致（パッチサイズなど）は描画崩壊の原因となる
 * - InputLayoutはDSには影響しない（VS入力のみ）
 *
 * @todo
 * - リフレクション対応（パッチサイズ取得）
 * - テッセレーション設定の自動化
 * - キャッシュ機構
 */
class Domain_Shader : public Shader_Base {
public:

	/**
	 * @brief シェーダーロード
	 *
	 * @param hlslPath HLSLファイルパス（デバッグ用）
	 * @param csoPath コンパイル済みファイル（リリース用）
	 * @param entry エントリーポイント
	 * @param target シェーダーモデル（例: ds_5_0）
	 *
	 * @return 成功した場合 true
	 *
	 * @note
	 * - デバッグ時はHLSLからコンパイル
	 * - リリース時はCSOを読み込み
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
		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
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

		m_ds.Reset();

		hr = device->CreateDomainShader(
			m_blob->GetBufferPointer(),
			m_blob->GetBufferSize(),
			nullptr,
			&m_ds
		);

		return SUCCEEDED(hr);
	}

	/**
	 * @brief ドメインシェーダー取得
	 *
	 * @return ID3D11DomainShader*
	 */
	ID3D11DomainShader* get() const { return m_ds.Get(); }

private:
	/// ドメインシェーダー本体
	ComPtr<ID3D11DomainShader> m_ds;

	/// シェーダーバイナリ
	ComPtr<ID3DBlob> m_blob;
};