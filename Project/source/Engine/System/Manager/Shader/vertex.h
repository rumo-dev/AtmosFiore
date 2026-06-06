#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>   // Reflection 用
#include <wrl/client.h>
#include <string>
#include <vector>

#include "Engine/system/graphics_core.h"
#include "Engine/utilities/misc.h"
#include "shader_base.h"

using Microsoft::WRL::ComPtr;

/**
 * @brief Direct3D11 Vertex Shader 管理クラス
 *
 * @details
 * HLSL またはコンパイル済み CSO から頂点シェーダーを生成し、
 * 必要に応じて InputLayout の作成も行う。
 *
 * @note
 * - Debug ビルドでは HLSL を毎回コンパイルする（ホットリロード用途）
 * - Release ビルドでは CSO を読み込む（高速化）
 * - InputLayout はシェーダーのバイトコード（BLOB）に依存するため、
 *   Shader 作成後に生成する必要があります
 *
 * - m_blob は以下の用途で保持されます：
 *   - VertexShader 作成
 *   - InputLayout 作成
 *   - （必要なら Reflection）
 *
 * @warning
 * - InputLayout は「シェーダーの入力シグネチャ」と一致しないと失敗します
 *   （semantic 名 / format / slot など）
 *
 * - load() 実行前に create_input_layout() を呼ぶと必ず失敗します
 *
 * - Release ビルドで CSO が存在しない場合、ロードは失敗します
 *
 * - Graphics_Core の Device が未初期化の場合、すべて失敗します
 *
 * @code
 * Vertex_Shader vs;
 *
 * // ロード（Debug: HLSL / Release: CSO）
 * vs.load(
 *     L"./hlsl/basic_vs.hlsl",
 *     L"./cso/basic_vs.cso",
 *     "main",
 *     "vs_5_0"
 * );
 *
 * // InputLayout 作成
 * D3D11_INPUT_ELEMENT_DESC layout[] =
 * {
 *     { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
 *       D3D11_INPUT_PER_VERTEX_DATA, 0 },
 *
 *     { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12,
 *       D3D11_INPUT_PER_VERTEX_DATA, 0 }
 * };
 *
 * vs.create_input_layout(layout, _countof(layout));
 *
 * // 使用
 * context->IASetInputLayout(vs.get_input_layout());
 * context->VSSetShader(vs.get(), nullptr, 0);
 * @endcode
 */
class Vertex_Shader : public Shader_Base {
public:

	Vertex_Shader() = default;
	~Vertex_Shader() override = default;

	/**
	 * @brief シェーダーをロード
	 *
	 * @param hlslPath HLSLファイルパス
	 * @param csoPath  コンパイル済みCSOパス
	 * @param entry    エントリポイント（通常 "main"）
	 * @param target   シェーダーモデル（例: vs_5_0）
	 *
	 * @return 成功時 true / 失敗時 false
	 *
	 * @note
	 * Debug と Release で挙動が異なります：
	 * - Debug   : HLSL をコンパイル
	 * - Release : CSO をロード
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
		log_printf("VS: HLSL コンパイル中... %ls\n", LogLevel::Info, hlslPath.c_str());

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
		log_printf("VS: CSO 読み込み... %ls\n", LogLevel::Info, csoPath.c_str());

		hr = D3DReadFileToBlob(csoPath.c_str(), &m_blob);
		if (FAILED(hr)) {
			log_printf("VS CSO 読み込み失敗: %ls\n", LogLevel::Error, csoPath.c_str());
			return false;
		}
#endif

		// 既存リソース解放
		m_vs.Reset();

		hr = device->CreateVertexShader(
			m_blob->GetBufferPointer(),
			m_blob->GetBufferSize(),
			nullptr,
			&m_vs
		);

		if (FAILED(hr)) return false;

		log_printf("VS 作成成功\n", LogLevel::Success);

		return true;
	}

	/**
	 * @brief ネイティブ VertexShader を取得
	 * @return ID3D11VertexShader*
	 */
	ID3D11VertexShader* get() const { return m_vs.Get(); }

	/**
	 * @brief バイトコード（BLOB）を取得
	 * @return ID3DBlob*
	 *
	 * @note InputLayout 作成時に必要
	 */
	ID3DBlob* get_byte_code() const { return m_blob.Get(); }

	/**
	 * @brief InputLayout を取得
	 * @return ID3D11InputLayout*
	 */
	ID3D11InputLayout* get_input_layout() const { return m_inputLayout.Get(); }

	/**
	 * @brief InputLayout を生成
	 *
	 * @param input_element_desc 頂点レイアウト配列
	 * @param element_count 要素数
	 *
	 * @return 成功時 true
	 *
	 * @warning
	 * - シェーダーの入力シグネチャと一致していないと失敗します
	 * - load() 後に呼び出す必要があります
	 */

	bool create_input_layout(
		const D3D11_INPUT_ELEMENT_DESC* input_element_desc,
		UINT element_count)
	{
		ID3D11Device* device = Graphics_Core::instance().get_device();

		if (!device || !m_blob) {
			log_printf("VS: InputLayout 生成失敗（デバイスまたは BLOB が無効）\n", LogLevel::Error);
			return false;
		}

		HRESULT hr = device->CreateInputLayout(
			input_element_desc,
			element_count,
			m_blob->GetBufferPointer(),
			m_blob->GetBufferSize(),
			&m_inputLayout
		);

		if (SUCCEEDED(hr)) {
			log_printf("VS: InputLayout 生成成功\n", LogLevel::Success);
			return true;
		}
		else {
			log_printf("VS: InputLayout 生成失敗 (HRESULT=0x%08X)\n", LogLevel::Error, hr);
			return false;
		}
	}

private:
	ComPtr<ID3D11VertexShader> m_vs;     ///< VertexShader本体
	ComPtr<ID3DBlob> m_blob;             ///< バイトコード（コンパイル結果）
	ComPtr<ID3D11InputLayout> m_inputLayout; ///< 入力レイアウト
};