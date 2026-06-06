#pragma once
#define NOMINMAX
#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>
#include <cfloat>
#include <unordered_map>

#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include "Engine/Graphics/Model/Tiny/tiny_gltf.h"
#include "Engine/Graphics/shader/shader.h"
#include "Engine/Graphics/texture/texture.h"


/**
 * @brief 描画パス種別
 */
enum class pass_mode
{
	deferred_geometry,      ///< GBuffer書き込み
	forward_opaque,         ///< 不透明フォワード描画
	forward_transparency,   ///< 透明描画
	directional_shadow,  ///< 平行光源シャドウマップ生成
	shadow                  ///< シャドウマップ生成
};

/**
 * @brief glTFモデルクラス
 *
 * glTF 2.0モデルのロード・管理・描画・アニメーション再生を行う。
 *
 * @remark
 * - PBR (Metallic-Roughness) ワークフロー対応
 * - スキニング・アニメーション対応
 * - Deferred / Forward 両対応
 *
 * @warning
 * - SRVとして使用中のテクスチャをRTV/UAVとして同時バインドしてはならない（D3D11制約）
 * - glTFの座標系・行列順序（右手系 / 列優先）に注意
 *
 * @todo
 * - Tangent自動生成
 * - Morph Target対応
 * - GPUスキニング対応（現在はCPU寄り）
 * - インスタンシング対応
 * - Bindless的リソース管理
 */
class Gltf_Model
{
	std::string filename;

private:
	/**
	 * @brief シーン
	 */
	struct scene
	{
		std::string name;
		std::vector<int> nodes; ///< ルートノード
	};
	std::vector<scene> scenes;

public:
	/**
	 * @brief ノード（階層構造）
	 */
	struct node
	{
		std::string name;
		int skin{ -1 };  // index of skin referenced by this node
		int mesh{ -1 };  // index of mesh referenced by this node

		std::vector<int> children; // An array of indices of child nodes of this node

		/// ローカル変換
		DirectX::XMFLOAT4 rotation{ 0, 0, 0, 1 };
		DirectX::XMFLOAT3 scale{ 1, 1, 1 };
		DirectX::XMFLOAT3 translation{ 0, 0, 0 };

		/// ワールド変換
		DirectX::XMFLOAT4X4 global_transform{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	};

	// アニメーション再生モード
	enum class animation_mode
	{
		single,   // 番号指定で1つ再生
		all,      // 全アニメーション同時再生
	};

	// 既存メンバ変数
	float time = 0.0f;
	int current_animation_index = 0;
	std::vector<node> nodes;
	/**
	 * @brief ノード情報を取得
	 * @param Gltf_Model glTFモデル
	 */
	void fetch_nodes(const tinygltf::Model& Gltf_Model);

private:
	/**
	 * @brief GPUバッファビュー
	 */
	struct buffer_view
	{
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		size_t stride_in_bytes{ 0 };
		size_t size_in_bytes{ 0 };
		size_t count() const
		{
			return size_in_bytes / stride_in_bytes;
		}
	};
	/**
	 * @brief メッシュ
	 */
	struct mesh
	{
		std::string name;
		struct primitive
		{
			int material;
			std::map<std::string, buffer_view> vertex_buffer_views;
			buffer_view index_buffer_view;
			// CPU-side copy of POSITION attribute for dynamic bbox calculation
			std::vector<DirectX::XMFLOAT3> cpu_positions;
		};
		std::vector<primitive> primitives;
	};
	std::vector<mesh> meshes;
	/**
	* @brief バッファビューを作成
	* @param accessor アクセサ
	* @return バッファビュー
	*/
	buffer_view make_buffer_view(const tinygltf::Accessor& accessor);
	/**
	 * @brief メッシュを取得
	 * @param device デバイス
	 * @param Gltf_Model glTFモデル
	 */
	void fetch_meshes(ID3D11Device* device, const tinygltf::Model& Gltf_Model);


private:
	/**
	 * @brief テクスチャ情報
	 */
	struct texture_info
	{
		int index = -1;
		int texcoord = 0;
	};
	/**
	 * @brief 法線マップ情報
	 */
	struct normal_texture_info
	{
		int index = -1;
		int texcoord = 0;
		float scale = 1;
	};
	/**
	 * @brief 隠蔽マップ情報
	 */
	struct occlusion_texture_info
	{
		int index = -1;
		int texcoord = 0;
		float strength = 1;
	};
	/**
	 * @brief PBR Metallic-Roughness
	 */
	struct pbr_metallic_roughness
	{
		float basecolor_factor[4] = { 1, 1, 1, 1 };
		texture_info basecolor_texture;
		float metallic_factor = 1;
		float roughness_factor = 1;
		texture_info metallic_roughness_texture;
	};
	/**
	 * @brief マテリアル
	 */
	struct material {
		std::string name;
		struct cbuffer
		{
			float emissive_factor[3] = { 0, 0, 0 };
			int alpha_mode = 0;	// "OPAQUE" : 0, "MASK" : 1, "BLEND" : 2
			float alpha_cutoff = 0.5f;
			bool double_sided = false;

			pbr_metallic_roughness pbr_metallic_roughness;

			normal_texture_info normal_texture;
			occlusion_texture_info occlusion_texture;
			texture_info emissive_texture;
		};
		cbuffer data;
	};
	std::vector<material> materials;
	/// マテリアル定数バッファ（SRVとしてまとめて参照）
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> material_resource_view;
	/**
	* @brief マテリアルを取得
	* @param device デバイス
	* @param Gltf_Model glTFモデル
	*/
	void fetch_materials(ID3D11Device* device, const tinygltf::Model& Gltf_Model);



private:
	/**
	 * @brief テクスチャ
	 */
	struct texture
	{
		std::string name;
		int source{ -1 };
	};
	std::vector<texture> textures;
	struct image
	{
		std::string name;
		int width{ -1 };
		int height{ -1 };
		int component{ -1 };
		int bits{ -1 };
		int pixel_type{ -1 };
		int buffer_view;
		std::string mime_type;
		std::string uri;
		bool as_is{ false };
	};
	std::vector<image> images;
	std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> texture_resource_views;
	void fetch_textures(ID3D11Device* device, const tinygltf::Model& Gltf_Model);
private:
	/**
	 * @brief スキン（ボーン）
	 */
	struct skin
	{
		std::vector<DirectX::XMFLOAT4X4> inverse_bind_matrices;
		std::vector<int> joints;
	};
	std::vector<skin> skins;
	/**
	 * @brief アニメーション
	 */
	struct animation
	{
		std::string name;
		float duration{ 0.0f };
		struct channel
		{
			int sampler{ -1 };
			int target_node{ -1 };
			std::string target_path;
		};
		std::vector<channel> channels;

		struct sampler
		{
			int input{ -1 };
			int output{ -1 };
			std::string interpolation;
		};
		std::vector<sampler> samplers;

		std::unordered_map<int/*sampler.input*/, std::vector<float>> timelines;
		std::unordered_map<int/*sampler.output*/, std::vector<DirectX::XMFLOAT3>> scales;
		std::unordered_map<int/*sampler.output*/, std::vector<DirectX::XMFLOAT4>> rotations;
		std::unordered_map<int/*sampler.output*/, std::vector<DirectX::XMFLOAT3>> translations;
	};
public:
	std::vector<animation> animations;
	// Axis-aligned bounding box in model-local space
	DirectX::XMFLOAT3 bbox_min{ FLT_MAX, FLT_MAX, FLT_MAX };
	DirectX::XMFLOAT3 bbox_max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
	bool has_bbox{ false };
	/// 最大ボーン数（シェーダー制限）
	static const size_t PRIMITIVE_MAX_JOINTS = 512;
	/**
	 * @brief スキニング用定数
	 */
	struct primitive_joint_constants
	{
		DirectX::XMFLOAT4X4 matrices[PRIMITIVE_MAX_JOINTS];
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer> primitive_joint_cbuffer;


	void fetch_animations(const tinygltf::Model& Gltf_Model);

public:
	// Compute instance world-space AABB, considering animation/skinning if applicable
	void compute_instance_aabb(
		const DirectX::XMFLOAT4X4& world,
		bool is_animation,
		float animation_time,
		animation_mode anim_mode,
		int animation_index,
		DirectX::XMFLOAT3& out_min,
		DirectX::XMFLOAT3& out_max
	) const;
private:
	//util
	void cumulate_transforms(std::vector<node>& nodes);

private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
	struct primitive_constants
	{
		DirectX::XMFLOAT4X4 world;
		int material{ -1 };
		int has_tangent{ 0 };
		int skin{ -1 };
		int pad;
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer> primitive_cbuffer;

public:
	/**
	 * @brief コンストラクタ
	 *
	 * @param device D3D11デバイス
	 * @param filename glTFファイルパス
	 */
	Gltf_Model(ID3D11Device* device, const std::string& filename);
	/// @brief デストラクタ
	virtual ~Gltf_Model() = default;

	/**
	 * @brief モデル描画
	 *
	 * @param immediate_context デバイスコンテキスト
	 * @param world ワールド行列
	 * @param pass 描画パス
	 * @param is_animation アニメーション使用
	 * @param animation_index 使用アニメーション
	 * @param _time 再生時間
	 *
	 * @pre
	 * - アニメーション使用時、事前にanimate()を呼び出すこと
	 */
	 // 関数宣言変更
	void render(
		ID3D11DeviceContext* immediate_context,
		const DirectX::XMFLOAT4X4& world,
		pass_mode pass,
		bool is_animation,
		float delta_time,
		animation_mode anim_mode = animation_mode::single,
		int animation_index = 0
	);

	// 全アニメーション同時適用
	void animate_all(float time, std::vector<node>& animated_nodes);

	/**
	 * @brief アニメーション更新
	 *
	 * @param animation_index 使用アニメーション
	 * @param time 再生時間
	 * @param animated_nodes アニメーション適用先ノード配列
	 */
	void animate(size_t animation_index, float time, std::vector<node>& animated_nodes);

	/**
	 * @brief マテリアルがパスに含まれるか判定
	 *
	 * @param alpha_mode アルファモード
	 * @param pass 描画パス
	 * @return マテリアルがパスに含まれるか
	 */
	bool is_material_in_pass(int alpha_mode, pass_mode pass)
	{
		switch (pass)
		{
		case pass_mode::deferred_geometry:
		case pass_mode::forward_opaque:
			return alpha_mode == 0/*OPAQUE*/ || alpha_mode == 1/*MASK*/;
		case pass_mode::forward_transparency:
			return alpha_mode == 2/*BLEND*/;
		case pass_mode::directional_shadow:
		case pass_mode::shadow:
			return  alpha_mode == 0; // TODO
		default:
			return false;
		}
	}

};
