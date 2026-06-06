#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <cstdint>

/**
 * @brief フルスクリーンクアッド描画クラス
 *
 * 画面全体に対してテクスチャを描画するためのユーティリティ。
 * 主にポストプロセス（ブラー、トーンマッピングなど）や
 * フレームバッファのブリット処理に使用する。
 *
 * @warning
 * D3D11の仕様上、同一リソースをSRVとRTVとして同時にバインドすることは禁止されている。
 * 入力として使用するSRVが現在レンダーターゲットに設定されている場合、
 * 必ず事前にアンバインドすること。
 *
 * @remark
 * 現在はMSAA（マルチサンプルアンチエイリアシング）には未対応。
 * SampleDesc.Count = 1 固定の前提で動作する。
 *
 * @todo
 * - MipMap対応（GenerateMipsまたは手動生成）
 * - MSAA対応（Resolve処理の追加）
 * - 複数レンダーターゲット（MRT）対応
 */
class FullscreenQuad
{
public:
	/**
	 * @brief コンストラクタ
	 *
	 * 内部で使用する頂点シェーダーおよびピクセルシェーダーを生成する。
	 *
	 * @param device D3D11デバイス
	 */
	FullscreenQuad(ID3D11Device* device);

	virtual ~FullscreenQuad() = default;
	/**
 * @brief テクスチャを画面に描画（ブリット）
 *
 * 指定されたShaderResourceViewを入力としてフルスクリーン描画を行う。
 * ピクセルシェーダーを差し替えることでポストプロセスも可能。
 *
 * @param immediate_context デバイスコンテキスト
 * @param shader_resource_view 使用するSRV配列
 * @param start_slot SRVの開始スロット
 * @param num_views バインドするSRV数
 * @param replaced_pixel_shader 使用するピクセルシェーダー（nullptrで内部シェーダー使用）
 */
	void Blit(ID3D11DeviceContext* immediate_context,
		ID3D11ShaderResourceView** shader_resource_view,
		uint32_t start_slot,
		uint32_t num_views,
		ID3D11PixelShader* replaced_pixel_shader = nullptr);
private:
	/// 内部頂点シェーダー（フルスクリーンクアッド用）
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_embeddedVertexShader;

	/// 内部ピクセルシェーダー（単純コピー用など）
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_embeddedPixelShader;


};