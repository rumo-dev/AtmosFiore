#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include "Engine/Graphics/Model/Gltf_Model.h"

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

	// アニメーション共通
	bool is_animation = false;
	float animation_time = 0.0f;
	float animation_speed = 1.0f;
	bool loop_animation = true;

	// モード
	Gltf_Model::animation_mode anim_mode = Gltf_Model::animation_mode::single;

	// single モード用
	int animation_index = 0;
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

	// culling stats
	int last_culled_count = 0;
	int last_total_count = 0;
	int last_point_face_culled[6] = {0,0,0,0,0,0};
	int last_point_face_total[6] = {0,0,0,0,0,0};

private:
	/// モデルキャッシュ（共有）
	std::unordered_map<std::string, std::unique_ptr<Gltf_Model>> model_cache_;

	/// インスタンス管理
	std::unordered_map<std::string, ModelInstance> instances_;
};