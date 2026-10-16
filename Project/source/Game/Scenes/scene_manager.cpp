#include"Scene_Manager.h"
#include "scene.h"
#include "tracy_util.h"
#include "Engine/System/graphics_core.h"
#include <algorithm>

namespace
{
	constexpr float TransitionPhaseDuration = 0.8f;
}

//更新処理
void Scene_Manager::update(float elapsedTime)
{
	TRACY_CPU_ZONE_C("Scene_Manager::update", TracyCategory::Update);

	if (_next_scene != nullptr || _transition_switched)
	{
		_transition_time += elapsedTime;
		// 暗転を終えてからシーンを切り替え、同じ長さをかけて明転する。
		if (_next_scene != nullptr && !_transition_switched && _transition_time >= TransitionPhaseDuration)
		{
			TRACY_CPU_ZONE_N("Scene_Manager::update::SceneSwitch");
			if (_current_scene) { _current_scene->finalize(); delete _current_scene; }
			_current_scene = _next_scene;
			_next_scene = nullptr;
			if (!_current_scene->is_ready()) _current_scene->initialize();
			_transition_switched = true;
		}
		if (_transition_switched && _transition_time >= TransitionPhaseDuration * 2.0f)
		{
			_transition_time = 0.0f;
			_transition_switched = false;
		}
	}
	if (_current_scene != nullptr && !_transition_switched && (!_next_scene || _transition_time < TransitionPhaseDuration))
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
	if (_transition_time > 0.0f)
	{
		if (!_transition_effect.is_initialized())
		{
			_transition_effect.initialize(Graphics_Core::instance().get_device(), "transition_ps.cso", 32);
		}
		if (_transition_effect.is_initialized())
		{
			struct alignas(16) TransitionConstants
			{
				float progress;
				int transition_type;
				float resolution[2];
				float seed;
				float padding[3];
			} constants{};
			constants.progress = _transition_switched
				? 1.0f - std::clamp((_transition_time - TransitionPhaseDuration) / TransitionPhaseDuration, 0.0f, 1.0f)
				: std::clamp(_transition_time / TransitionPhaseDuration, 0.0f, 1.0f);
			constants.transition_type = 0;
			constants.resolution[0] = Graphics_Core::instance().get_screen_width();
			constants.resolution[1] = Graphics_Core::instance().get_screen_height();
			constants.seed = _transition_seed;
			_transition_effect.draw(&constants, sizeof(constants));
		}
	}
}

void Scene_Manager::draw_gui()
{
	TRACY_CPU_ZONE_C("Scene_Manager::draw_gui", TracyCategory::UI);

	if (_current_scene != nullptr)
	{
		_current_scene->draw_gui();
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
	if (_next_scene)
	{
		// すでに遷移中なら要求を受け付けない。呼び出し元が所有するシーンを
		// ここで破棄すると、ロード処理中のシーンや参照先を壊す可能性がある。
		return;
	}
	_next_scene = scene;
	_transition_time = 0.0f;
	_transition_switched = false;
	_transition_seed += 17.0f;
}
