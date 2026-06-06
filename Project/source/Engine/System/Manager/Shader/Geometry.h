#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>

#include "Engine/system/graphics_core.h"
#include "Engine/utilities/misc.h"
#include "shader_base.h"

/**
 * @brief ジオメトリシェーダークラス
 *
 * 頂点/プリミティブを入力として受け取り、
 * 新たな頂点やプリミティブを生成するシェーダー。
 *
 * @remark
 * パイプライン構成：
 * Vertex Shader → (Hull Shader) → (Domain Shader) → Geometry Shader → Pixel Shader
 *
 * 主な用途：
 * - ワイヤーフレーム生成
 * - シャドウボリューム
 * - Billboard生成
 * - パーティクル展開
 *
 * @warning
 * - Geometry Shaderは非常に遅くなりやすい（大量プリミティブ生成で特に顕著）
 * - 大量描画用途には不向き（代替：Instancing / Compute Shader）
 * - Stream Output使用時はバッファ設定が必須
 *
 * @note
 * - 入力プリミティブ（point/line/triangle）に依存
 * - 出力頂点数には制限がある
 *
 * @todo
 * - Stream Output対応
 * - インスタンシング代替パス実装
 * - GS使用のON/OFF切り替え機構
 */
class Geometry_Shader : public Shader_Base {
public:
	Geometry_Shader() = default;
	~Geometry_Shader() override = default;

	/**
	 * @brief シェーダーロード
	 *
	 * @param hlslPath HLSLファイル（デバッグ用）
	 * @param csoPath コンパイル済みファイル（リリース用）
	 * @param entry エントリーポイント
	 * @param target シェーダーモデル（例: gs_5_0）
	 *
	 * @return 成功した場合 true
	 *
	 * @note
	 * デバッグ時はHLSLからコンパイル、リリース時はCSOをロード
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
		log_printf("DEBUG GS: HLSL コンパイル %ls\n", LogLevel::Info, hlslPath.c_str());

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
		log_printf("RELEASE GS: CSO 読み込み %ls\n", LogLevel::Info, csoPath.c_str());

		hr = D3DReadFileToBlob(csoPath.c_str(), &m_blob);
		if (FAILED(hr)) {
			log_printf("GS CSO 読み込み失敗\n", LogLevel::Error);
			return false;
		}
#endif

		m_gs.Reset();

		hr = device->CreateGeometryShader(
			m_blob->GetBufferPointer(),
			m_blob->GetBufferSize(),
			nullptr,
			&m_gs
		);

		if (FAILED(hr)) return false;

		log_printf("Geometry Shader 作成成功\n", LogLevel::Success);
		return true;
	}

	/**
	 * @brief ジオメトリシェーダー取得
	 *
	 * @return ID3D11GeometryShader*
	 */
	ID3D11GeometryShader* get() const { return m_gs.Get(); }

private:
	/// ジオメトリシェーダー本体
	ComPtr<ID3D11GeometryShader> m_gs;

	/// シェーダーバイナリ
	ComPtr<ID3DBlob> m_blob;
};