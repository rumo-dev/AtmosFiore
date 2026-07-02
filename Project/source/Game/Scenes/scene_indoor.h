#pragma once
#include "scene_base.h"
#include "Engine/Graphics/UI/sprite/sprite.h"
#include "Engine/system/graphics_core.h"

#include "Engine/Graphics/UI/text/text.h"
#include "Engine/System/Manager/resource_manager.h"
#include "Engine/System/Manager/PostProcess/post_process_manager.h"

#include "Engine/Graphics/Model/gltf_model.h"
#include "Game/World/Light/directional_light.h"
#include "Engine/utilities/dx_common.h"

#include "Engine/utilities/color_util.h"
#include "Engine/Graphics/IBL/ibl.h"
#include "Game/World/Player/player.h"
#include "Engine/utilities/collision_util.h"

// ゲームシーン
class Scene_Indoor :public Scene_Base
{
public:
	Scene_Indoor() {}
	~Scene_Indoor() override {}

	// 初期化
	void initialize()override;

	// 終了化
	void finalize()override;

	// 更新処理
	void update(float elapsedTime)override;

	// 描画処理
	void render(float elapsedTime)override;

	void render_shadowmap(float elapsedTime);

	void render_defferd(float elapsedTime);

	void render_forward(float elapsedTime);

	void render_UI(float elapsedTime);

	void render_debug(float elapsedTime);

	// GUI描画
	void draw_gui()override;

private:
	enum class Sprite_Type
	{
		Default,
		Texel,
		Num,
	};
	std::unique_ptr<Gltf_Model>_gltf_model;
	Directional_Light _directional_light;

	/// プレイヤー
	Player _player;

	/// プレイヤー用スポットライトの配列インデックス
	int _player_spotlight_index = -1;

	// -----------------------------------------------------------------
	// コリジョン関連
	// -----------------------------------------------------------------

	/// 当たり判定対象として明示的に登録したインスタンス情報
	struct Collision_Source
	{
		std::string instance_name;               ///< model_manager 上のインスタンス名
		DirectX::XMFLOAT4X4 world_transform;      ///< 抽出時点のワールド変換
	};
	/// 現在登録されているコリジョン対象インスタンス一覧
	std::vector<Collision_Source> _collision_sources;

	/// 登録済みインスタンス分をすべて統合したワールド空間三角形リスト（3頂点で1三角形）
	std::vector<DirectX::XMFLOAT3> _collision_triangles;

	/// _collision_triangles から構築した空間グリッド（近傍検索用）
	CollisionTriangleGrid _collision_grid;

	/// 毎フレームの近傍絞り込み結果（Player::update に渡すバッファ、使い回し）
	std::vector<DirectX::XMFLOAT3> _nearby_collision_triangles;

	/// プレイヤー周辺の三角形を検索する半径（m）
	static constexpr float COLLISION_QUERY_RADIUS = 5.0f;

	/**
	 * @brief 指定インスタンスを当たり判定対象として登録する
	 *
	 * model_manager からモデル実体を取得し、指定ワールド変換で
	 * コリジョン三角形を抽出して _collision_triangles に追加する。
	 * グリッドの再構築は行わないため、複数登録後にまとめて
	 * rebuild_collision_grid() を呼ぶこと。
	 *
	 * @param instance_name   model_manager に登録済みのインスタンス名
	 * @param world_transform 抽出に使うワールド変換（静的配置前提）
	 * @return 成功時true（モデルが見つからない、または既に登録済みの場合false）
	 */
	bool register_collision_instance(const std::string& instance_name,
		const DirectX::XMFLOAT4X4& world_transform);

	/// 登録済みインスタンスと三角形データをすべて解除する
	void clear_collision_instances();

	/// _collision_triangles の内容から空間グリッドを再構築する
	void rebuild_collision_grid();
};