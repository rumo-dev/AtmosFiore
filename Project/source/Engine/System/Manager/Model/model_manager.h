#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include <array>
#include <chrono>
#include "Engine/Graphics/Model/Gltf_Model.h"
#include "Engine/Utilities/math_util.h"

/**
 * @brief モデルインスタンス状態
 *
 * モデル（共有リソース）に対する個別の変換・アニメーション状態を保持する。
 *
 * @remark
 * - モデル本体（メッシュ・マテリアル）は共有
 * - 各インスタンスごとにトランスフォームやアニメーションを持つ
 */
struct ModelInstance
{
	std::string model_key;
	DirectX::XMFLOAT4X4 world_transform;
	bool visible = true;
	// Useful for glTF scenes whose node transforms spread meshes beyond the
	// mesh-local bounds currently used by the model-level frustum culler.
	bool disable_frustum_culling = false;

	// アニメーション共通
	bool is_animation = false;
	float animation_time = 0.0f;
	float animation_speed = 1.0f;
	bool loop_animation = true;

	// モード
	Gltf_Model::animation_mode anim_mode = Gltf_Model::animation_mode::single;

	// single モード用
	int animation_index = 0;

	// アニメーションノードキャッシュ（update()で計算、render()で再利用）
	std::vector<Gltf_Model::node> cached_animated_nodes;

	// スキン行列（ジョイント行列）キャッシュ（update()で1回だけ計算し、
	// 各描画パス（deferred/shadow x2/directional/forward）で使い回す）
	std::unordered_map<int, Gltf_Model::primitive_joint_constants> cached_joint_matrices;
	Gltf_Model::occlusion_state occlusion;
};

/**
 * @brief モデル管理クラス
 *
 * glTFモデルのロード・キャッシュ・インスタンス管理・描画を行う。
 *
 * @remark
 * - モデル（Gltf_Model）は共有リソースとして管理
 * - インスタンスは軽量データとして複数生成可能
 *
 * @warning
 * - インスタンスのキー重複に注意（上書きされる）
 * - モデルをアンロードすると参照しているインスタンスは無効になる
 *
 * @todo
 * - インスタンスIDを整数化（高速化）
 * - マルチスレッドロード対応
 * - GPUインスタンシング対応
 * - フラスタムカリング / オクルージョンカリング
 */
class ModelManager
{
public:
	// ─────────────────────────────
	// モデル（共有リソース）管理
	// ─────────────────────────────

	/**
	 * @brief モデルロード
	 *
	 * 同じキーで再度呼び出した場合、既存キャッシュを返す。
	 *
	 * @param key モデル識別キー
	 * @param filepath ファイルパス
	 * @return Gltf_Model* モデルポインタ
	 *
	 * @note
	 * ロード済みの場合は再ロードされない
	 */
	Gltf_Model* load(
		const std::string& key,
		const std::string& filepath);

	/**
	 * @brief モデル取得
	 *
	 * @param key モデルキー
	 * @return Gltf_Model* 存在しない場合 nullptr
	 */
	Gltf_Model* get_model(const std::string& key);

	/**
	 * @brief モデルアンロード
	 *
	 * @param key モデルキー
	 *
	 * @warning
	 * このモデルを参照しているインスタンスは無効になる
	 */
	void unload(const std::string& key);

	/**
	 * @brief 全モデルアンロード
	 */
	void unload_all();

	// Release cached models not referenced by any live instance.
	size_t unload_unused();
	size_t get_loaded_model_count() const { return model_cache_.size(); }
	size_t get_instance_count() const { return instances_.size(); }

	// ─────────────────────────────
	// インスタンス管理
	// ─────────────────────────────

	/**
	 * @brief インスタンス追加
	 *
	 * @param key インスタンスキー
	 * @param instance インスタンス情報
	 *
	 * @warning
	 * 同一キーが存在する場合は上書きされる
	 */
	void add_instance(const std::string& key, ModelInstance instance);

	/**
	 * @brief インスタンス取得
	 *
	 * @param key インスタンスキー
	 * @return ModelInstance* 編集可能ポインタ（存在しない場合 nullptr）
	 *
	 * @warning
	 * 直接編集されるため整合性に注意
	 */
	ModelInstance* get_instance(const std::string& key);

	/**
	 * @brief インスタンス削除
	 *
	 * @param key インスタンスキー
	 */
	void remove_instance(const std::string& key);

	/**
	 * @brief 全インスタンス削除
	 */
	void remove_all_instances();

	// ─────────────────────────────
	// 更新・描画
	// ─────────────────────────────

	/**
	 * @brief アニメーション更新
	 *
	 * 全インスタンスのアニメーション時間を進める。
	 *
	 * @param delta_time 経過時間（秒）
	 *
	 * @note
	 * ループ設定に応じて時間を制御する
	 */
	void update(float delta_time);

	/**
	 * @brief 全インスタンス描画
	 *
	 * @param pass 描画パス
	 *
	 * @pre
	 * シェーダー・ライト・カメラが事前に設定されていること
	 */
	void render_all(pass_mode pass);
	void issue_occlusion_queries(ID3D11DeviceContext* context);
	void begin_profile(ID3D11DeviceContext* context);
	void end_profile(ID3D11DeviceContext* context);

	/**
	 * @brief 単一インスタンス描画
	 *
	 * @param key インスタンスキー
	 * @param pass 描画パス
	 */
	void render(const std::string& key, pass_mode pass);

	/**
	 * @brief ImGuiデバッグ描画
	 */
	void draw_imgui();
	bool meshlet_culling_enabled() const { return meshlet_culling_enabled_; }
	bool occlusion_culling_enabled() const { return occlusion_culling_enabled_; }
	void set_meshlet_culling_enabled(bool enabled) { meshlet_culling_enabled_ = enabled; }
	void set_occlusion_culling_enabled(bool enabled) { occlusion_culling_enabled_ = enabled; }
	void set_camera_culling_frustum(const Frustum& frustum) { frozen_camera_frustum_ = frustum; camera_culling_frozen_ = true; }
	void restore_camera_culling_frustum(bool frozen,const Frustum& frustum) { frozen_camera_frustum_ = frustum; camera_culling_frozen_ = frozen; }
	bool camera_culling_frozen() const { return camera_culling_frozen_; }
	const Frustum& frozen_camera_frustum() const { return frozen_camera_frustum_; }
	const Gltf_Model::meshlet_culling_stats& camera_meshlet_stats() const { return camera_meshlet_stats_; }
	UINT last_occlusion_queries_issued() const { return last_occlusion_queries_issued_; }
	uint64_t total_occlusion_queries_issued() const { return total_occlusion_queries_issued_; }
	UINT stationary_camera_frames() const { return stationary_camera_frames_; }

	// culling stats
	int last_culled_count = 0;
	int last_total_count = 0;
	int last_point_face_culled[6] = { 0,0,0,0,0,0 };
	int last_point_face_total[6] = { 0,0,0,0,0,0 };

private:
	bool meshlet_culling_enabled_ = true;
	bool occlusion_culling_enabled_ = true;
	bool profile_enabled_ = false;
	bool camera_culling_frozen_ = false;
	Frustum frozen_camera_frustum_{};
	Gltf_Model::meshlet_culling_stats camera_meshlet_stats_{};
	Gltf_Model::meshlet_culling_stats directional_meshlet_stats_{};
	UINT last_occlusion_queries_issued_ = 0;
	uint64_t total_occlusion_queries_issued_ = 0;
	DirectX::XMFLOAT4X4 previous_camera_view_projection_{};
	bool have_previous_camera_view_projection_ = false;
	UINT stationary_camera_frames_ = 0;
	uint64_t frame_index_ = 0;
	uint64_t last_query_frame_ = 0;
	size_t query_instance_cursor_ = 0;
	struct performance_average
	{
		double cpu_ms = 0.0;
		double gpu_ms = 0.0;
		uint64_t cpu_samples = 0;
		uint64_t gpu_samples = 0;
		double queries_per_frame = 0.0;
		uint64_t skipped_triangles = 0;
	};
	struct gpu_timer_slot
	{
		Microsoft::WRL::ComPtr<ID3D11Query> disjoint;
		Microsoft::WRL::ComPtr<ID3D11Query> begin;
		Microsoft::WRL::ComPtr<ID3D11Query> end;
		bool pending = false;
		bool occlusion_enabled = false;
		uint64_t generation = 0;
	};
	std::array<performance_average, 2> performance_{};
	std::array<gpu_timer_slot, 4> gpu_timers_{};
	std::chrono::steady_clock::time_point cpu_profile_begin_{};
	uint64_t profile_generation_ = 0;
	int active_gpu_timer_ = -1;
	bool profile_active_ = false;
	bool profile_occlusion_enabled_ = false;
	Microsoft::WRL::ComPtr<ID3D11Buffer> occlusion_box_cbuffer_;

	/// モデルキャッシュ（共有）
	std::unordered_map<std::string, std::unique_ptr<Gltf_Model>> model_cache_;

	/// インスタンス管理
	std::unordered_map<std::string, ModelInstance> instances_;
};
