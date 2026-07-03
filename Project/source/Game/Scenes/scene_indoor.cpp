#include "Scene_indoor.h"
#include "Engine/system/manager/resource_manager.h"
#include "Engine/Utilities/math_util.h"
#include "Engine/Utilities/collision_util.h"

// 【追加】すべてのカメラクラスのヘッダーをインクルード
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

// -----------------------------------------------------------------------
// コリジョン対象インスタンス登録／解除／グリッド構築
// -----------------------------------------------------------------------

bool Scene_Indoor::register_collision_instance(const std::string& instance_name,
	const DirectX::XMFLOAT4X4& world_transform)
{
	auto& mgr = Resource_Manager::instance().model_manager;
	Gltf_Model* model = mgr.get_model(instance_name);
	if (!model)
	{
		log_printf("Collision instance not found: %s\n", LogLevel::Warning, instance_name.c_str());
		return false;
	}

	// 重複登録防止
	for (const auto& src : _collision_sources)
	{
		if (src.instance_name == instance_name)
		{
			log_printf("Collision instance already registered: %s\n", LogLevel::Warning, instance_name.c_str());
			return false;
		}
	}

	std::vector<DirectX::XMFLOAT3> tris;
	model->extract_collision_triangles(world_transform, tris);

	_collision_sources.push_back({ instance_name, world_transform });
	_collision_triangles.insert(_collision_triangles.end(), tris.begin(), tris.end());

	log_printf("Collision instance registered: '%s' (%zu triangles, total %zu)\n",
		LogLevel::Info, instance_name.c_str(), tris.size() / 3, _collision_triangles.size() / 3);

	return true;
}

void Scene_Indoor::clear_collision_instances()
{
	_collision_sources.clear();
	_collision_triangles.clear();
}

void Scene_Indoor::rebuild_collision_grid()
{
	_collision_grid.build(_collision_triangles);
}

// 初期化
void Scene_Indoor::initialize()
{
	log_printf("objモデルシーン開始\n", LogLevel::Info);
	log_printf("objモデルシーン初期化開始\n", LogLevel::Info);
	ID3D11Device* device = Graphics_Core::instance().get_device();

	// スポットライトの追加
	for (int i = 0; i < 1; i++) {
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

	// -----------------------------------------------------------------------
	// 当たり判定対象インスタンスの登録
	// -----------------------------------------------------------------------
	clear_collision_instances();
	register_collision_instance("Library", library.world_transform);
	rebuild_collision_grid();

	IBL::Initialize(device, L"./data/ibl");

	// -------------------------------------------------------------
	// すべてのカメラの生成と登録
	// -------------------------------------------------------------
	HWND hwnd = Graphics_Core::instance().get_window_handle();
	auto& camera_mgr = Camera_Manager::instance();

	// 1. スペクテイターカメラ
	camera_mgr.register_camera("Spectator", std::make_shared<Spectator_Camera>(hwnd));

	// 2. サードパーソンカメラ
	camera_mgr.register_camera("ThirdPerson", std::make_shared<Third_Person_Camera>(hwnd));

	// 3. ファーストパーソンカメラ
	camera_mgr.register_camera("FirstPerson", std::make_shared<First_Person_Camera>(hwnd));

	// 4. オービットカメラ（初期注視点をLibraryモデルの座標にする）
	dx::XMVECTOR target_pos = dx::XMVectorSet(0.0f, 3.5f, 0.0f, 1.0f);
	camera_mgr.register_camera("Orbit", std::make_shared<Orbit_Camera>(hwnd, target_pos));

	// 5. クォータービューカメラ
	camera_mgr.register_camera("QuarterView", std::make_shared<Quarter_View_Camera>(target_pos));

	// 6. シネマティックカメラ
	auto cinematic_cam = std::make_shared<Cinematic_Camera>();
	cinematic_cam->add_way_point(dx::XMVectorSet(-10.0f, 5.0f, -15.0f, 1.0f), target_pos);
	cinematic_cam->add_way_point(dx::XMVectorSet(-5.0f, 4.0f, -10.0f, 1.0f), target_pos);
	cinematic_cam->add_way_point(dx::XMVectorSet(0.0f, 8.0f, -8.0f, 1.0f), target_pos);
	cinematic_cam->add_way_point(dx::XMVectorSet(5.0f, 4.0f, -10.0f, 1.0f), target_pos);
	cinematic_cam->add_way_point(dx::XMVectorSet(10.0f, 5.0f, -15.0f, 1.0f), target_pos);
	camera_mgr.register_camera("Cinematic", cinematic_cam);

	// デフォルトカメラをサードパーソンに設定
	camera_mgr.switch_camera("ThirdPerson");

	// -----------------------------------------------------------------
	// プレイヤー（ドローン）初期化
	// -----------------------------------------------------------------
	auto& spot_mgr = Graphics_Core::instance().get_spot_light_manager();
	_player_spotlight_index = static_cast<int>(spot_mgr.get_lights().size());
	spot_mgr.add_light(
		{ 0.0f, -0.8f, 0.0f },
		{ 0.0f, -1.0f, 0.0f },
		20.0f,
		12.0f,
		0.10f,
		0.30f,
		{ 1.0f, 0.9f, 0.7f, 1.0f }
	);

	_player.initialize(
		hwnd,
		dx::XMVectorSet(0.0f, 3.0f, 0.0f, 1.0f),
		_player_spotlight_index
	);

	// -------------------------------------------------------------
	// プレイヤーモデルの登録（ドローン）
	// -------------------------------------------------------------
	ModelInstance player_model;
	player_model.model_key = _player.get_model_name();  // ← モデル名をPlayerから取得
	player_model.world_transform = _player.get_world_transform();
	player_model.is_animation = true;
	player_model.animation_index = 0;
	player_model.animation_time = 0.0f;
	player_model.loop_animation = true;
	player_model.animation_speed = 1.0f;
	player_model.anim_mode = Gltf_Model::animation_mode::single;

	mgr.add_instance("Player", player_model);

	log_printf("objモデルシーン初期化終了\n", LogLevel::Info);
}

// 終了化
void Scene_Indoor::finalize() {
	Graphics_Core::instance().get_point_light_manager().clear_light();
	Graphics_Core::instance().get_spot_light_manager().clear_light();
	clear_collision_instances();
	log_printf("objモデルシーン終了\n", LogLevel::Info);
}

// 更新処理
void Scene_Indoor::update(float elapsedTime)
{
	auto& camera_mgr = Camera_Manager::instance();

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
	// コリジョン三角形をプレイヤー周辺だけに絞り込む
	// -----------------------------------------------------------------
	dx::XMFLOAT3 player_pos_f;
	dx::XMStoreFloat3(&player_pos_f, _player.get_position());
	_collision_grid.query(player_pos_f, COLLISION_QUERY_RADIUS, _nearby_collision_triangles);

	// プレイヤー更新
	_player.update(elapsedTime, cam_yaw_deg, cam_pitch_deg,
		_nearby_collision_triangles.empty() ? nullptr : &_nearby_collision_triangles);

	dx::XMVECTOR player_pos = _player.get_position();

	std::string active_name = camera_mgr.get_active_camera_name();

	if (active_name == "ThirdPerson") {
		auto tp_cam = std::dynamic_pointer_cast<Third_Person_Camera>(camera_mgr.get_active_camera());
		if (tp_cam) tp_cam->set_target_position(player_pos);
		camera_mgr.update(elapsedTime);
	}
	else if (active_name == "FirstPerson") {
		auto fp_cam = std::dynamic_pointer_cast<First_Person_Camera>(camera_mgr.get_active_camera());
		if (fp_cam) {
			fp_cam->set_character_head_position(_player.get_head_position());
		}
		camera_mgr.update(elapsedTime);
	}
	else {
		camera_mgr.update(elapsedTime);
	}

	// -----------------------------------------------------------------
	// プレイヤー真下スポットライトを毎フレーム更新
	// -----------------------------------------------------------------
	auto& spot_lights = Graphics_Core::instance().get_spot_light_manager().get_lights();
	if (_player_spotlight_index >= 0 &&
		_player_spotlight_index < static_cast<int>(spot_lights.size()))
	{
		auto& pl = spot_lights[_player_spotlight_index];
		pl.position = _player.get_light_position();
		pl.direction = _player.get_light_direction();
		pl.radius = _player.get_light_radius();
		pl.intensity = _player.get_light_intensity();
		pl.innerAngle = _player.get_light_inner_angle();
		pl.outerAngle = _player.get_light_outer_angle();
		pl.diffuseColor = _player.get_light_color();
	}

	_directional_light.update();
	_directional_light.tick(elapsedTime);
	IBL::Bind(Graphics_Core::instance().get_device_context());
	Graphics_Core::instance().get_point_light_manager().update();
	Graphics_Core::instance().get_point_light_manager().upload_to_gpu(Graphics_Core::instance().get_device_context());
	Graphics_Core::instance().get_spot_light_manager().update();
	Graphics_Core::instance().get_spot_light_manager().upload_to_gpu(Graphics_Core::instance().get_device_context());
	Graphics_Core::instance().get_area_light_manager().update();
	Graphics_Core::instance().get_area_light_manager().upload_to_gpu(Graphics_Core::instance().get_device_context());

	// -----------------------------------------------------------------
	// プレイヤーモデルインスタンスの更新とデバッグ操作の受付
	// -----------------------------------------------------------------
	static bool k_pressed = false;
	if (GetAsyncKeyState('K') & 0x8000)
	{
		if (!k_pressed)
		{
			_player.set_dead(!_player.is_dead());
			k_pressed = true;
		}
	}
	else
	{
		k_pressed = false;
	}

	auto player_inst = Resource_Manager::instance().model_manager.get_instance("Player");
	if (player_inst)
	{
		player_inst->world_transform = _player.get_world_transform();
		player_inst->animation_speed = _player.get_animation_speed();
		player_inst->loop_animation = true;

		// モデル名が変更されたら再登録
		static std::string last_model_name = _player.get_model_name();
		if (last_model_name != _player.get_model_name())
		{
			last_model_name = _player.get_model_name();

			// 古いインスタンスを削除
			Resource_Manager::instance().model_manager.remove_instance("Player");

			// 新しいモデルで再登録
			ModelInstance new_model;
			new_model.model_key = _player.get_model_name();
			new_model.world_transform = _player.get_world_transform();
			new_model.is_animation = true;
			new_model.animation_index = 0;
			new_model.animation_time = 0.0f;
			new_model.loop_animation = true;
			new_model.animation_speed = 1.0f;
			new_model.anim_mode = Gltf_Model::animation_mode::single;

			Resource_Manager::instance().model_manager.add_instance("Player", new_model);
			player_inst = Resource_Manager::instance().model_manager.get_instance("Player");
		}

		int next_anim_index = _player.get_animation_index();
		if (player_inst->animation_index != next_anim_index)
		{
			player_inst->animation_index = next_anim_index;
			player_inst->animation_time = 0.0f;
		}
		std::string active_name = camera_mgr.get_active_camera_name();
		if (active_name == "FirstPerson") {
			player_inst->visible = false;
		}
		else {
			player_inst->visible = true;
		}
	}

	Resource_Manager::instance().model_manager.update(elapsedTime);
}

// 描画処理
void Scene_Indoor::render(float elapsedTime) {
	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	Render_State::instance().set_3d_render_states(immediate_context, Rasterizer_State::Cull_Back_CCW);
	render_shadowmap(elapsedTime);
	render_defferd(elapsedTime);
	render_forward(elapsedTime);
	render_UI(elapsedTime);
}

void Scene_Indoor::render_shadowmap(float elapsedTime) {
	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	auto& shadower = Graphics_Core::instance().post_procss.GetShadow();

	shadower.make_directional_shadow_begin();
	Resource_Manager::instance().model_manager.render_all(pass_mode::directional_shadow);
	shadower.make_shadow_end();

	shadower.make_shadow_begin(shadow::PointShadowFace::Front);
	Resource_Manager::instance().model_manager.render_all(pass_mode::shadow);
	shadower.make_shadow_end();

	shadower.make_shadow_begin(shadow::PointShadowFace::Back);
	Resource_Manager::instance().model_manager.render_all(pass_mode::shadow);
	shadower.make_shadow_end();
}

void Scene_Indoor::render_defferd(float elapsedTime) {
	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	Graphics_Core::instance().update_constants();

	Graphics_Core::instance().get_geometry_buffer()->Clear(Graphics_Core::instance().get_device_context());
	Graphics_Core::instance().get_geometry_buffer()->Activate(Graphics_Core::instance().get_device_context());
	Render_State::instance().set_deferred_render_states(immediate_context, Rasterizer_State::Cull_None_CCW);

	Resource_Manager::instance().model_manager.render_all(pass_mode::deferred_geometry);
	Graphics_Core::instance().get_geometry_buffer()->Deactivate(Graphics_Core::instance().get_device_context());
}

void Scene_Indoor::render_forward(float elapsedTime) {
	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	immediate_context->OMSetRenderTargets(1, Graphics_Core::instance().get_render_target_view().GetAddressOf(), Graphics_Core::instance().get_geometry_buffer()->GetDepthStencilView());

	Render_State::instance().set_2d_render_states(immediate_context);
	Graphics_Core::instance().clear(Color_Utils::from_hex("#291F32"));

	Graphics_Core::instance().post_procss.begin();
	Render_State::instance().set_3d_render_states(immediate_context, Rasterizer_State::Cull_Back_CW, Depth_State::Test_Enable_Write_Disable);

	Resource_Manager::instance().model_manager.render_all(pass_mode::forward_transparency);
}

void Scene_Indoor::render_UI(float elapsedTime) {
	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	Render_State::instance().set_2d_render_states(immediate_context);

	Graphics_Core::instance().post_procss.end();
	Graphics_Core::instance().post_procss.draw();
	Graphics_Core::instance().post_procss.render();

	Text::text_data.Color = Color_Utils::hex_to_colorF("#F2D7EE");

	Text::draw(L"Indoor", D2D1_RECT_F(Graphics_Core::instance().get_screen_width(), Graphics_Core::instance().get_screen_height()), D2D1_DRAW_TEXT_OPTIONS_NONE, true);
}

void Scene_Indoor::render_debug(float elapsedTime) {
	Graphics_Core::instance().get_point_light_manager().debug_render();
	Graphics_Core::instance().get_spot_light_manager().debug_render();
	Graphics_Core::instance().get_area_light_manager().debug_render();
}

// GUI描画
void Scene_Indoor::draw_gui() {
	_directional_light.draw_imgui("Directional Light");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	_player.draw_imgui();
}