#include"Scene_Manager.h"
#include "scene.h"
#include "tracy_util.h"

//更新処理
void Scene_Manager::update(float elapsedTime)
{
	TRACY_CPU_ZONE_C("Scene_Manager::update", TracyCategory::Update);

	if (_next_scene != nullptr)
	{
		TRACY_CPU_ZONE_N("Scene_Manager::update::SceneSwitch");

		if (_current_scene)
		{
			_current_scene->finalize();
			delete _current_scene;
		}
		_current_scene = _next_scene;
		_next_scene = nullptr;
		if (!_current_scene->is_ready())
		{
			_current_scene->initialize();
		}
	}
	if (_current_scene != nullptr)
	{
		_current_scene->update(elapsedTime);
	}
}

void Scene_Manager::render(float elapsedTime)
{
	TRACY_CPU_ZONE_C("Scene_Manager::render", TracyCategory::RenderPass);

	if (_current_scene != nullptr)
	{
		_current_scene->render(elapsedTime);
	}
}

void Scene_Manager::draw_gui()
{
	TRACY_CPU_ZONE_C("Scene_Manager::draw_gui", TracyCategory::UI);

	if (_current_scene != nullptr)
	{
		_current_scene->draw_gui();
	}
	// シーン切り替え用GUI
	// 例: シーン名リスト
	static const char* scene_names[] = { "Title","ObjModel" };
	static int selected_scene = 0;
	if (ImGui::Combo("Scene", &selected_scene, scene_names, IM_ARRAYSIZE(scene_names)))
	{
		// シーン切り替え
		switch (selected_scene)
		{
		case 0:
			change_scene(new Scene_Loading(new Scene_Title()));
			break;
		case 1:
			change_scene(new Scene_Loading(new Scene_Indoor()));
			break;
		}
	}
}

void Scene_Manager::clear()
{
	TRACY_CPU_ZONE_C("Scene_Manager::clear", TracyCategory::System);

	if (_current_scene != nullptr)
	{
		_current_scene->finalize();
		delete _current_scene;
		_current_scene = nullptr;
	}
}

void Scene_Manager::change_scene(Scene_Base* scene)
{
	TRACY_CPU_ZONE_N("Scene_Manager::change_scene");

	//Clear();
	//新しいシーンを設定
	_next_scene = scene;
}