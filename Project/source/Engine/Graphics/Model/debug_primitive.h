#pragma once
#include <d3d11.h>
#include "Engine/Utilities/dx_common.h"
#include "Engine/Utilities/color_util.h"

/**
 * @brief デバッグ用プリミティブ描画クラス（基底クラス）
 *
 * シンプルなメッシュ（立方体・球・カプセルなど）を描画するためのクラス。
 * 主に当たり判定の可視化やデバッグ用途で使用される。
 *
 * @remark
 * - 軽量な描画を目的としており、ライティングや複雑なマテリアルは考慮しない
 * - ワールド変換 + 単色カラーのみ対応
 *
 * @warning
 * 高頻度描画（大量インスタンス）には不向き。
 * 本クラスはデバッグ用途に限定して使用すること。
 *
 * @todo
 * - インスタンシング対応
 * - ワイヤーフレームモード対応
 * - ライン描画（AABB, OBBなど）対応
 */
class DebugPrimitive
{

public:
	/**
	 * @brief 頂点構造体
	 */
	struct Vertex
	{
		DirectX::XMFLOAT3 position; ///< 頂点位置
		DirectX::XMFLOAT3 normal;   ///< 法線
	};

	/**
	 * @brief 定数バッファ構造体
	 */
	struct Constants
	{
		DirectX::XMFLOAT4X4 world;        ///< ワールド行列
		DirectX::XMFLOAT4 material_color; ///< 描画色
	};


	virtual ~DebugPrimitive() = default;

	/**
	 * @brief プリミティブを描画
	 *
	 * @param immediate_context デバイスコンテキスト
	 * @param world ワールド変換行列
	 * @param material_color 描画色
	 *
	 * @pre
	 * 適切なレンダーターゲットとビューポートが設定されていること
	 */
	void Render(ID3D11DeviceContext* immediate_context,
		const DirectX::XMFLOAT4X4& world,
		const DirectX::XMFLOAT4& material_color);

protected:
	/**
	 * @brief コンストラクタ
	 *
	 * @param device D3D11デバイス
	 */
	DebugPrimitive(ID3D11Device* device);

	/**
	 * @brief GPUバッファを生成
	 *
	 * 頂点・インデックスバッファを作成する。
	 *
	 * @param device D3D11デバイス
	 * @param vertices 頂点配列
	 * @param vertex_count 頂点数
	 * @param indices インデックス配列
	 * @param index_count インデックス数
	 */
	void CreateComBuffers(ID3D11Device* device,
		Vertex* vertices,
		size_t vertex_count,
		uint32_t* indices,
		size_t index_count);
private:
	/// 頂点バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;

	/// インデックスバッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;

	/// 頂点シェーダー
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;

	/// ピクセルシェーダー
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;

	/// 入力レイアウト
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

	/// 定数バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbDebugPrimitive;

public:

};

/**
 * @brief デバッグ用立方体
 */
class Debug_Cube : public DebugPrimitive
{
public:
	/**
	 * @brief コンストラクタ
	 *
	 * @param device D3D11デバイス
	 */
	Debug_Cube(ID3D11Device* device);
};

/**
 * @brief デバッグ用円柱
 */
class Debug_Cylinder : public DebugPrimitive
{
public:
	/**
	 * @brief コンストラクタ
	 *
	 * @param device D3D11デバイス
	 * @param slices 円周分割数
	 *
	 * @note slicesが大きいほど滑らかになるがコスト増加
	 */
	Debug_Cylinder(ID3D11Device* device, uint32_t slices);
};

/**
 * @brief デバッグ用球
 */
class Debug_Sphere : public DebugPrimitive
{
public:
	/**
	 * @brief コンストラクタ
	 *
	 * @param device D3D11デバイス
	 * @param slices 経度方向分割数
	 * @param stacks 緯度方向分割数
	 *
	 * @remark 分割数が多いほど品質向上するが頂点数増加
	 */
	Debug_Sphere(ID3D11Device* device, uint32_t slices, uint32_t stacks);
};

/**
 * @brief デバッグ用カプセル
 */
class Debug_Capsule : public DebugPrimitive
{
public:
	/**
	 * @brief コンストラクタ
	 *
	 * @param device D3D11デバイス
	 * @param mantle_height 円柱部分の高さ
	 * @param radius 半径（XYZ個別指定可）
	 * @param slices 円周分割数
	 * @param ellipsoid_stacks 半球部分の分割数
	 * @param mantle_stacks 円柱部分の分割数
	 *
	 * @note
	 * radiusを非等方にすると楕円カプセルになる
	 */
	Debug_Capsule(ID3D11Device* device,
		float mantle_height,
		const DirectX::XMFLOAT3& radius,
		uint32_t slices,
		uint32_t ellipsoid_stacks,
		uint32_t mantle_stacks);
};

/**
 * @brief デバッグ用円錐
 */
class Debug_Cone : public DebugPrimitive
{
public:
	/**
	 * @brief コンストラクタ
	 *
	 * @param device D3D11デバイス
	 * @param slices 円周分割数
	 *
	 * @note slicesが大きいほど滑らかになるがコスト増加
	 */
	Debug_Cone(ID3D11Device* device, uint32_t slices);
};

/**
 * @brief デバッグ用円盤（ディスク）
 */
class Debug_Disk : public DebugPrimitive
{
public:
	/**
	 * @brief コンストラクタ
	 *
	 * @param device D3D11デバイス
	 * @param slices 円周分割数
	 *
	 * @note slicesが大きいほど滑らかになるがコスト増加
	 */
	Debug_Disk(ID3D11Device* device, uint32_t slices);
};