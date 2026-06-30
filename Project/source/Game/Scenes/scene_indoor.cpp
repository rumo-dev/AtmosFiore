#include "Scene_indoor.h"
#include "Engine/system/manager/resource_manager.h"
#include "Engine/Utilities/math_util.h"

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

// 初期化
void Scene_Indoor::initialize()
{
	log_printf("objモデルシーン開始\n", LogLevel::Info);
	log_printf("objモデルシーン初期化開始\n", LogLevel::Info);
	ID3D11Device* device = Graphics_Core::instance().get_device();

	//Graphics_Core::instance().get_point_light_manager().add_light({ 0,2,1 });
	//ライトを1024個追加してみる（）
	//色や強度もランダム

	// スポットライトの追加
	//Graphics_Core::instance().get_spot_light_manager().add_light(
	//	{ 0, 5, 0 },           // 位置
	//	{ 0, -1, 0 },          // 方向（下向き）
	//	10.0f,                 // 半径
	//	2.0f,                  // 強度
	//	0.3f,                  // 内角
	//	0.6f,                  // 外角
	//	{ 1.0f, 0.8f, 0.6f, 1.0f } // 色
	//);
	//spotLightを1024個Intencity以外ランダムで追加

	for (int i = 0; i < 1; i++) {
		float x = Random::Range(-10.0f, 10.0f); // -10～10の範囲で配置
		float z = Random::Range(-10.0f, 10.0f); // -10～10の範囲で配置
		float y = Random::Range(3.0f, 7.0f); // 高さをランダム化
		float r = Random::Range(5.0f, 15.0f); // 半径をランダム化
		float intensity = Random::Range(5.0f, 15.0f); // 強度をランダム化
		float innerAngle = Random::Range(0.1f, 0.5f); // 内角をランダム化
		float outerAngle = Random::Range(0.4f, 0.8f); // 外角をランダム化
		dx::XMFLOAT3 direction = { Random::Range(-1.0f, 1.0f), Random::Range(-1.0f, -0.5f), Random::Range(-1.0f, 1.0f) }; // ランダムな方向（下向きが多め）
		dx::XMFLOAT4 diffuseColor = Color_Utils::random_hsv(1.0f, 1.0f, 1.0f); // ランダムな色相の明るいz色
		Graphics_Core::instance().get_spot_light_manager().add_light({ x, y, z }, direction, r, intensity, innerAngle, outerAngle, diffuseColor);
	}

	for (int i = 0; i < 1; i++) {
		float x = Random::Range(-10.0f, 10.0f); // -10～10の範囲で配置
		float z = Random::Range(-10.0f, 10.0f); // -10～10の範囲で配置
		float y = Random::Range(3.0f, 7.0f); // 高さをランダム化
		float r = 5.0f; // 半径は固定（必要に応じてランダム化も可能）
		float intensity = 10.0f; // 強度は固定（必要に応じてランダム化も可能）
		dx::XMFLOAT4 diffuseColor = Color_Utils::random_hsv(1.0f, 1.0f, 1.0f); // ランダムな色相の明るい色
		Graphics_Core::instance().get_point_light_manager().add_light({ x, y, z }, r, intensity, diffuseColor);
	}
	Graphics_Core::instance().get_area_light_manager().add_light();

	ModelInstance s;
	s.model_key = "Library";
	s.world_transform = make_world_matrix(
		CoordinateSystem::RH_Y_UP,
		{ 1,1,1 },
		{ 0,3.5f,0 },
		{ 0,0,0 },
		1.0f);
	s.is_animation = false;
	s.animation_index = 12;
	s.animation_time = 0.f;
	s.loop_animation = true;
	s.animation_speed = 0.01f;
	s.anim_mode = Gltf_Model::animation_mode::all;



	//// 追加時のデバッグ
	auto& mgr = Resource_Manager::instance().model_manager;

	mgr.add_instance("Library", s);
	ModelInstance c;
	c.world_transform = make_world_matrix(
		CoordinateSystem::RH_Y_UP,
		{ 1,1,1 },
		{ 0,3.5f,0 },
		{ 0,3,0 },
		1.f);
	c.model_key = "DamagedHelmet";
	mgr.add_instance("DamagedHelmet", c);


	IBL::Initialize(device, L"./data/ibl");

	// -------------------------------------------------------------
	// ?? すべてのカメラの生成と登録
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
	// ルート（ウェイポイント）の登録（ダミー → 本番ポイント → ダミー）
	cinematic_cam->add_way_point(dx::XMVectorSet(-10.0f, 5.0f, -15.0f, 1.0f), target_pos); // P0:ダミー
	cinematic_cam->add_way_point(dx::XMVectorSet(-5.0f, 4.0f, -10.0f, 1.0f), target_pos); // P1:始点
	cinematic_cam->add_way_point(dx::XMVectorSet(0.0f, 8.0f, -8.0f, 1.0f), target_pos); // P2:経由
	cinematic_cam->add_way_point(dx::XMVectorSet(5.0f, 4.0f, -10.0f, 1.0f), target_pos); // P3:終点
	cinematic_cam->add_way_point(dx::XMVectorSet(10.0f, 5.0f, -15.0f, 1.0f), target_pos); // P4:ダミー
	camera_mgr.register_camera("Cinematic", cinematic_cam);

	// デフォルトカメラをスペクテイターに設定
	camera_mgr.switch_camera("Spectator");
	float elapsedTime = 0.0f; // 初期化時のダミー値

	// -----------------------------------------------------------------
	// プレイヤー初期化
	// -----------------------------------------------------------------
	// プレイヤー前方を照らすスポットライトを事前に登録し、インデックスを記憶する
	auto& spot_mgr = Graphics_Core::instance().get_spot_light_manager();
	_player_spotlight_index = static_cast<int>(spot_mgr.get_lights().size());
	spot_mgr.add_light(
		{ 0.0f, 1.5f, 0.0f },   // 初期位置（後で毎フレーム上書き）
		{ 0.0f, 0.0f, 1.0f },   // 初期方向（後で毎フレーム上書き）
		15.0f,                   // 半径
		8.0f,                    // 強度
		0.15f,                   // 内角
		0.40f,                   // 外角
		{ 1.0f, 0.95f, 0.85f, 1.0f } // 暖色系白
	);

	_player.initialize(
		hwnd,
		dx::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
		_player_spotlight_index
	);

	log_printf("objモデルシーン初期化終了\n", LogLevel::Info);
}

// 終了化
void Scene_Indoor::finalize() {
	//sprite_object[static_cast<int>(Sprite_Type::None)] & delete;
	Graphics_Core::instance().get_point_light_manager().clear_light();
	Graphics_Core::instance().get_spot_light_manager().clear_light();
	log_printf("objモデルシーン終了\n", LogLevel::Info);
}

// 更新処理
void Scene_Indoor::update(float elapsedTime)
{
	auto& camera_mgr = Camera_Manager::instance();


	// -----------------------------------------------------------------
	// カメラ更新
	// -----------------------------------------------------------------
	// アクティブカメラの向き（Yaw）をビュー行列から逆算してプレイヤーに渡す
	auto& active_cam = camera_mgr.get_active_camera()->get_camera();

	// カメラ前方ベクトル（target - position）からYaw・Pitchを計算
	dx::XMFLOAT3 cam_pos_f, cam_tar_f;
	dx::XMStoreFloat3(&cam_pos_f, active_cam.position);
	dx::XMStoreFloat3(&cam_tar_f, active_cam.target);

	float fwd_x = cam_tar_f.x - cam_pos_f.x;
	float fwd_y = cam_tar_f.y - cam_pos_f.y;
	float fwd_z = cam_tar_f.z - cam_pos_f.z;

	float cam_yaw_deg   = dx::XMConvertToDegrees(atan2f(fwd_x, fwd_z));
	float cam_pitch_deg = dx::XMConvertToDegrees(atan2f(fwd_y, sqrtf(fwd_x * fwd_x + fwd_z * fwd_z)));

	// プレイヤー更新
	_player.update(elapsedTime, cam_yaw_deg, cam_pitch_deg);

	// プレイヤーの実座標
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
		// 引数の要らない通常カメラ（Spectator, Orbit, QuarterView, Cinematic）は一括更新
		camera_mgr.update(elapsedTime);
	}

	// -----------------------------------------------------------------
	// プレイヤー前方スポットライトを毎フレーム更新
	// -----------------------------------------------------------------
	auto& spot_lights = Graphics_Core::instance().get_spot_light_manager().get_lights();
	if (_player_spotlight_index >= 0 &&
		_player_spotlight_index < static_cast<int>(spot_lights.size()))
	{
		auto& pl = spot_lights[_player_spotlight_index];
		pl.position     = _player.get_light_position();
		pl.direction    = _player.get_light_direction();
		pl.radius       = _player.get_light_radius();
		pl.intensity    = _player.get_light_intensity();
		pl.innerAngle   = _player.get_light_inner_angle();
		pl.outerAngle   = _player.get_light_outer_angle();
		pl.diffuseColor = _player.get_light_color();
	}

	// _directional_light.set_camera(Camera_Manager::instance().get_active_camera()->get_camera());
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
void Scene_Indoor::render(float elapsedTime) {
	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	Render_State::instance().set_3d_render_states(immediate_context, Rasterizer_State::Cull_Back_CCW);
	render_shadowmap(elapsedTime);
	render_defferd(elapsedTime);
	render_forward(elapsedTime);
	//render_debug(elapsedTime);
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
