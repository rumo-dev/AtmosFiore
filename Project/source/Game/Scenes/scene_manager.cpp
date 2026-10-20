#include"Scene_Manager.h"
#include "scene.h"
#include "tracy_util.h"
#include "Engine/System/graphics_core.h"
#include "Engine/System/Manager/resource_manager.h"
#include "Game/World/camera/camera_manager.h"
#include "Game/World/camera/spectator_camera.h"
#include "Engine/Utilities/math_util.h"
#include "Engine/Utilities/color_util.h"
#include "Engine/Graphics/IBL/ibl.h"
#include <memory>
#include <algorithm>

namespace
{
	constexpr float TransitionPhaseDuration = 0.8f;

	enum class RenderTestKind { Reflections, Materials, Occlusion };

	class Scene_RenderTest final : public Scene_Base
	{
	public:
		explicit Scene_RenderTest(RenderTestKind kind) : kind_(kind) {}
		void initialize() override
		{
			auto& graphics = Graphics_Core::instance();
			auto& resources = Resource_Manager::instance();
			// The gameplay scene leaves its model instances in the shared manager.
			resources.remove_model_instance("Library");
			resources.remove_model_instance("Player");
			resources.remove_model_instance("RenderTest_Main");
			resources.remove_model_instance("RenderTest_Secondary");
			graphics.get_point_light_manager().clear_light();
			graphics.get_spot_light_manager().clear_light();
			graphics.get_area_light_manager().clear_light();

			const char* modelKey = nullptr;
			const char* modelPath = nullptr;
			const wchar_t* label = L"AO / Lighting Test";
			if (kind_ == RenderTestKind::Reflections) {
				modelKey = "SSR_MetalRoughSpheres"; modelPath = "./data/model/MetalRoughSpheresNoTextures.glb";
				label = L"SSR Test - Khronos Metal-Rough Spheres (CC0)";
				resources.load_model(modelKey, modelPath);
				// The source sphere radius is 0.00035 and grid pitch is 0.001 source
				// units. Scale by 1000 for a 0.7 m diameter and 1 m spacing; center
				// the 7x7 grid around X=0 with its lowest row resting at Y=0.
				add_instance("RenderTest_Main", modelKey, 1000.0f, { -3.0f, 0.85f, 3.0f }, 0.0f);
				// This sample spreads spheres through node transforms, while the
				// current model-level bounds only cover mesh-local geometry.
				if (auto* instance = resources.get_model_instance("RenderTest_Main"))
					instance->disable_frustum_culling = true;
			ssr_was_enabled_ = Post_Process_Manager::IsSSREnabled();
			Post_Process_Manager::SetSSREnabled(true);
			}
			else if (kind_ == RenderTestKind::Materials) {
				modelKey = "RenderTest_Helmet"; modelPath = "./data/model/glTF/DamagedHelmet/DamagedHelmet.gltf";
				label = L"PBR Material Test";
				resources.load_model(modelKey, modelPath);
				resources.load_model("RenderTest_Engine", "./data/model/glTF/2CylinderEngine.gltf");
				// DamagedHelmet is approximately 2 m tall in the source asset; display
				// it at 1 m. The Khronos engine sample uses centimeter-like coordinates.
				add_instance("RenderTest_Main", modelKey, 0.5f, { -1.4f, 0.6f, 0.0f }, 0.0f);
				add_instance("RenderTest_Secondary", "RenderTest_Engine", 0.75f,
					{ 1.4f, 0.55f, 0.0f }, dx::XM_PIDIV2, CoordinateSystem::RH_Y_UP, 0.01f);
			}
			else {
				modelKey = "Library"; modelPath = "./data/model/Library/library_OneMesh.gltf";
				label = L"AO / Lighting Test";
				resources.load_model(modelKey, modelPath);
				// Match the existing Indoor placement and model scale convention.
				add_instance("RenderTest_Main", modelKey, 1.0f, { 0.0f, 3.5f, 0.0f }, 0.0f);
				for (int i = 0; i < 4; ++i) {
					const float x = (i & 1) ? 3.0f : -3.0f;
					const float z = (i & 2) ? 3.0f : -3.0f;
					graphics.get_point_light_manager().add_light({ x, 2.5f, z }, 8.0f, 12000.0f,
						Color_Utils::random_hsv(0.8f, 0.9f, 1.0f));
				}
				graphics.get_area_light_manager().add_light();
			}

			light_.update();
			IBL::Initialize(graphics.get_device(), L"./data/ibl");
			const HWND hwnd = graphics.get_window_handle();
			auto camera = std::make_shared<SpectatorCamera>(hwnd);
			const bool reflectionTest = kind_ == RenderTestKind::Reflections;
			const float cameraDistance = reflectionTest ? 12.0f : (kind_ == RenderTestKind::Materials ? 4.5f : 7.0f);
			const float cameraHeight = reflectionTest ? 3.85f : (kind_ == RenderTestKind::Materials ? 1.3f : 2.5f);
			const float targetHeight = reflectionTest ? 3.85f : (kind_ == RenderTestKind::Materials ? 0.55f : 1.5f);
			const float pitch = reflectionTest ? 0.0f : (kind_ == RenderTestKind::Materials ? -9.5f : -8.0f);
			camera->set_rotation(90.0f, pitch,
				dx::XMVectorSet(0.0f, cameraHeight, -cameraDistance, 1.0f),
				dx::XMVectorSet(0.0f, targetHeight, 0.0f, 1.0f));
			CameraManager::instance().register_camera("Render Test Camera", camera);
			CameraManager::instance().switch_camera("Render Test Camera");
			label_ = label;
		}
		void finalize() override
		{
			auto& resources = Resource_Manager::instance();
			resources.remove_model_instance("RenderTest_Main");
			resources.remove_model_instance("RenderTest_Secondary");
			Graphics_Core::instance().get_point_light_manager().clear_light();
			Graphics_Core::instance().get_spot_light_manager().clear_light();
			Graphics_Core::instance().get_area_light_manager().clear_light();
			if (kind_ == RenderTestKind::Reflections)
				Post_Process_Manager::SetSSREnabled(ssr_was_enabled_);
		}
		void update(float elapsedTime) override
		{
			auto& g = Graphics_Core::instance();
			auto& camera = CameraManager::instance();
			camera.update(elapsedTime, g.get_aspect_ratio(), g.get_screen_height());
			light_.tick(elapsedTime);
			IBL::Bind(g.get_device_context());
			g.get_point_light_manager().update();
			g.get_point_light_manager().upload_to_gpu(g.get_device_context());
			g.get_spot_light_manager().update();
			g.get_spot_light_manager().upload_to_gpu(g.get_device_context());
			g.get_area_light_manager().update();
			g.get_area_light_manager().upload_to_gpu(g.get_device_context());
			Resource_Manager::instance().update_models(elapsedTime);
		}
		void render(float elapsedTime) override
		{
			auto& g = Graphics_Core::instance();
			auto* context = g.get_device_context();
			Render_State::instance().set_3d_render_states(context, Rasterizer_State::Cull_Back_CCW);
			auto& shadow = g.post_procss.GetShadow();
			shadow.make_directional_shadow_begin();
			Resource_Manager::instance().render_models(pass_mode::directional_shadow);
			shadow.make_shadow_end();
			g.update_constants();
			g.get_geometry_buffer()->Clear(context);
			g.get_geometry_buffer()->Activate(context);
			Render_State::instance().set_deferred_render_states(context, Rasterizer_State::Cull_None_CCW);
			Resource_Manager::instance().render_models(pass_mode::deferred_geometry);
			g.get_geometry_buffer()->Deactivate(context);
			context->OMSetRenderTargets(1, g.get_render_target_view().GetAddressOf(), nullptr);
			Render_State::instance().set_2d_render_states(context);
			g.clear(Color_Utils::from_hex("#202632"));
			g.post_procss.begin();
			Render_State::instance().set_3d_render_states(context, Rasterizer_State::Cull_Back_CW, Depth_State::Test_Enable_Write_Disable);
			Resource_Manager::instance().render_models(pass_mode::forward_transparency);
			g.post_procss.end();
			g.post_procss.draw();
			g.post_procss.render();
			Text::text_data.Color = Color_Utils::hex_to_colorF("#FFFFFF");
			Text::draw(label_, D2D1_RECT_F(g.get_screen_width(), g.get_screen_height()), D2D1_DRAW_TEXT_OPTIONS_NONE, true);
		}
		void draw_gui() override { light_.draw_imgui("Test Directional Light"); }
	private:
		void add_instance(const std::string& instanceKey, const std::string& modelKey, float scale,
			const dx::XMFLOAT3& position, float rotationY,
			CoordinateSystem coordinateSystem = CoordinateSystem::RH_Y_UP, float sourceUnitToMeters = 1.0f)
		{
			add_instance_scaled(instanceKey, modelKey, { scale, scale, scale }, position, rotationY,
				coordinateSystem, sourceUnitToMeters);
		}
		void add_instance_scaled(const std::string& instanceKey, const std::string& modelKey,
			const dx::XMFLOAT3& scale, const dx::XMFLOAT3& position, float rotationY,
			CoordinateSystem coordinateSystem = CoordinateSystem::RH_Y_UP, float sourceUnitToMeters = 1.0f)
		{
			ModelInstance instance{};
			instance.model_key = modelKey;
			// Position is always in engine meters. The helper's final scale factor
			// applies only to source mesh coordinates.
			instance.world_transform = make_world_matrix(coordinateSystem, scale, position,
				{ 0.0f, rotationY, 0.0f }, sourceUnitToMeters);
			Resource_Manager::instance().add_model_instance(instanceKey, std::move(instance));
		}
		RenderTestKind kind_;
		DirectionalLight light_;
		std::wstring label_;
		bool ssr_was_enabled_ = false;
	};
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
	if (ImGui::CollapsingHeader("Render Test Scenes", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextDisabled("Select a scene to inspect rendering conditions.");
		if (ImGui::Button("Indoor")) change_scene(new Scene_Indoor());
		ImGui::SameLine();
		if (ImGui::Button("SSR Reflections")) change_scene(new Scene_RenderTest(RenderTestKind::Reflections));
		if (ImGui::Button("PBR Materials")) change_scene(new Scene_RenderTest(RenderTestKind::Materials));
		if (ImGui::Button("AO / Lighting")) change_scene(new Scene_RenderTest(RenderTestKind::Occlusion));
		ImGui::Separator();
	}

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
