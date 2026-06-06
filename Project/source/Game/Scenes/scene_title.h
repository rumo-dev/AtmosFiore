#pragma once
#include "scene_base.h"
#include "Engine/Graphics/UI/text/text.h"

#include "Engine/utilities/easing_util.h"
#include "scene_manager.h"
#include "loading.h"

// ゲームシーン
class Scene_Title :public Scene_Base
{
public:
	Scene_Title() {}
	~Scene_Title() override {}

	// 初期化
	void initialize()override;

	// 終了化
	void finalize()override;

	// 更新処理
	void update(float elapsedTime)override;

	// 描画処理
	void render(float elapsedTime)override;

	// GUI描画
	void draw_gui()override;

private:
	float _timer = 0.0f;
};
