#pragma once
#include <d3d11.h>
#include <wrl.h>

/// Gバッファのレンダーターゲット数（位置・法線・アルベド・マテリアルなど）
#define GBUFFER_COUNT  4

/**
 * @brief ジオメトリバッファ（G-buffer）
 *
 * 遅延レンダリング（Deferred Rendering）用の複数レンダーターゲットを管理するクラス。
 * 各ピクセルに対してジオメトリ情報（法線・アルベド・深度など）を格納する。
 *
 * @warning
 * D3D11の仕様上、同一リソースをSRVとRTVとして同時にバインドすることは禁止されている。
 * ライティングパスでSRVとして使用する前に、必ずレンダーターゲットからアンバインドすること。
 *
 * @remark
 * - MRT（Multiple Render Targets）を使用（最大 GBUFFER_COUNT）
 * - 現在はMSAA未対応（SampleDesc.Count = 1前提）
 *
 * @todo
 * - MSAA対応（Resolve処理）
 * - 各RTのフォーマットを柔軟化（現在は固定想定）
 * - 可変GBuffer数対応
 * - 圧縮フォーマット（例: R10G10B10A2）対応
 */
class GeometryBuffer
{
public:

	/**
	 * @brief コンストラクタ
	 *
	 * G-buffer用の複数レンダーターゲットと深度バッファを生成する。
	 *
	 * @param device D3D11デバイス
	 * @param width バッファ幅
	 * @param height バッファ高さ
	 */
	GeometryBuffer(ID3D11Device* device, UINT width, UINT height);

	/**
	 * @brief デストラクタ
	 *
	 * 内部リソースを解放する。
	 *
	 * @note
	 * nullptrチェックを行っていないため、
	 * 生成失敗時に安全ではない可能性がある。
	 */
	virtual ~GeometryBuffer()
	{
		for (size_t render_target_index = 0; render_target_index < GBUFFER_COUNT; ++render_target_index)
		{
			if (m_renderTargetBuffers[render_target_index]) m_renderTargetBuffers[render_target_index]->Release();
			if (m_renderTargetViews[render_target_index]) m_renderTargetViews[render_target_index]->Release();
			if (m_renderTargetShaderResourceViews[render_target_index]) m_renderTargetShaderResourceViews[render_target_index]->Release();
		}
		if (m_depthStencilBuffer) m_depthStencilBuffer->Release();
		if (m_depthStencilView) m_depthStencilView->Release();
		if (m_depthStencilShaderResourceView) m_depthStencilShaderResourceView->Release();
	}

	/**
	 * @brief G-bufferをクリア
	 *
	 * @param immediate_context デバイスコンテキスト
	 */
	void Clear(ID3D11DeviceContext* immediate_context);

	/**
	 * @brief G-bufferをレンダーターゲットとして設定
	 *
	 * 現在のレンダーターゲットとビューポートを保存し、
	 * G-bufferへの描画を開始する。
	 *
	 * @param immediate_context デバイスコンテキスト
	 */
	void Activate(ID3D11DeviceContext* immediate_context);

	/**
	 * @brief G-bufferの使用終了
	 *
	 * activate()前の状態を復元する。
	 *
	 * @param immediate_context デバイスコンテキスト
	 */
	void Deactivate(ID3D11DeviceContext* immediate_context);

	/**
	 * @brief SRV配列を取得
	 *
	 * ライティングパスで使用するためのG-buffer SRVを取得する。
	 *
	 * @param out_srvs 出力SRV配列
	 * @param max_count 最大取得数
	 * @param start_index 開始インデックス
	 *
	 * @pre
	 * これらのSRVは現在レンダーターゲットとしてバインドされていないこと
	 */
	void GetShaderResourceViews(
		ID3D11ShaderResourceView** out_srvs,
		UINT max_count = GBUFFER_COUNT,
		UINT start_index = 0
	) const
	{
		const UINT count = (max_count < GBUFFER_COUNT) ? max_count : GBUFFER_COUNT;
		for (UINT i = start_index; i < (start_index + count); ++i)
		{
			out_srvs[i] = m_renderTargetShaderResourceViews[i];
		}
	}
	ID3D11DepthStencilView* GetDepthStencilView() const { return m_depthStencilView; }
private:
	/// キャッシュするビューポート数
	UINT m_cachedViewportCount = { D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE };

	/// キャッシュされたビューポート
	D3D11_VIEWPORT m_cachedViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];

	/// キャッシュされたRTV
	ID3D11RenderTargetView* m_cachedRenderTargetViews[GBUFFER_COUNT];

	/// キャッシュされたDSV
	ID3D11DepthStencilView* m_cachedDepthStencilView;

	/// カラーバッファ（G-buffer各要素）
	ID3D11Texture2D* m_renderTargetBuffers[GBUFFER_COUNT] = {};

	/// レンダーターゲットビュー（描画用）
	ID3D11RenderTargetView* m_renderTargetViews[GBUFFER_COUNT] = {};

	/// シェーダーリソースビュー（ライティング用）
	ID3D11ShaderResourceView* m_renderTargetShaderResourceViews[GBUFFER_COUNT] = {};

	/// 深度バッファ
	ID3D11Texture2D* m_depthStencilBuffer = {};

	/// 深度ステンシルビュー
	ID3D11DepthStencilView* m_depthStencilView = {};

	/// 深度のSRV（シャドウ・ライティング用途）
	ID3D11ShaderResourceView* m_depthStencilShaderResourceView = {};

	/// ビューポート
	D3D11_VIEWPORT m_viewport = {};
};