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
	sprite* _sprite_object[static_cast<int>(Sprite_Type::Num)] = {};
	/*std::unique_ptr<Obj_Model> obj_model;*/
	std::unique_ptr<Gltf_Model>_gltf_model;
	Directional_Light _directional_light;


};
