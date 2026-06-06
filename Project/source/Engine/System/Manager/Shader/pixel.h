#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <string>

#include "Engine/system/graphics_core.h"
#include "Engine/utilities/misc.h"
#include "shader_base.h"

using Microsoft::WRL::ComPtr;

/**
 * @brief ピクセルシェーダークラス
 *
 * ラスタライズ後のピクセル単位で実行され、
 * 最終的な色（および深度）を出力するシェーダー。
 *
 * @remark
 * パイプライン構成：
 * Vertex Shader → (Hull/Domain/Geometry) → Pixel Shader
 *
 * 主な役割：
 * - ライティング計算（PBR / Blinn-Phong など）
 * - テクスチャサンプリング
 * - ポストプロセス
 *
 * @note
 * - 出力はRenderTarget（RTV）へ書き込まれる
 * - 複数RenderTarget（MRT）にも対応可能
 *
 * @warning
 * - SRVとRTVの同時バインドは禁止（同一リソース）
 * - 分岐（if文）が多いとパフォーマンス低下
 * - overdraw（重なり描画）に弱い
 *
 * @todo
 * - リフレクションによる自動バインド
 * - PSバリエーション管理（Forward/Deferred）
 * - 動的シェーダー切り替え最適化
 */
class Pixel_Shader : public Shader_Base {
public:
	Pixel_Shader() = default;
	~Pixel_Shader() override = default;

	/**
	 * @brief シェーダーロード
	 *
	 * @param hlslPath HLSLファイル（デバッグ用）
	 * @param csoPath コンパイル済みバイナリ（リリース用）
	 * @param entry エントリーポイント関数名
	 * @param target シェーダーモデル（例: ps_5_0）
	 *
	 * @return 成功時 true
	 *
	 * @note
	 * - デバッグ時はHLSLをコンパイル
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
		log_printf("PS: HLSL コンパイル中... %ls\n", LogLevel::Info, hlslPath.c_str());

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
		log_printf("PS: CSO 読み込み... %ls\n", LogLevel::Info, csoPath.c_str());

		hr = D3DReadFileToBlob(csoPath.c_str(), &m_blob);
		if (FAILED(hr)) {
			log_printf("PS CSO 読み込み失敗\n", LogLevel::Error);
			return false;
		}
#endif

		m_ps.Reset();

		hr = device->CreatePixelShader(
			m_blob->GetBufferPointer(),
			m_blob->GetBufferSize(),
			nullptr,
			&m_ps
		);

		if (FAILED(hr)) return false;

		log_printf("Pixel Shader 作成成功\n", LogLevel::Success);
		return true;
	}

	/**
	 * @brief ピクセルシェーダー取得
	 *
	 * @return ID3D11PixelShader*
	 */
	ID3D11PixelShader* get() const { return m_ps.Get(); }

private:
	/// ピクセルシェーダー本体
	ComPtr<ID3D11PixelShader> m_ps;

	/// コンパイル済みバイナリ
	ComPtr<ID3DBlob> m_blob;
};