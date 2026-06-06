#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>
#include <string>
#include <vector>

#include "Engine/Graphics/Shader/shader.h"
#include "Engine/Utilities/misc.h"

#include <fstream>
#include <filesystem>
#include "Engine/Graphics/texture/texture.h"

/**
 * @brief OBJモデルクラス
 *
 * Wavefront OBJ形式のモデルをロード・描画するクラス。
 * MTLファイルによるマテリアルおよびテクスチャに対応する。
 *
 * @remark
 * - シンプルなForward描画を想定
 * - PBRではなく従来のBlinn-Phong風（Ka, Kd, Ks）
 *
 * @warning
 * - OBJは法線・接線が不完全な場合があるため、描画結果に影響する
 * - SRVとして使用中のテクスチャをRTVとして同時バインドしてはならない（D3D11制約）
 *
 * @todo
 * - Tangent生成（NormalMap対応）
 * - 複数UVセット対応
 * - マテリアル拡張（PBR化）
 * - インスタンシング対応
 */
class Obj_Model
{
public:
	/**
	 * @brief 頂点構造体
	 */
	struct Vertex
	{
		DirectX::XMFLOAT3 position; ///< 頂点位置
		DirectX::XMFLOAT3 normal;   ///< 法線
		DirectX::XMFLOAT2 texcoord; ///< UV座標
	};

	/**
	 * @brief 定数バッファ
	 */
	struct Constants
	{
		DirectX::XMFLOAT4X4 world; ///< ワールド行列
		DirectX::XMFLOAT4 Ka;      ///< 環境反射
		DirectX::XMFLOAT4 Kd;      ///< 拡散反射
		DirectX::XMFLOAT4 Ks;      ///< 鏡面反射
	};

	/**
	 * @brief サブセット（マテリアル単位）
	 */
	struct Subset
	{
		std::wstring usemtl;   ///< 使用マテリアル名
		uint32_t index_start{ 0 };  ///< インデックス開始位置
		uint32_t index_count{ 0 };  ///< インデックス数
	};

	std::vector<Subset> subsets;

	/**
	 * @brief マテリアル（MTL）
	 */
	struct Material
	{
		std::wstring name;

		/// 環境・拡散・鏡面
		DirectX::XMFLOAT4 Ka{ 0.2f, 0.2f, 0.2f, 1.0f };
		DirectX::XMFLOAT4 Kd{ 0.8f, 0.8f, 0.8f, 1.0f };
		DirectX::XMFLOAT4 Ks{ 1.0f, 1.0f, 1.0f, 1.0f };

		/// テクスチャ（0: diffuse, 1: specular等）
		std::wstring texture_filenames[2];

		/// GPUリソース
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_views[2];
	};

	std::vector<Material> materials;

	/**
	 * @brief バウンディングボックス（AABB）
	 *
	 * [0] = 最小, [1] = 最大
	 */
	DirectX::XMFLOAT3 bounding_box[2]{
		{ D3D11_FLOAT32_MAX, D3D11_FLOAT32_MAX, D3D11_FLOAT32_MAX },
		{ -D3D11_FLOAT32_MAX, -D3D11_FLOAT32_MAX, -D3D11_FLOAT32_MAX }
	};

private:
	/// 頂点バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> _vertex_buffer;

	/// インデックスバッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> _index_buffer;

	/// シェーダー
	Microsoft::WRL::ComPtr<ID3D11VertexShader> _vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> _pixel_shader;

	/// 入力レイアウト
	Microsoft::WRL::ComPtr<ID3D11InputLayout> _input_layout;

	/// 定数バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> _constant_buffer;

	/// バウンディングボックス描画用
	ComPtr<ID3D11Buffer> _bbox_vertex_buffer;
	ComPtr<ID3D11Buffer> _bbox_index_buffer;

public:
	/**
	 * @brief コンストラクタ
	 *
	 * OBJおよびMTLファイルをロードし、GPUリソースを生成する。
	 *
	 * @param device D3D11デバイス
	 * @param obj_filename OBJファイルパス
	 * @param flipping_v_coordinates UVのV座標反転
	 *
	 * @note
	 * 多くのOBJはV座標が上下逆のため、フラグで調整可能
	 */
	Obj_Model(ID3D11Device* device,
		const wchar_t* obj_filename,
		bool flipping_v_coordinates);

	virtual ~Obj_Model() = default;

	/**
	 * @brief モデル描画
	 *
	 * @param immediate_context デバイスコンテキスト
	 * @param world ワールド行列
	 * @param material_color 乗算カラー
	 * @param is_baunding_box バウンディングボックス描画フラグ
	 * @param alternative_pixel_shader 代替ピクセルシェーダー
	 *
	 * @pre
	 * レンダーターゲット・ビューポートが設定済みであること
	 *
	 * @remark
	 * alternative_pixel_shader を指定すると、
	 * マテリアル設定を無視して描画可能（デバッグ用途など）
	 */
	void render(ID3D11DeviceContext* immediate_context,
		const DirectX::XMFLOAT4X4& world,
		const DirectX::XMFLOAT4& material_color,
		bool is_baunding_box,
		ID3D11PixelShader* alternative_pixel_shader = nullptr);

protected:
	/**
	 * @brief GPUバッファ生成
	 */
	void create_com_buffers(ID3D11Device* device,
		Vertex* vertices,
		size_t vertex_count,
		uint32_t* indices,
		size_t index_count);

	/**
	 * @brief バウンディングボックス生成
	 *
	 * @param device D3D11デバイス
	 */
	void create_bounding_box(ID3D11Device* device);
};