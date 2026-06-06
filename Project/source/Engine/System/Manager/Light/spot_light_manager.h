#pragma once
#include <vector>
#include "Game/World/Light/spot_light.h"
#include "Engine/Graphics/model/debug_primitive.h"

#ifdef DISABLE_DWRITE
#define RENDER_DOG TRUE
#else
#ifdef _DEBUG
#define RENDER_DOG TRUE
#else
#define RENDER_DOG FALSE
#endif
#endif

/**
 * @brief スポットライト管理クラス
 *
 * シーン内のスポットライトを管理し、
 * GPUへの転送およびデバッグ描画を行う。
 *
 * @remark
 * - StructuredBufferを用いてシェーダーへライト情報を渡す
 * - デバッグ用に円錐で可視化可能
 *
 * @warning
 * - 定数バッファサイズ制限（D3D11: 最大64KB）に注意
 * - ライト数が増えるとパフォーマンスに影響する
 */
class SpotLightManager
{
public:
	/**
	 * @brief コンストラクタ
	 */
	SpotLightManager()
	{}

	/**
	 * @brief 初期化
	 *
	 * @param device D3D11デバイス
	 *
	 * @note
	 * StructuredBufferやデバッグ用メッシュを生成する
	 */
	void initialize(ID3D11Device* device);

	/**
	 * @brief ライト追加
	 *
	 * @param light 追加するライト
	 */
	void add_light(const SpotLight& light);

	/**
	 * @brief ライト追加（簡易版）
	 *
	 * @param position 位置
	 * @param direction 方向
	 * @param radius 半径（影響範囲）
	 * @param intensity 強度
	 * @param innerAngle 内角
	 * @param outerAngle 外角
	 */
	void add_light(dx::XMFLOAT3 position = { 0, 0, 0 },
		dx::XMFLOAT3 direction = { 0, -1, 0 },
		float radius = 10.0f,
		float intensity = 1.0f,
		float innerAngle = 0.5f,
		float outerAngle = 0.7f,
		dx::XMFLOAT4 diffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f });

	/**
	 * @brief ライト全削除
	 */
	void clear_light()
	{
		_lights.clear();
	}

	/**
	 * @brief ライト削除（インデックス指定）
	 *
	 * @param index 削除するライトのインデックス
	 */
	void remove_light(int index);

	/**
	 * @brief ライト配列取得
	 *
	 * @return std::vector<SpotLight>&
	 *
	 * @warning
	 * 外部から直接変更されるため注意
	 */
	std::vector<SpotLight>& get_lights();

	/**
	 * @brief 更新処理
	 *
	 * ライトのアニメーションや内部状態更新を行う
	 */
	void update();

	/**
	 * @brief ImGui描画
	 *
	 * ライトの編集UIを表示する
	 */
	void draw_imgui();

	/**
	 * @brief GPUへライト情報転送
	 *
	 * @param ctx デバイスコンテキスト
	 *
	 * @pre
	 * シェーダー側で同一レイアウトのStructuredBufferが定義されていること
	 */
	void upload_to_gpu(ID3D11DeviceContext* ctx);

	/**
	 * @brief デバッグ描画
	 *
	 * ライト位置と方向を円錐で可視化する
	 *
	 * @note
	 * デバッグ用途のみ使用
	 */
	void debug_render();

private:
	/// ライト配列
	std::vector<SpotLight> _lights;

	/// 描画されている（可視）ライト数
	int _visible_count = 0;

	/// ライト用StructuredBuffer
	Microsoft::WRL::ComPtr<ID3D11Buffer> _sbLights;
	/// ライト用ShaderResourceView
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _sbLightsSRV;
	/// ライト数用定数バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> _cbLightCount;

	/**
	 * @brief StructuredBuffer生成
	 */
	void create_structured_buffer(ID3D11Device* device);

	/**
	 * @brief ライト数用定数バッファ生成
	 */
	void create_light_count_buffer(ID3D11Device* device);

	/// デバッグ描画用円錐
	Debug_Cone* _debug_cone;
};
