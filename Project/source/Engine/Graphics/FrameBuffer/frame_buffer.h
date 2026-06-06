#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <cstdint>

/**
 * @brief オフスクリーン描画用フレームバッファクラス（Direct3D 11）
 *
 * カラーバッファおよびオプションで深度・ステンシルバッファを管理する。
 * DXGI_FORMAT に基づいたレンダーターゲット／深度バッファを生成し、
 * シェーダーから参照可能な ShaderResourceView も提供する。
 */
class Framebuffer
{
public:
	/**
	 * @brief コンストラクタ
	 *
	 * @param device D3D11デバイス
	 * @param width フレームバッファの幅
	 * @param height フレームバッファの高さ
	 * @param format カラーバッファのDXGIフォーマット（例: DXGI_FORMAT_R8G8B8A8_UNORM）
	 * @param use_depth 深度バッファを使用するか
	 * @param use_stencil ステンシルバッファを使用するか
	 *
	 * @note 深度・ステンシルを使用する場合、対応するDXGIフォーマット
	 *       （例: DXGI_FORMAT_D24_UNORM_S8_UINT）が内部で使用される。
	 */
	Framebuffer(ID3D11Device* device,
		uint32_t width,
		uint32_t height,
		DXGI_FORMAT format,
		int mip_levels = 1,
		bool use_depth = false,
		bool use_stencil = false);

	Framebuffer() = default;
	virtual ~Framebuffer() = default;

	/**
	 * @brief フレームバッファをクリアする
	 *
	 * @param immediate_context デバイスコンテキスト
	 * @param r 赤成分（0.0?1.0）
	 * @param g 緑成分（0.0?1.0）
	 * @param b 青成分（0.0?1.0）
	 * @param a アルファ成分（0.0?1.0）
	 * @param depth 深度値（通常 1.0）
	 */
	void Clear(ID3D11DeviceContext* immediate_context,
		float r = 0, float g = 0, float b = 0, float a = 1,
		float depth = 1);

	/**
	 * @brief フレームバッファをアクティブにする
	 *
	 * 指定されたフレームバッファをレンダーターゲットとしてセットする。
	 * 既存のレンダーターゲットとビューポートは内部に保存される。
	 *
	 * @param immediate_context デバイスコンテキスト
	 * @param adopted_depth_stencil_view 外部のDSVを使用する場合に指定（nullptrで内部DSVを使用）
	 */
	void Activate(ID3D11DeviceContext* immediate_context,
		ID3D11DepthStencilView* adopted_depth_stencil_view = nullptr);

	/**
	 * @brief フレームバッファを非アクティブ化する
	 *
	 * activate() 呼び出し前のレンダーターゲットとビューポート状態を復元する。
	 *
	 * @param immediate_context デバイスコンテキスト
	 */
	void Deactivate(ID3D11DeviceContext* immediate_context);

public:
	/**
	 * @brief カラーバッファのShaderResourceViewを取得
	 *
	 * @return ID3D11ShaderResourceView*
	 *
	 * @note DXGIフォーマットに応じてシェーダーから参照可能（例: UNORM → float）
	 */
	ID3D11ShaderResourceView* GetColorMap() { return m_colorMap.Get(); }

	/**
	 * @brief カラーバッファSRVのアドレスを取得
	 *
	 * @return ID3D11ShaderResourceView**
	 */
	ID3D11ShaderResourceView** GetColorMapAddress()
	{
		return m_colorMap.GetAddressOf();
	}

	/**
	 * @brief 深度バッファのShaderResourceViewを取得
	 *
	 * @return ID3D11ShaderResourceView*
	 *
	 * @note Typelessフォーマット（例: DXGI_FORMAT_R24_UNORM_X8_TYPELESS）から生成されることが多い
	 */
	ID3D11ShaderResourceView* GetDepthMap() { return m_depthMap.Get(); }

	/**
	 * @brief 深度バッファSRVのアドレスを取得
	 *
	 * @return ID3D11ShaderResourceView**
	 */
	ID3D11ShaderResourceView** GetDepthMapAddress()
	{
		return m_depthMap.GetAddressOf();
	}

	/// コピー禁止
	Framebuffer(const Framebuffer&) = delete;
	Framebuffer& operator=(const Framebuffer&) = delete;

	/// ムーブ許可
	Framebuffer(Framebuffer&&) = default;
	Framebuffer& operator=(Framebuffer&&) = default;

private:
	/// 保存するビューポート数
	UINT m_viewportCount{ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE };

	/// 以前のビューポートをキャッシュ
	D3D11_VIEWPORT m_cachedViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];

	/// 以前のレンダーターゲットビュー
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_cachedRenderTargetView;

	/// 以前の深度ステンシルビュー
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_cachedDepthStencilView;

	/// カラーバッファのShaderResourceView
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_colorMap;

	/// 深度バッファのShaderResourceView
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_depthMap;

	/// レンダーターゲットビュー（RTV）
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;

	/// 深度ステンシルビュー（DSV）
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencilView;

	/// ビューポート設定
	D3D11_VIEWPORT m_viewport;
};