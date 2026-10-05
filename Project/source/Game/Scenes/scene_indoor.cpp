#include "Scene_indoor.h"
#include "Engine/System/Graphics_Core.h"
#include "Engine/system/manager/resource_manager.h"
#include "Engine/Utilities/math_util.h"

// すべてのカメラクラスのヘッダーをインクルード
#include "Game/World/camera/spectator_camera.h"
#include "Game/World/camera/third_person_camera.h"
#include "Game/World/camera/first_person_camera.h"
#include "Game/World/camera/orbit_camera.h"
#include "Game/World/camera/quarter_view_camera.h"
#include "Game/World/camera/cinematic_camera.h"

#include "Engine/Audio/AudioSystem.h"

#include "Game/World/camera/camera_manager.h"
#include <string>
#include "Engine/Utilities/color_Util.h"

// Tracy Profiler Utility Header (Tracy.hpp や D3D11マクロ、カテゴリ定義が含まれます)
#include "tracy_util.h"

// 初期化
void Scene_Indoor::initialize()
{
	log_printf("ゲームシーン開始\n", LogLevel::Info);
	log_printf("ゲームシーン初期化開始\n", LogLevel::Info);
	ID3D11Device* device = Graphics_Core::instance().get_device();

	// スポットライトの追加
	for (int i = 0; i < 10; i++) {
		float x = Random::Range(-10.0f, 10.0f);
		float z = Random::Range(-10.0f, 10.0f);
		float y = Random::Range(3.0f, 7.0f);
		float r = Random::Range(5.0f, 15.0f);
		float intensity = Random::Range(5.0f, 15.0f);
		float innerAngle = Random::Range(0.1f, 0.5f);
		float outerAngle = Random::Range(0.4f, 0.8f);
		dx::XMFLOAT3 direction = { Random::Range(-1.0f, 1.0f), Random::Range(-1.0f, -0.5f), Random::Range(-1.0f, 1.0f) };
		dx::XMFLOAT4 diffuseColor = Color_Utils::random_hsv(1.0f, 1.0f, 1.0f);
		Graphics_Core::instance().get_spot_light_manager().add_light({ x, y, z }, direction, r, intensity, innerAngle, outerAngle, diffuseColor);
	}

	// ポイントライトの追加
	for (int i = 0; i < 1; i++) {
		float x = Random::Range(-10.0f, 10.0f);
		float z = Random::Range(-10.0f, 10.0f);
		float y = Random::Range(3.0f, 7.0f);
		float r = 5.0f;
		float intensity = 10.0f;
		dx::XMFLOAT4 diffuseColor = Color_Utils::random_hsv(1.0f, 1.0f, 1.0f);
		Graphics_Core::instance().get_point_light_manager().add_light({ x, y, z }, r, intensity, diffuseColor);
	}
	Graphics_Core::instance().get_area_light_manager().add_light();

	// ライブラリモデルの登録
	ModelInstance library;
	library.model_key = "Library";
	library.world_transform = make_world_matrix(
		CoordinateSystem::RH_Y_UP,
		{ 1,1,1 },
		{ 0,3.5f,0 },
		{ 0,0,0 },
		1.0f);
	library.is_animation = false;
	library.animation_index = 12;
	library.animation_time = 0.f;
	library.loop_animation = true;
	library.animation_speed = 0.01f;
	library.anim_mode = Gltf_Model::animation_mode::all;

	auto& mgr = Resource_Manager::instance().model_manager;
	mgr.add_instance("Library", library);

	IBL::Initialize(device, L"./data/ibl");

	// すべてのカメラの生成と登録
	HWND hwnd = Graphics_Core::instance().get_window_handle();
	auto& camera_mgr = CameraManager::instance();

	camera_mgr.register_camera("Spectator", std::make_shared<SpectatorCamera>(hwnd));
	camera_mgr.register_camera("ThirdPerson", std::make_shared<ThirdPersonCamera>(hwnd));
	camera_mgr.register_camera("FirstPerson", std::make_shared<FirstPersonCamera>(hwnd));

	dx::XMVECTOR target_pos = dx::XMVectorSet(0.0f, 3.5f, 0.0f, 1.0f);
	camera_mgr.register_camera("Orbit", std::make_shared<OrbitCamera>(hwnd, target_pos));
	camera_mgr.register_camera("QuarterView", std::make_shared<QuarterViewCamera>(target_pos));

	auto cinematic_cam = std::make_shared<CinematicCamera>();
	cinematic_cam->add_way_point(dx::XMVectorSet(-10.0f, 5.0f, -15.0f, 1.0f), target_pos);
	cinematic_cam->add_way_point(dx::XMVectorSet(-5.0f, 4.0f, -10.0f, 1.0f), target_pos);
	cinematic_cam->add_way_point(dx::XMVectorSet(0.0f, 8.0f, -8.0f, 1.0f), target_pos);
	cinematic_cam->add_way_point(dx::XMVectorSet(5.0f, 4.0f, -10.0f, 1.0f), target_pos);
	cinematic_cam->add_way_point(dx::XMVectorSet(10.0f, 5.0f, -15.0f, 1.0f), target_pos);
	camera_mgr.register_camera("Cinematic", cinematic_cam);

	camera_mgr.switch_camera("ThirdPerson");

	_player.initialize(hwnd, dx::XMVectorSet(0.0f, 3.0f, 0.0f, 1.0f));
	_player.setup_spotlight();
	_player.setup_collision("Library", library.world_transform);
	_player.setup_model_instance("Player");

	log_printf("ゲームシーン初期化終了\n", LogLevel::Info);
}

// 終了化
void Scene_Indoor::finalize() {
	Graphics_Core::instance().get_point_light_manager().clear_light();
	Graphics_Core::instance().get_spot_light_manager().clear_light();
	log_printf("ゲームシーン終了\n", LogLevel::Info);
}

// 更新処理
void Scene_Indoor::update(float elapsedTime)
{
	// 更新ロジックカテゴリ (薄黄色)
	TRACY_CPU_ZONE_C("Scene_Indoor::update", TracyCategory::Update);

	auto& camera_mgr = CameraManager::instance();
	const auto& graphics = Graphics_Core::instance();
	const float viewport_aspect_ratio = graphics.get_aspect_ratio();
	const float viewport_height = graphics.get_screen_height();

	// -----------------------------------------------------------------
	// カメラ更新
	// -----------------------------------------------------------------
	auto& active_cam = camera_mgr.get_active_camera()->get_camera();

	dx::XMFLOAT3 cam_pos_f, cam_tar_f;
	dx::XMStoreFloat3(&cam_pos_f, active_cam.position);
	dx::XMStoreFloat3(&cam_tar_f, active_cam.target);

	float fwd_x = cam_tar_f.x - cam_pos_f.x;
	float fwd_y = cam_tar_f.y - cam_pos_f.y;
	float fwd_z = cam_tar_f.z - cam_pos_f.z;

	float cam_yaw_deg = dx::XMConvertToDegrees(atan2f(fwd_x, fwd_z));
	float cam_pitch_deg = dx::XMConvertToDegrees(atan2f(fwd_y, sqrtf(fwd_x * fwd_x + fwd_z * fwd_z)));

	// -----------------------------------------------------------------
	// プレイヤー更新
	// -----------------------------------------------------------------
	_player.update(elapsedTime, cam_yaw_deg, cam_pitch_deg);

	dx::XMVECTOR player_pos = _player.get_position();

	// 毎フレーム string をコピー生成しないよう、const std::string& で参照
	const std::string& active_name = camera_mgr.get_active_camera_name();

	if (active_name == "ThirdPerson") {
		auto tp_cam = std::dynamic_pointer_cast<ThirdPersonCamera>(camera_mgr.get_active_camera());
		if (tp_cam) tp_cam->set_target_position(player_pos);
	}
	else if (active_name == "FirstPerson") {
		auto fp_cam = std::dynamic_pointer_cast<FirstPersonCamera>(camera_mgr.get_active_camera());
		if (fp_cam) {
			fp_cam->set_character_head_position(_player.get_head_position());
		}
	}
	camera_mgr.update(elapsedTime, viewport_aspect_ratio, viewport_height);

	// -----------------------------------------------------------------
	// ライト・リソース更新
	// -----------------------------------------------------------------
	_player.sync_spotlight();
	_player.sync_model_instance(active_name);

	_directional_light.update();
	_directional_light.tick(elapsedTime);
	IBL::Bind(Graphics_Core::instance().get_device_context());

	Graphics_Core::instance().get_point_light_manager().update();
	Graphics_Core::instance().get_point_light_manager().upload_to_gpu(Graphics_Core::instance().get_device_context());
	Graphics_Core::instance().get_spot_light_manager().update();
	Graphics_Core::instance().get_spot_light_manager().upload_to_gpu(Graphics_Core::instance().get_device_context());
	Graphics_Core::instance().get_area_light_manager().update();
	Graphics_Core::instance().get_area_light_manager().upload_to_gpu(Graphics_Core::instance().get_device_context());

	Resource_Manager::instance().model_manager.update(elapsedTime);
}

// 描画処理
void Scene_Indoor::render(float elapsedTime)
{
	// メイン描画パスカテゴリ (ロイヤルブルー)
	TRACY_CPU_ZONE_C("Scene_Indoor::render_CPU", TracyCategory::RenderPass);
	TRACY_GPU_ZONE_C("Scene_Indoor::render_GPU", TracyCategory::RenderPass);

	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	Render_State::instance().set_3d_render_states(immediate_context, Rasterizer_State::Cull_Back_CCW);

	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Render Scene Indoor");
		render_shadowmap(elapsedTime);
		render_defferd(elapsedTime);

		render_forward(elapsedTime);
		render_UI(elapsedTime);
	}
}

// 1. シャドウマップ描画
void Scene_Indoor::render_shadowmap(float elapsedTime) {
	// シャドウマップ描画カテゴリ (ゴールド系)
	TRACY_CPU_ZONE_C("Scene_Indoor::render_shadowmap_CPU", TracyCategory::Shadow);
	TRACY_GPU_ZONE_C("Scene_Indoor::render_shadowmap_GPU", TracyCategory::Shadow);

	DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Render Shadow Map");

	auto& shadower = Graphics_Core::instance().post_procss.GetShadow();

	// ディレクショナルライトのシャドウ
	{
		TRACY_CPU_ZONE_C("DirectionalShadow_CPU", TracyCategory::Shadow);
		TRACY_GPU_ZONE_C("DirectionalShadow_GPU", TracyCategory::Shadow);
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Render Directional Shadow");
		shadower.make_directional_shadow_begin();
		Resource_Manager::instance().model_manager.render_all(pass_mode::directional_shadow);
		shadower.make_shadow_end();
	}

	// ポイントライト（前方）のシャドウ
	{
		TRACY_CPU_ZONE_C("PointShadow_Front_CPU", TracyCategory::Shadow);
		TRACY_GPU_ZONE_C("PointShadow_Front_GPU", TracyCategory::Shadow);
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Render Point Shadow Front");
		shadower.make_shadow_begin(shadow::PointShadowFace::Front);
		Resource_Manager::instance().model_manager.render_all(pass_mode::shadow);
		shadower.make_shadow_end();
	}

	// ポイントライト（後方）のシャドウ
	{
		TRACY_CPU_ZONE_C("PointShadow_Back_CPU", TracyCategory::Shadow);
		TRACY_GPU_ZONE_C("PointShadow_Back_GPU", TracyCategory::Shadow);
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Render Point Shadow Back");
		shadower.make_shadow_begin(shadow::PointShadowFace::Back);
		Resource_Manager::instance().model_manager.render_all(pass_mode::shadow);
		shadower.make_shadow_end();
	}
}

// 2. デファード等倍G-Buffer書き込み
void Scene_Indoor::render_defferd(float elapsedTime) {
	// ジオメトリ（不透明）描画カテゴリ (エメラルドグリーン)
	TRACY_CPU_ZONE_C("Scene_Indoor::render_deferred_CPU", TracyCategory::Geometry);
	TRACY_GPU_ZONE_C("Scene_Indoor::render_deferred_GPU", TracyCategory::Geometry);

	DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Render Deferred");
	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	{
		TRACY_GPU_ZONE_C("Scene_Indoor::update_deferred_cb", TracyCategory::ConstantBuffer);
		Graphics_Core::instance().update_constants();
	}

	Graphics_Core::instance().get_geometry_buffer()->Clear(immediate_context);
	Graphics_Core::instance().get_geometry_buffer()->Activate(immediate_context);
	Render_State::instance().set_deferred_render_states(immediate_context, Rasterizer_State::Cull_None_CCW);

	Resource_Manager::instance().model_manager.render_all(pass_mode::deferred_geometry);
	Graphics_Core::instance().get_geometry_buffer()->Deactivate(immediate_context);
}

// 3. 前方描画（フォワード・不透明/半透明）
void Scene_Indoor::render_forward(float elapsedTime) {
	// ジオメトリ（半透明）描画カテゴリ (エメラルドグリーン)
	TRACY_CPU_ZONE_C("Scene_Indoor::render_forward_CPU", TracyCategory::Geometry);
	TRACY_GPU_ZONE_C("Scene_Indoor::render_forward_GPU", TracyCategory::Geometry);

	DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Render Forward");

	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	immediate_context->OMSetRenderTargets(1, Graphics_Core::instance().get_render_target_view().GetAddressOf(), Graphics_Core::instance().get_geometry_buffer()->GetDepthStencilView());

	Render_State::instance().set_2d_render_states(immediate_context);
	Graphics_Core::instance().clear(Color_Utils::from_hex("#291F32"));

	Graphics_Core::instance().post_procss.begin();
	Render_State::instance().set_3d_render_states(immediate_context, Rasterizer_State::Cull_Back_CW, Depth_State::Test_Enable_Write_Disable);

	Resource_Manager::instance().model_manager.render_all(pass_mode::forward_transparency);
}

// 4. UI・ポストプロセス描画
void Scene_Indoor::render_UI(float elapsedTime) {
	// ポストプロセスカテゴリ (マゼンタピンク)
	TRACY_CPU_ZONE_C("Scene_Indoor::render_UI_CPU", TracyCategory::PostProcess);
	TRACY_GPU_ZONE_C("Scene_Indoor::render_UI_GPU", TracyCategory::PostProcess);

	DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Render UI");

	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	Render_State::instance().set_2d_render_states(immediate_context);

	Graphics_Core::instance().post_procss.end();
	Graphics_Core::instance().post_procss.draw();
	Graphics_Core::instance().post_procss.render();

	// テキスト等のUI描画はUIカテゴリに変更してプロファイリング
	{
		TRACY_CPU_ZONE_C("Text_Draw_CPU", TracyCategory::UI);
		Text::text_data.Color = Color_Utils::hex_to_colorF("#F2D7EE");
		Text::draw(L"Indoor", D2D1_RECT_F(Graphics_Core::instance().get_screen_width(), Graphics_Core::instance().get_screen_height()), D2D1_DRAW_TEXT_OPTIONS_NONE, true);
	}
}

// 5. デバッグ描画
void Scene_Indoor::render_debug(float elapsedTime) {
	// デバッグ表示用カテゴリ (UIカテゴリで統合)
	TRACY_CPU_ZONE_C("Scene_Indoor::render_debug_CPU", TracyCategory::UI);
	TRACY_GPU_ZONE_C("Scene_Indoor::render_debug_GPU", TracyCategory::UI);

	DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Render Debug");

	Graphics_Core::instance().get_point_light_manager().debug_render();
	Graphics_Core::instance().get_spot_light_manager().debug_render();
	Graphics_Core::instance().get_area_light_manager().debug_render();
}

// GUI描画
void Scene_Indoor::draw_gui() {
	// ImGui系のGUI処理 (UIカテゴリ)
	TRACY_CPU_ZONE_C("Scene_Indoor::draw_gui", TracyCategory::UI);

	_directional_light.draw_imgui("Directional Light");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	_player.draw_imgui();
}