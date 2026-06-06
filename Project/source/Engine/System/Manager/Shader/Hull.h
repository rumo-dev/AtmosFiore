#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>

#include "Engine/system/graphics_core.h"
#include "Engine/utilities/misc.h"
#include "shader_base.h"

/**
 * @brief ハルシェーダークラス（テッセレーション制御）
 *
 * 入力パッチ（制御点）を受け取り、
 * テッセレーション分割数（tessellation factors）を決定するシェーダー。
 *
 * @remark
 * パイプライン構成：
 * Vertex Shader → Hull Shader → Domain Shader → Pixel Shader
 *
 * 主な役割：
 * - パッチの分割レベル（LOD）を決定
 * - 制御点の加工（Bezier / PN Triangle など）
 *
 * @note
 * - Domain Shaderとセットで使用必須
 * - 出力されるパッチサイズはHS/DSで一致させる必要がある
 *
 * @warning
 * - テッセレーション係数が大きすぎるとパフォーマンスが急激に低下する
 * - トポロジ（PATCHLIST）設定を忘れると描画されない
 *
 * @todo
 * - 距離ベースLOD（カメラ距離でtess factor調整）
 * - リフレクション対応
 * - HS/DSの統合管理
 */
class Hull_Shader : public Shader_Base {
public:
	Hull_Shader() = default;
	~Hull_Shader() override = default;

	/**
	 * @brief シェーダーロード
	 *
	 * @param hlslPath HLSLファイル（デバッグ用）
	 * @param csoPath コンパイル済みファイル（リリース用）
	 * @param entry エントリーポイント
	 * @param target シェーダーモデル（例: hs_5_0）
	 *
	 * @return 成功した場合 true
	 *
	 * @note
	 * - デバッグ時：HLSLをコンパイル
	 * - リリース時：CSOをロード
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

		m_hs.Reset();

		hr = device->CreateHullShader(
			m_blob->GetBufferPointer(),
			m_blob->GetBufferSize(),
			nullptr,
			&m_hs
		);

		return SUCCEEDED(hr);
	}

	/**
	 * @brief ハルシェーダー取得
	 *
	 * @return ID3D11HullShader*
	 */
	ID3D11HullShader* get() const { return m_hs.Get(); }

private:
	/// ハルシェーダー本体
	ComPtr<ID3D11HullShader> m_hs;

	/// シェーダーバイナリ
	ComPtr<ID3DBlob> m_blob;
};