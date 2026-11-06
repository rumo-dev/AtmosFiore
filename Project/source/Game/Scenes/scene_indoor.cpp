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
#include <algorithm>
#include "Engine/Utilities/color_Util.h"

// Tracy Profiler Utility Header (Tracy.hpp や D3D11マクロ、カテゴリ定義が含まれます)
#include "tracy_util.h"

// 初期化
void Scene_Indoor::initialize()
{
	log_printf("ゲームシーン開始\n", LogLevel::Info);
	log_printf("ゲームシーン初期化開始\n", LogLevel::Info);
	ID3D11Device* device = Graphics_Core::instance().get_device();

	// Indoor.gltf の座標は cm 相当。Room 2 をプレイヤーの周囲に置く。
	constexpr float source_to_meters = 0.01f;
	constexpr float room_offset_x = -30.0f;
	auto& graphics = Graphics_Core::instance();
	graphics.post_procss.UseSettingsFile("data/post_process_indoor.json");

	// 天井の開口部から午後の日光を入れる。方向光のシャドウが屋根と梁を遮蔽する。
	_directional_light.illuminance_lux = 80000.0f;
	_directional_light.ambientColor = { 0.04f, 0.04f, 0.04f, 1.0f };
	_directional_light.set_time_of_day(14.0f, false);

	// インドアモデルの登録
	auto& mgr = Resource_Manager::instance();
	Gltf_Model* indoor_model = mgr.load_model("Indoor", "./data/model/Indoor/Indoor.gltf");
	ModelInstance indoor;
	indoor.model_key = "Indoor";
	indoor.world_transform = make_world_matrix(
		CoordinateSystem::RH_Y_UP,
		{ 1.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ room_offset_x, 0.0f, 0.0f }, source_to_meters);
	indoor.is_animation = false;
	indoor.disable_frustum_culling = true; // ノードが広範囲に分布するモデル
	mgr.add_model_instance("Indoor", indoor);
	_indoor_world = indoor.world_transform;
	if (indoor_model)
	{
		auto& spots = graphics.get_spot_light_manager();
		auto& points = graphics.get_point_light_manager();
		const dx::XMFLOAT4 warm_white = { 1.0f, 0.88f, 0.72f, 1.0f };
		const dx::XMFLOAT4 neutral_white = { 1.0f, 0.96f, 0.88f, 1.0f };
		const dx::XMMATRIX scene_world = dx::XMLoadFloat4x4(&_indoor_world);
		int normal_hotspots = 0;
		int bulb_hotspots = 0;
		for (const auto& node : indoor_model->nodes)
		{
			const std::string& name = node.name;
			const bool is_spot = name.rfind("Light_Spot_", 0) == 0;
			const bool is_point = name.rfind("Light_Point_", 0) == 0;
			if (!is_spot && !is_point) continue;

			// ノードの親子変換とモデルの RH→LH 変換を両方適用する。
			const dx::XMMATRIX fixture_world =
				dx::XMLoadFloat4x4(&node.global_transform) * scene_world;
			dx::XMFLOAT3 position;
			dx::XMStoreFloat3(&position, dx::XMVector3TransformCoord(
				dx::XMVectorZero(), fixture_world));
			if (is_spot)
			{
				SpotLight light;
				light.position = position;
				dx::XMStoreFloat3(&light.direction, dx::XMVector3Normalize(
					dx::XMVector3TransformNormal(dx::XMVectorSet(0, -1, 0, 0), fixture_world)));
				const bool floor_spot = name.rfind("Light_Spot_800lm", 0) == 0;
				const bool large_spot = name.rfind("Light_Spot_3200lm", 0) == 0;
				light.radius = large_spot ? 8.0f : (floor_spot ? 5.0f : 6.0f);
				light.intensity = large_spot ? 12000.0f : (floor_spot ? 5000.0f :
					name.rfind("Light_Spot_1600lm", 0) == 0 ? 8000.0f : 6000.0f);
				light.innerAngle = floor_spot ? 0.30f : 0.38f;
				light.outerAngle = floor_spot ? 0.55f : 0.65f;
				light.diffuseColor = floor_spot ? warm_white : neutral_white;
				spots.add_light(light);
			}
			else
			{
				PointLight light;
				light.position = position;
				const bool small_hotspot = name.rfind("Light_Point_Hotspot_Small", 0) == 0;
				const bool bulb = name.rfind("Light_Point_Bulb", 0) == 0;
				light.radius = small_hotspot ? 3.0f : (bulb ? 5.0f : 6.0f);
				light.intensity = small_hotspot ? 800.0f : (bulb ? 4000.0f : 8000.0f);
				light.diffuseColor = small_hotspot || bulb ? warm_white : neutral_white;
				// 影はモデル内のポイント光だけに割り当てる。アトラスの上限は 16 灯。
				light.cast_shadows = bulb ? bulb_hotspots++ == 0 :
					!small_hotspot && normal_hotspots++ % 3 == 0 && normal_hotspots <= 24;
				points.add_light(light);
			}
		}
	}
	_voxel_gi = std::make_unique<VoxelGI>();
	_voxel_gi->set_bounds_root("Structure");
	_voxel_gi->set_debug_slice_height(5.2f);
	_voxel_gi->set_resolution(192);
	_voxel_gi->set_emissive_multiplier(2.0f);
	_voxel_gi->set_strength(0.8f);
	if (!_voxel_gi->initialize(device, indoor_model, _indoor_world))
		_voxel_gi.reset();
	if (_voxel_gi)
	{
		_ddgi = std::make_unique<DDGI>();
		_ddgi->set_probe_bounds({ 20.0f, 0.0f, -8.0f }, { 36.0f, 10.0f, 7.0f });
		if (!_ddgi->initialize(device)) _ddgi.reset();
	}

	IBL::Initialize(device, L"./data/ibl");
	_reflection_probes_available = initialize_reflection_probes(device);
	if (!_reflection_probes_available) _reflection_probes_enabled = false;

	// すべてのカメラの生成と登録
	HWND hwnd = Graphics_Core::instance().get_window_handle();
	auto& camera_mgr = CameraManager::instance();

	auto indoor_camera = [](const std::shared_ptr<ICamera>& camera)
	{
		auto& lens = camera->get_camera();
		lens.FocalLength = 35.0f;
		lens.SensorSize = 36.0f;
		lens.near_z = 0.08f;
		lens.far_z = 250.0f;
		lens.FNumber = 8.0f;
		lens.FocusDist = 6.0f;
		lens.shutterDenominator = 60.0f;
		lens.ISO = 400.0f;
		lens.EV_Compensation = 0.0f;
		lens.autoSettingsEnabled = false;
		return camera;
	};
	camera_mgr.register_camera("Spectator", indoor_camera(std::make_shared<SpectatorCamera>(hwnd)));
	camera_mgr.register_camera("ThirdPerson", indoor_camera(std::make_shared<ThirdPersonCamera>(hwnd)));
	camera_mgr.register_camera("FirstPerson", indoor_camera(std::make_shared<FirstPersonCamera>(hwnd)));

	dx::XMVECTOR target_pos = dx::XMVectorSet(0.0f, 3.5f, 0.0f, 1.0f);
	camera_mgr.register_camera("Orbit", indoor_camera(std::make_shared<OrbitCamera>(hwnd, target_pos)));
	camera_mgr.register_camera("QuarterView", indoor_camera(std::make_shared<QuarterViewCamera>(target_pos)));

	auto cinematic_cam = std::make_shared<CinematicCamera>();
	cinematic_cam->add_way_point(dx::XMVectorSet(-10.0f, 5.0f, -15.0f, 1.0f), target_pos);
	cinematic_cam->add_way_point(dx::XMVectorSet(-5.0f, 4.0f, -10.0f, 1.0f), target_pos);
	cinematic_cam->add_way_point(dx::XMVectorSet(0.0f, 8.0f, -8.0f, 1.0f), target_pos);
	cinematic_cam->add_way_point(dx::XMVectorSet(5.0f, 4.0f, -10.0f, 1.0f), target_pos);
	cinematic_cam->add_way_point(dx::XMVectorSet(10.0f, 5.0f, -15.0f, 1.0f), target_pos);
	camera_mgr.register_camera("Cinematic", indoor_camera(cinematic_cam));

	camera_mgr.switch_camera("ThirdPerson");

	_player.initialize(hwnd, dx::XMVectorSet(0.0f, 3.0f, 0.0f, 1.0f));
	_player.setup_spotlight();
	_player.setup_collision("Indoor", indoor.world_transform);
	_player.setup_model_instance("Player");

	log_printf("ゲームシーン初期化終了\n", LogLevel::Info);
}

// 終了化
void Scene_Indoor::finalize() {
	Graphics_Core::instance().post_procss.UseSettingsFile("data/post_process_settings.json");
	ID3D11DeviceContext* context = Graphics_Core::instance().get_device_context();
	ID3D11ShaderResourceView* null_probe_srvs[ReflectionProbeCount]{};
	ID3D11Buffer* null_probe_buffer = nullptr;
	context->PSSetShaderResources(36, ReflectionProbeCount, null_probe_srvs);
	context->PSSetConstantBuffers(10, 1, &null_probe_buffer);
	ID3D11ShaderResourceView* null_gi_srvs[2]{};
	ID3D11Buffer* null_gi_buffers[2]{};
	context->PSSetShaderResources(13, 2, null_gi_srvs);
	context->PSSetConstantBuffers(8, 2, null_gi_buffers);
	_ddgi.reset();
	_voxel_gi.reset();
	Graphics_Core::instance().get_point_light_manager().clear_light();
	Graphics_Core::instance().get_spot_light_manager().clear_light();
	Graphics_Core::instance().get_area_light_manager().clear_light();
	Resource_Manager::instance().remove_model_instance("Indoor");
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
	// デバッグカメラ操作中は同じ WASD 入力でプレイヤーを動かさない。
	const std::string& active_name = camera_mgr.get_active_camera_name();
	if (active_name != "Meshlet Debug")
		_player.update(elapsedTime, cam_yaw_deg, cam_pitch_deg);

	dx::XMVECTOR player_pos = _player.get_position();

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

	Resource_Manager::instance().update_models(elapsedTime);
}

// 描画処理
void Scene_Indoor::render(float elapsedTime)
{
	// メイン描画パスカテゴリ (
	TRACY_CPU_ZONE_C("Scene_Indoor::render_CPU", TracyCategory::RenderPass);
	TRACY_GPU_ZONE_C("Scene_Indoor::render_GPU", TracyCategory::RenderPass);

	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	Render_State::instance().set_3d_render_states(immediate_context, Rasterizer_State::Cull_Back_CCW);

	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Render Scene Indoor");
		render_shadowmap(elapsedTime);
		Graphics_Core::instance().update_constants();
		capture_reflection_probe(immediate_context);
		if (_voxel_gi)
			_voxel_gi->render(immediate_context,
				Resource_Manager::instance().get_model("Indoor"), _indoor_world);
		if (_ddgi && _voxel_gi) _ddgi->update(immediate_context, *_voxel_gi);
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
		Resource_Manager::instance().render_models(pass_mode::directional_shadow);
		shadower.make_shadow_end();
	}

	// Keep exactly two dual-paraboloid textures and pack enabled point lights into 4x4 atlas cells.
	auto& pointLights = Graphics_Core::instance().get_point_light_manager().get_lights();
	const Camera& camera = CameraManager::instance().get_active_camera()->get_camera();
	const Frustum camera_frustum = ExtractFrustum(camera.view, camera.projection);
	bool clearFrontAtlas = true;
	bool clearBackAtlas = true;
	for (int lightIndex = 0; shadower.point_shadow_enabled && lightIndex < static_cast<int>(pointLights.size()); ++lightIndex)
	{
		const auto& light = pointLights[lightIndex];
		if (light.shadow_tile < 0)
			continue;
		// No visible surface can be lit by a light whose entire influence
		// volume lies outside the current camera frustum.
		const dx::XMVECTOR light_min = dx::XMVectorSet(
			light.position.x - light.radius, light.position.y - light.radius,
			light.position.z - light.radius, 0.0f);
		const dx::XMVECTOR light_max = dx::XMVectorSet(
			light.position.x + light.radius, light.position.y + light.radius,
			light.position.z + light.radius, 0.0f);
		if (!AABBIntersectsFrustum(camera_frustum, light_min, light_max))
			continue;

		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Render Point Shadow Light");
		{
			DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Point Shadow Front");
			shadower.make_shadow_begin(shadow::PointShadowFace::Front, lightIndex, clearFrontAtlas);
			clearFrontAtlas = false;
			Resource_Manager::instance().render_models(pass_mode::shadow);
			shadower.make_shadow_end();
		}
		{
			DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Point Shadow Back");
			shadower.make_shadow_begin(shadow::PointShadowFace::Back, lightIndex, clearBackAtlas);
			clearBackAtlas = false;
			Resource_Manager::instance().render_models(pass_mode::shadow);
			shadower.make_shadow_end();
		}
	}
}

// 2. デファード等倍G-Buffer書き込み
void Scene_Indoor::render_defferd(float elapsedTime) {
	// ジオメトリ（不透明）描画カテゴリ (エメラルドグリーン)
	TRACY_CPU_ZONE_C("Scene_Indoor::render_deferred_CPU", TracyCategory::Geometry);
	TRACY_GPU_ZONE_C("Scene_Indoor::render_deferred_GPU", TracyCategory::Geometry);

	DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Render Deferred");
	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	Graphics_Core::instance().get_geometry_buffer()->Clear(immediate_context);
	Graphics_Core::instance().get_geometry_buffer()->Activate(immediate_context);
	Render_State::instance().set_deferred_render_states(immediate_context, Rasterizer_State::Cull_None_CCW);

	Resource_Manager::instance().begin_model_profile(immediate_context);
	Resource_Manager::instance().render_models(pass_mode::deferred_geometry);
	Resource_Manager::instance().issue_model_occlusion_queries(immediate_context);
	Resource_Manager::instance().end_model_profile(immediate_context);
	Graphics_Core::instance().get_geometry_buffer()->Deactivate(immediate_context);
	if (_voxel_gi) _voxel_gi->bind_lighting(immediate_context);
	if (_ddgi) _ddgi->bind_lighting(immediate_context);
	else
	{
		ID3D11ShaderResourceView* null_srv = nullptr;
		ID3D11Buffer* null_buffer = nullptr;
		immediate_context->PSSetShaderResources(14, 1, &null_srv);
		immediate_context->PSSetConstantBuffers(9, 1, &null_buffer);
	}
	bind_reflection_probes(immediate_context);
}

// 3. 前方描画（フォワード・不透明/半透明）
void Scene_Indoor::render_forward(float elapsedTime) {
	// ジオメトリ（半透明）描画カテゴリ (エメラルドグリーン)
	TRACY_CPU_ZONE_C("Scene_Indoor::render_forward_CPU", TracyCategory::Geometry);
	TRACY_GPU_ZONE_C("Scene_Indoor::render_forward_GPU", TracyCategory::Geometry);

	DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Render Forward");

	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	// The back buffer is at output resolution, while the G-buffer depth surface
	// may be at DLSS render resolution. Post_Process_Manager::begin binds the
	// matching render-resolution color/depth targets immediately afterwards.
	immediate_context->OMSetRenderTargets(1, Graphics_Core::instance().get_render_target_view().GetAddressOf(), nullptr);

	Render_State::instance().set_2d_render_states(immediate_context);
	Graphics_Core::instance().clear(Color_Utils::from_hex("#291F32"));

	Graphics_Core::instance().post_procss.begin();
	Render_State::instance().set_3d_render_states(immediate_context, Rasterizer_State::Cull_None_CW, Depth_State::Test_Enable_Write_Disable);

	Resource_Manager::instance().render_models(pass_mode::forward_transparency);
	render_debug(elapsedTime);
}

// 4. UI・ポストプロセス描画
void Scene_Indoor::render_UI(float elapsedTime) {
	// ポストプロセスカテゴリ (マゼンタピンク)
	TRACY_CPU_ZONE_C("Scene_Indoor::render_UI_CPU", TracyCategory::PostProcess);
	TRACY_GPU_ZONE_C("Scene_Indoor::render_UI_GPU", TracyCategory::PostProcess);

	DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Render UI");

	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	Render_State::instance().set_2d_render_states(immediate_context);

	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Post-Process End");
		Graphics_Core::instance().post_procss.end();
	}
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Post-Process Effects");
		Graphics_Core::instance().post_procss.GetVolumetricFog().set_gi_sources(
			_ddgi.get(), _voxel_gi.get());
		Graphics_Core::instance().post_procss.draw();
	}
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Post-Process Present");
		Graphics_Core::instance().post_procss.render();
	}
	if (_voxel_gi) _voxel_gi->render_debug_3d(immediate_context);
	if (_ddgi) _ddgi->render_debug(immediate_context);

	// テキスト等のUI描画はUIカテゴリに変更してプロファイリング
	{
		TRACY_CPU_ZONE_C("Text_Draw_CPU", TracyCategory::UI);
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Text Draw");
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
	Graphics_Core::instance().get_area_light_manager().debug_render(Graphics_Core::instance().get_device_context());
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
	if (_voxel_gi) _voxel_gi->draw_gui();
	if (_ddgi) _ddgi->draw_gui();
	draw_reflection_probe_gui();
}

bool Scene_Indoor::initialize_reflection_probes(ID3D11Device* device)
{
	constexpr UINT size = 128;
	const DirectX::XMFLOAT4 positions[ReflectionProbeCount] = {
		{ -30.0f, 3.0f, 0.0f, 1.0f },
		{ 0.0f, 3.0f, 0.0f, 1.0f },
		{ 27.0f, 3.0f, 0.0f, 1.0f }
	};
	const DirectX::XMFLOAT4 minima[ReflectionProbeCount] = {
		{ -47.0f, -0.5f, -16.0f, 0.0f },
		{ -15.0f, -0.5f, -16.0f, 0.0f },
		{ 18.0f, -0.5f, -16.0f, 0.0f }
	};
	const DirectX::XMFLOAT4 maxima[ReflectionProbeCount] = {
		{ -15.0f, 13.0f, 16.0f, 0.0f },
		{ 18.0f, 13.0f, 16.0f, 0.0f },
		{ 36.0f, 13.0f, 16.0f, 0.0f }
	};
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = desc.Height = size;
	desc.MipLevels = 0;
	desc.ArraySize = 6;
	desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE | D3D11_RESOURCE_MISC_GENERATE_MIPS;
	for (unsigned int i = 0; i < ReflectionProbeCount; ++i)
	{
		auto& probe = _reflection_probes[i];
		probe.position = positions[i];
		probe.minimum = minima[i];
		probe.maximum = maxima[i];
		if (FAILED(device->CreateTexture2D(&desc, nullptr, probe.texture.GetAddressOf()))) return false;
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
		srv_desc.Format = desc.Format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		srv_desc.TextureCube.MipLevels = UINT(-1);
		if (FAILED(device->CreateShaderResourceView(probe.texture.Get(), &srv_desc,
			probe.srv.GetAddressOf()))) return false;
		for (UINT face = 0; face < 6; ++face)
		{
			D3D11_RENDER_TARGET_VIEW_DESC rtv_desc{};
			rtv_desc.Format = desc.Format;
			rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			rtv_desc.Texture2DArray.ArraySize = 1;
			rtv_desc.Texture2DArray.FirstArraySlice = face;
			if (FAILED(device->CreateRenderTargetView(probe.texture.Get(), &rtv_desc,
				probe.faces[face].GetAddressOf()))) return false;
		}
	}
	D3D11_TEXTURE2D_DESC depth_desc{};
	depth_desc.Width = depth_desc.Height = size;
	depth_desc.MipLevels = depth_desc.ArraySize = 1;
	depth_desc.Format = DXGI_FORMAT_D32_FLOAT;
	depth_desc.SampleDesc.Count = 1;
	depth_desc.Usage = D3D11_USAGE_DEFAULT;
	depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> depth;
	if (FAILED(device->CreateTexture2D(&depth_desc, nullptr, depth.GetAddressOf()))) return false;
	if (FAILED(device->CreateDepthStencilView(depth.Get(), nullptr,
		_reflection_probe_depth.GetAddressOf()))) return false;
	D3D11_BUFFER_DESC cb_desc{};
	cb_desc.ByteWidth = sizeof(ReflectionProbeConstants);
	cb_desc.Usage = D3D11_USAGE_DEFAULT;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	return SUCCEEDED(device->CreateBuffer(&cb_desc, nullptr,
		_reflection_probe_constants.GetAddressOf()));
}

void Scene_Indoor::capture_reflection_probe(ID3D11DeviceContext* context)
{
	if (!_reflection_probes_available || !_reflection_probes_enabled ||
		!_reflection_probe_constants) return;
	Gltf_Model* model = Resource_Manager::instance().get_model("Indoor");
	if (!model) return;
	for (auto& probe : _reflection_probes)
	{
		if (!probe.dirty) continue;
		if (probe.next_face == 0) probe.ready = false;
		ID3D11ShaderResourceView* null_srvs[ReflectionProbeCount]{};
		context->PSSetShaderResources(36, ReflectionProbeCount, null_srvs);
		ReflectionProbeConstants disabled{};
		context->UpdateSubresource(_reflection_probe_constants.Get(), 0, nullptr, &disabled, 0, 0);
		ID3D11Buffer* probe_cb = _reflection_probe_constants.Get();
		context->PSSetConstantBuffers(10, 1, &probe_cb);

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> old_rtv;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> old_dsv;
		context->OMGetRenderTargets(1, old_rtv.GetAddressOf(), old_dsv.GetAddressOf());
		UINT viewport_count = 1;
		D3D11_VIEWPORT old_viewport{};
		context->RSGetViewports(&viewport_count, &old_viewport);
		D3D11_VIEWPORT viewport{ 0, 0, 128.0f, 128.0f, 0, 1 };
		context->RSSetViewports(1, &viewport);
		ID3D11Buffer* camera_cb = Graphics_Core::instance().get_constant_buffer(
			Graphics_Core::Conastant_Buffer_Type::Camera);
		const DirectX::XMVECTOR eye = DirectX::XMLoadFloat4(&probe.position);
		const DirectX::XMVECTOR directions[6] = {
			DirectX::XMVectorSet(1, 0, 0, 0), DirectX::XMVectorSet(-1, 0, 0, 0),
			DirectX::XMVectorSet(0, 1, 0, 0), DirectX::XMVectorSet(0, -1, 0, 0),
			DirectX::XMVectorSet(0, 0, 1, 0), DirectX::XMVectorSet(0, 0, -1, 0)
		};
		const DirectX::XMVECTOR up[6] = {
			DirectX::XMVectorSet(0, 1, 0, 0), DirectX::XMVectorSet(0, 1, 0, 0),
			DirectX::XMVectorSet(0, 0, -1, 0), DirectX::XMVectorSet(0, 0, 1, 0),
			DirectX::XMVectorSet(0, 1, 0, 0), DirectX::XMVectorSet(0, 1, 0, 0)
		};
		const DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(
			DirectX::XM_PIDIV2, 1.0f, 0.05f, 120.0f);
		const float clear_color[4] = { 0.025f, 0.03f, 0.04f, 1.0f };
		for (UINT face = probe.next_face; face < probe.next_face + 1; ++face)
		{
			ID3D11RenderTargetView* rtv = probe.faces[face].Get();
			context->OMSetRenderTargets(1, &rtv, _reflection_probe_depth.Get());
			context->ClearRenderTargetView(rtv, clear_color);
			context->ClearDepthStencilView(_reflection_probe_depth.Get(),
				D3D11_CLEAR_DEPTH, 1.0f, 0);
			const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(
				eye, DirectX::XMVectorAdd(eye, directions[face]), up[face]);
			const DirectX::XMMATRIX vp = view * projection;
			Camera_Constants camera{};
			camera.camera_position = probe.position;
			DirectX::XMStoreFloat4x4(&camera.view, view);
			DirectX::XMStoreFloat4x4(&camera.projection, projection);
			DirectX::XMStoreFloat4x4(&camera.view_projection, vp);
			camera.previous_view_projection = camera.view_projection;
			DirectX::XMStoreFloat4x4(&camera.inv_view, DirectX::XMMatrixInverse(nullptr, view));
			DirectX::XMStoreFloat4x4(&camera.inv_projection, DirectX::XMMatrixInverse(nullptr, projection));
			DirectX::XMStoreFloat4x4(&camera.inv_view_projection, DirectX::XMMatrixInverse(nullptr, vp));
			camera.light_view_projection = Graphics_Core::instance().post_procss.GetShadow().light_view_projection;
			camera.near_z = 0.05f;
			camera.far_z = 120.0f;
			context->UpdateSubresource(camera_cb, 0, nullptr, &camera, 0, 0);
			context->VSSetConstantBuffers(1, 1, &camera_cb);
			context->PSSetConstantBuffers(1, 1, &camera_cb);
			Render_State::instance().set_render_states(context,
				Depth_State::Test_Enable_Write_Enable, Blend_State::Opaque,
				Rasterizer_State::Cull_None_CCW);
			model->render(context, _indoor_world, pass_mode::forward_opaque,
				false, 0.0f, Gltf_Model::animation_mode::single, 0);
			Render_State::instance().set_render_states(context,
				Depth_State::Test_Enable_Write_Disable, Blend_State::Alpha,
				Rasterizer_State::Cull_None_CCW);
			model->render(context, _indoor_world, pass_mode::forward_transparency,
				false, 0.0f, Gltf_Model::animation_mode::single, 0);
		}
		context->OMSetRenderTargets(0, nullptr, nullptr);
		if (++probe.next_face == 6)
		{
			context->GenerateMips(probe.srv.Get());
			probe.next_face = 0;
			probe.ready = true;
			probe.dirty = false;
		}
		context->OMSetRenderTargets(1, old_rtv.GetAddressOf(), old_dsv.Get());
		if (viewport_count) context->RSSetViewports(1, &old_viewport);
		Graphics_Core::instance().restore_camera_constants();
		break; // Capture one cube face per frame to avoid a frame-time spike.
	}
}

void Scene_Indoor::bind_reflection_probes(ID3D11DeviceContext* context)
{
	ReflectionProbeConstants data{};
	ID3D11ShaderResourceView* srvs[ReflectionProbeCount]{};
	for (unsigned int i = 0; i < ReflectionProbeCount; ++i)
	{
		const auto& probe = _reflection_probes[i];
		if (!_reflection_probes_available || !_reflection_probes_enabled || !probe.ready) break;
		data.positions[i] = probe.position;
		data.minima[i] = probe.minimum;
		data.maxima[i] = probe.maximum;
		srvs[i] = probe.srv.Get();
		data.settings.x += 1.0f;
	}
	data.settings.y = _reflection_probe_edge_fade;
	data.settings.z = _reflection_probe_strength;
	ID3D11Buffer* cb = _reflection_probe_constants.Get();
	if (cb) context->UpdateSubresource(cb, 0, nullptr, &data, 0, 0);
	context->PSSetConstantBuffers(10, 1, &cb);
	context->PSSetShaderResources(36, ReflectionProbeCount, srvs);
}

void Scene_Indoor::draw_reflection_probe_gui()
{
	if (!ImGui::CollapsingHeader("Reflection Probes")) return;
	if (!_reflection_probes_available)
	{
		ImGui::TextDisabled("Probe allocation failed");
		return;
	}
	ImGui::Checkbox("Enabled##ReflectionProbes", &_reflection_probes_enabled);
	ImGui::DragFloat("Edge fade (m)##ReflectionProbes", &_reflection_probe_edge_fade,
		0.1f, 0.1f, 10.0f);
	ImGui::SliderFloat("Strength##ReflectionProbes", &_reflection_probe_strength,
		0.0f, 1.0f);
	if (ImGui::Button("Recapture all##ReflectionProbes"))
		for (auto& probe : _reflection_probes)
		{
			probe.dirty = true;
			probe.next_face = 0;
		}
	for (unsigned int i = 0; i < ReflectionProbeCount; ++i)
	{
		auto& probe = _reflection_probes[i];
		ImGui::PushID(static_cast<int>(i));
		ImGui::Separator();
		ImGui::Text("Probe %u: %s (%u/6 faces)", i + 1,
			probe.dirty ? "capture pending" : "ready", probe.dirty ? probe.next_face : 6);
		if (ImGui::DragFloat3("Position", &probe.position.x, 0.1f))
		{
			probe.dirty = true;
			probe.next_face = 0;
		}
		ImGui::DragFloat3("Box min", &probe.minimum.x, 0.1f);
		ImGui::DragFloat3("Box max", &probe.maximum.x, 0.1f);
		if (ImGui::Button("Recapture"))
		{
			probe.dirty = true;
			probe.next_face = 0;
		}
		ImGui::PopID();
	}
}
