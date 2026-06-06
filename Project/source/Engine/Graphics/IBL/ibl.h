#pragma once
#include <wrl.h>
#include <d3d11.h>
#include <string>

/**
 * @brief Image-Based Lighting（IBL）リソース管理クラス
 *
 * PBR（Physically Based Rendering）で使用する環境マップ群を管理する。
 * HDR環境マップ、拡散IBL、スペキュラIBL、BRDF LUTをまとめて扱う。
 *
 * @remark
 * 本クラスは静的リソース管理を行うユーティリティクラスであり、
 * アプリケーション全体で共有されることを想定している。
 *
 * @warning
 * SRVとして使用するテクスチャは、同時にRTV/UAVとしてバインドしてはいけない（D3D11制約）。
 *
 * @todo
 * - 動的IBL更新（リアルタイムキャプチャ）
 * - キューブマップ対応の明示化
 * - 解像度・フォーマットの外部指定
 * - 異なるBRDFモデル（Charlie, Ashikhminなど）対応
 */
class IBL
{
public:
	/**
	 * @brief IBLで使用するテクスチャ種別
	 */
	enum TextureType {
		Environment = 0,   ///< HDR環境マップ（スカイボックス）
		DiffuseIEM = 1,    ///< 拡散IBL（Irradiance Map）
		SpecularPMREM = 2, ///< スペキュラIBL（Prefiltered Environment Map）
		LUT_GGX = 3,       ///< BRDF LUT（GGX用）
		TextureCount       ///< テクスチャ数
	};

	IBL() {}
	~IBL() {}

	/**
	 * @brief IBLテクスチャを初期化
	 *
	 * 指定パスから各種IBLテクスチャをロードし、SRVとして保持する。
	 *
	 * @param device D3D11デバイス
	 * @param basePath テクスチャファイルのベースパス
	 * @return HRESULT 成功/失敗コード
	 *
	 * @note
	 * 想定されるファイル構成例:
	 * - environment.dds
	 * - diffuse.dds
	 * - specular.dds
	 * - brdf_lut.dds
	 *
	 * @pre
	 * デバイスが有効であること
	 */
	static HRESULT Initialize(
		ID3D11Device* device,
		const std::wstring& basePath);

	/**
	 * @brief IBLテクスチャをシェーダーにバインド
	 *
	 * ピクセルシェーダーに各IBLテクスチャをバインドする。
	 *
	 * @param context デバイスコンテキスト
	 *
	 * @pre
	 * Initialize() が成功していること
	 *
	 * @note
	 * スロット配置はシェーダー側と一致させる必要がある。
	 */
	static void Bind(ID3D11DeviceContext* context);

private:
	/// 各IBLテクスチャのShaderResourceView
	static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srvs[TextureCount];
};