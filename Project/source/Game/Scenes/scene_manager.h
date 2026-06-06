#pragma once

#include "scene_base.h"

#include "Engine/Graphics/UI/ImGui/imgui.h"

//シーンマネージャー
class Scene_Manager
{
private:
	Scene_Manager() {}
	~Scene_Manager() {}

public:
	//唯一のインスタンス取得
	static Scene_Manager& instance()
	{
		static Scene_Manager instance;
		return instance;
	}

	//更新処理
	void update(float elapsedTime);

	//描画処理
	void render(float elapsedTime);

	//GUI描画
	void draw_gui();

	//シーンクリア
	void clear();

	//シーン切り替え
	void change_scene(Scene_Base* scene);

	bool is_exit() const { return _is_exit; }

private:
	Scene_Base* _current_scene = nullptr;
	Scene_Base* _next_scene = nullptr;
	bool _is_exit = false;
};