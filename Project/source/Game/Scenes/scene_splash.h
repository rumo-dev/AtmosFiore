#pragma once
#include "scene_base.h"
#include "Engine/Graphics/UI/text/text.h"
#include "Engine/utilities/easing_util.h"
#include "scene_manager.h"
#include "scene_title.h"
#include "scene_indoor.h"

/**
 * @brief スプラッシュ画面シーンクラス
 *
 * @details
 * アプリケーション起動時に表示される美麗なブランドロゴ画面。
 * 滑らかなフェードイン、フェードアウトのアニメーションを行い、
 * 完了後に自動でタイトル画面（Scene_Title）へと遷移する。
 * SPACE または ENTER キーによるスキップに対応。
 */
class Scene_Splash : public Scene_Base
{
public:
	/**
	 * @brief コンストラクタ
	 */
	Scene_Splash() {}

	/**
	 * @brief デストラクタ
	 */
	~Scene_Splash() override {}

	/**
	 * @brief 初期化処理
	 */
	void initialize() override;

	/**
	 * @brief 終了処理
	 */
	void finalize() override;

	/**
	 * @brief 更新処理
	 * @param elapsedTime 経過時間（秒）
	 */
	void update(float elapsedTime) override;

	/**
	 * @brief 描画処理
	 * @param elapsedTime 経過時間（秒）
	 */
	void render(float elapsedTime) override;

	/**
	 * @brief GUI描画（デバッグ用）
	 */
	void draw_gui() override;

private:
	float _timer = 0.0f;       ///< シーン全体の経過時間
	bool _is_skipped = false;  ///< スキップが要求されたかどうかのフラグ
};
