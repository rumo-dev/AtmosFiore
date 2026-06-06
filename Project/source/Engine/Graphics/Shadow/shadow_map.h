#pragma once

// UNIT.99
#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include <cstdint>
#include <functional>

#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"

/**
 * @brief シャドウマップクラス
 *
 * ライト視点からの深度情報を記録するための深度テクスチャを管理する。
 * 生成した深度はシェーダーからSRVとして参照され、シャドウ判定に使用される。
 *
 * @remark
 * - 深度専用レンダリング（カラーRTVは使用しない）
 * - 通常はTypelessフォーマット → DSV/SRVで解釈分岐
 *
 * @warning
 * - 同一リソースをDSVとSRVとして同時にバインドしてはならない（D3D11制約）
 * - シャドウアクネやピーターパン対策（Depth Bias等）は別途必要
 *
 * @todo
 * - PCF（Percentage Closer Filtering）対応
 * - VSM / ESM対応
 * - カスケードシャドウマップ（CSM）
 * - 解像度可変 / 動的リサイズ
 */
class shadow_map
{
	/// 深度ステンシルビュー（描画用）
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth_stencil_view;

public:
	/**
	 * @brief コンストラクタ
	 *
	 * シャドウマップ用の深度テクスチャを生成する。
	 *
	 * @param device D3D11デバイス
	 * @param width シャドウマップ幅
	 * @param height シャドウマップ高さ
	 *
	 * @note
	 * 内部ではTypelessフォーマット（例: R32_TYPELESS）を使用し、
	 * DSV / SRV を用途に応じて生成する。
	 */
	shadow_map(ID3D11Device* device, uint32_t width, uint32_t height);

	virtual ~shadow_map() = default;

	/// コピー禁止
	shadow_map(const shadow_map&) = delete;
	shadow_map& operator =(const shadow_map&) = delete;

	/// ムーブ禁止（GPUリソース管理の安全性のため）
	shadow_map(shadow_map&&) noexcept = delete;
	shadow_map& operator =(shadow_map&&) noexcept = delete;

	/// 深度をシェーダーから参照するためのSRV
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;

	/// シャドウ描画用ビューポート
	D3D11_VIEWPORT viewport;

	/**
	 * @brief シャドウマップをクリア
	 *
	 * @param immediate_context デバイスコンテキスト
	 * @param depth クリア深度値（通常1.0）
	 */
	void clear(ID3D11DeviceContext* immediate_context, float depth = 1);

	/**
	 * @brief シャドウマップ描画開始
	 *
	 * 現在のレンダーターゲットを保存し、
	 * 深度専用描画に切り替える。
	 *
	 * @param immediate_context デバイスコンテキスト
	 *
	 * @pre
	 * シェーダー側はシャドウ用（深度出力のみ）に切り替えておくこと
	 */
	void activate(ID3D11DeviceContext* immediate_context);

	/**
	 * @brief シャドウマップ描画終了
	 *
	 * activate()前のレンダーターゲット状態を復元する。
	 *
	 * @param immediate_context デバイスコンテキスト
	 */
	void deactivate(ID3D11DeviceContext* immediate_context);

	/**
	 * @brief 深度マップ取得
	 *
	 * @return ID3D11ShaderResourceView*
	 *
	 * @note
	 * ピクセルシェーダーでシャドウ判定に使用
	 */
	ID3D11ShaderResourceView* get_depth_map() { return shader_resource_view.Get(); }

	/**
	 * @brief 深度マップSRVアドレス取得
	 *
	 * @return ID3D11ShaderResourceView**
	 */
	ID3D11ShaderResourceView** get_depth_map_address()
	{
		return shader_resource_view.GetAddressOf();
	}

private:
	/// キャッシュ用ビューポート数
	UINT viewport_count{ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE };

	/// キャッシュされたビューポート
	D3D11_VIEWPORT cached_viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];

	/// キャッシュされたRTV
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> cached_render_target_view;

	/// キャッシュされたDSV
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> cached_depth_stencil_view;
};