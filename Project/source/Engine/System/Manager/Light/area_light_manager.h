#pragma once
#include <vector>
#include "Game/World/Light/area_light.h"
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
 * @brief エリアライト管理クラス
 *
 * シーン内のエリアライトを管理し、
 * GPUへの転送およびデバッグ描画を行う。
 *
 * @remark
 * - StructuredBufferを用いてシェーダーへライト情報を渡す
 * - デバッグ用に矩形で可視化可能
 *
 * @warning
 * - 定数バッファサイズ制限（D3D11: 最大64KB）に注意
 * - ライト数が増えるとパフォーマンスに影響する
 */
class AreaLightManager
{
public:
	/**
	 * @brief コンストラクタ
	 */
	AreaLightManager()
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
	void add_light(const AreaLight& light);

	/**
	 * @brief ライト追加（簡易版）
	 *
	 * @param position 位置
	 * @param direction 方向（法線）
	 * @param right 右方向ベクトル
	 * @param width 幅
	 * @param height 高さ
	 * @param intensity 強度
	 */
	void add_light(dx::XMFLOAT3 position = { 0, 0, 0 },
		dx::XMFLOAT3 direction = { 0, -1, 0 },
		dx::XMFLOAT3 right = { 1, 0, 0 },
		float width = 2.0f,
		float height = 2.0f,
		float intensity = 1.0f,
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
	 * @return std::vector<AreaLight>&
	 *
	 * @warning
	 * 外部から直接変更されるため注意
	 */
	std::vector<AreaLight>& get_lights();

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
	 * ライト位置と方向を矩形で可視化する
	 *
	 * @note
	 * デバッグ用途のみ使用
	 */
	void debug_render();

private:
	/// ライト配列
	std::vector<AreaLight> _lights;

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

	/// デバッグ描画用矩形
	Debug_Cube* _debug_box;
	/// デバッグ描画用円盤
	Debug_Disk* _debug_disk;
	/// デバッグ描画用球
	Debug_Sphere* _debug_sphere;
};
