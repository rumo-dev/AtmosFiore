#include "Model_Manager.h"
#include <algorithm>
#include <cstring>
#include "Engine/system/graphics_core.h"
#include "Engine/System/render_state.h"
#include "Engine/System/Manager/resource_manager.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"
#include "Engine/Utilities/math_util.h"
#include "Engine/System/job_system.h"
#include "Game/World/camera/camera_manager.h"
#include "Game/World/camera/spectator_camera.h"
#include "Game/Effect/shadow/shadow.h"

// ===== Models(Model Cache) Management =====

Gltf_Model* ModelManager::load(
	const std::string& key,
	const std::string& filepath)
{
	// Check if model already loaded
	auto it = model_cache_.find(key);
	if (it != model_cache_.end())
		return it->second.get();

	auto model = std::make_unique<Gltf_Model>(Graphics_Core::instance().get_device(), filepath);
	Gltf_Model* raw = model.get();
	model_cache_.emplace(key, std::move(model));
	return raw;
}

Gltf_Model* ModelManager::get_model(const std::string& key)
{
	auto it = model_cache_.find(key);
	return (it != model_cache_.end()) ? it->second.get() : nullptr;
}

void ModelManager::unload(const std::string& key)
{
	model_cache_.erase(key);
}

void ModelManager::unload_all()
{
	model_cache_.clear();
}

size_t ModelManager::unload_unused()
{
	size_t removed = 0;
	for (auto model = model_cache_.begin(); model != model_cache_.end();)
	{
		const bool referenced = std::any_of(instances_.begin(), instances_.end(),
			[&model](const auto& instance) { return instance.second.model_key == model->first; });
		if (!referenced)
		{
			model = model_cache_.erase(model);
			++removed;
		}
		else
		{
			++model;
		}
	}
	return removed;
}

// ===== Instance Management =====

void ModelManager::add_instance(const std::string& key, ModelInstance instance)
{
	//登録されたモデルの情報を全表示
	//struct ModelInstance
	//{
	//	std::string model_key;
	//	DirectX::XMFLOAT4X4 world_transform;
	//	bool visible = true;

	//	// アニメーション共通
	//	bool is_animation = false;
	//	float animation_time = 0.0f;
	//	float animation_speed = 1.0f;
	//	bool loop_animation = true;

	//	// モード
	//	Gltf_Model::animation_mode anim_mode = Gltf_Model::animation_mode::single;

	//	// single モード用
	//	int animation_index = 0;
	//};

	log_printf("Adding instance with key: %s\n", LogLevel::Info, key.c_str());
	log_printf("  Model Key: %s\n", LogLevel::Info, instance.model_key.c_str());
	log_printf("  World Transform: [%.2f, %.2f, %.2f, %.2f]\n", LogLevel::Info,
		instance.world_transform._11, instance.world_transform._12, instance.world_transform._13, instance.world_transform._14);
	log_printf("  Visible: %s\n", LogLevel::Info, instance.visible ? "true" : "false");
	log_printf("  Is Animation: %s\n", LogLevel::Info, instance.is_animation ? "true" : "false");
	log_printf("  Animation Time: %f\n", LogLevel::Info, instance.animation_time);
	log_printf("  Animation Speed: %f\n", LogLevel::Info, instance.animation_speed);
	log_printf("  Loop Animation: %s\n", LogLevel::Info, instance.loop_animation ? "true" : "false");
	log_printf("  Animation Mode: %s\n", LogLevel::Info,
		(instance.anim_mode == Gltf_Model::animation_mode::single) ? "single" : "all");
	log_printf("  Animation Index: %d\n", LogLevel::Info, instance.animation_index);

	if (get_model(instance.model_key) == nullptr) {
		// ここが呼ばれるなら、モデルのロード漏れ
		// c_str() を使わず、std::string 同士として結合してから c_str() を呼ぶ
		log_printf("%s model not loaded!\n", LogLevel::Error, instance.model_key.c_str());
		instance.model_key = "Error";
	}
	else {

		log_printf("%s model loaded successfully\n", LogLevel::Success, instance.model_key.c_str());
	}

	instances_.emplace(key, std::move(instance));
}

ModelInstance* ModelManager::get_instance(const std::string& key)
{
	auto it = instances_.find(key);
	return (it != instances_.end()) ? &it->second : nullptr;
}

void ModelManager::remove_instance(const std::string& key)
{
	instances_.erase(key);
}

void ModelManager::remove_all_instances()
{
	instances_.clear();
}

// ===== Update/Render =====

void ModelManager::update(float delta_time)
{
	struct AnimationWork
	{
		Gltf_Model* model;
		ModelInstance* instance;
	};
	std::vector<AnimationWork> animation_work;
	animation_work.reserve(instances_.size());

	for (auto& [key, instance] : instances_)
	{
		Gltf_Model* model = get_model(instance.model_key);
		if (!instance.is_animation || !model || model->animations.empty())
		{
			instance.cached_animated_nodes.clear();
			instance.cached_joint_matrices.clear();
			continue;
		}
		animation_work.push_back({ model, &instance });
	}

	// Model animation data is read-only here and each job writes to a distinct
	// instance. Keep small scenes serial to avoid scheduling overhead.
	auto update_animation = [&animation_work, delta_time](size_t work_index)
	{
		AnimationWork& work = animation_work[work_index];
		Gltf_Model& model = *work.model;
		ModelInstance& inst = *work.instance;
		float duration = 0.0f;

		if (inst.anim_mode == Gltf_Model::animation_mode::single)
		{
			if (inst.animation_index < 0 ||
				inst.animation_index >= static_cast<int>(model.animations.size()))
			{
				inst.cached_animated_nodes.clear();
				inst.cached_joint_matrices.clear();
				return;
			}
			duration = model.animations[inst.animation_index].duration;
		}
		else
		{
			for (const auto& animation : model.animations)
				duration = std::max(duration, animation.duration);
		}

		if (duration <= 0.0f)
		{
			inst.cached_animated_nodes.clear();
			inst.cached_joint_matrices.clear();
			return;
		}

		inst.animation_time += delta_time * inst.animation_speed;
		if (inst.loop_animation)
			inst.animation_time = std::fmod(inst.animation_time, duration);
		else
			inst.animation_time = std::min(inst.animation_time, duration);

		inst.cached_animated_nodes = model.nodes;
		if (inst.anim_mode == Gltf_Model::animation_mode::single)
			model.animate(static_cast<size_t>(inst.animation_index), inst.animation_time, inst.cached_animated_nodes);
		else
			model.animate_all(inst.animation_time, inst.cached_animated_nodes);

		model.compute_joint_matrices(inst.cached_animated_nodes, inst.cached_joint_matrices);
	};

	if (animation_work.size() >= 4)
		JobSystem::instance().parallel_for(animation_work.size(), 4, update_animation);
	else
		for (size_t index = 0; index < animation_work.size(); ++index)
			update_animation(index);
}

void ModelManager::begin_profile(ID3D11DeviceContext* context)
{
	if (!context || !profile_enabled_) return;
	for (auto& timer : gpu_timers_)
	{
		if (!timer.pending) continue;
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT clock{};
		UINT64 begin = 0, end = 0;
		if (context->GetData(timer.disjoint.Get(), &clock, sizeof(clock), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
			context->GetData(timer.begin.Get(), &begin, sizeof(begin), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
			context->GetData(timer.end.Get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
			continue;
		timer.pending = false;
		if (timer.generation != profile_generation_ || clock.Disjoint || !clock.Frequency || end < begin)
			continue;
		const double gpu_ms = static_cast<double>(end - begin) * 1000.0 /
			static_cast<double>(clock.Frequency);
		auto& average = performance_[timer.occlusion_enabled ? 1 : 0];
		average.gpu_ms = average.gpu_samples ? average.gpu_ms * 0.9 + gpu_ms * 0.1 : gpu_ms;
		++average.gpu_samples;
	}
	profile_active_ = true;
	profile_occlusion_enabled_ = occlusion_culling_enabled_;
	active_gpu_timer_ = -1;
	cpu_profile_begin_ = std::chrono::steady_clock::now();
	for (size_t i = 0; i < gpu_timers_.size(); ++i)
	{
		auto& timer = gpu_timers_[i];
		if (timer.pending) continue;
		D3D11_QUERY_DESC desc{};
		if (!timer.disjoint)
		{
			desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
			if (FAILED(Graphics_Core::instance().get_device()->CreateQuery(&desc, timer.disjoint.GetAddressOf())))
				continue;
		}
		desc.Query = D3D11_QUERY_TIMESTAMP;
		if (!timer.begin && FAILED(Graphics_Core::instance().get_device()->CreateQuery(&desc, timer.begin.GetAddressOf())))
			continue;
		if (!timer.end && FAILED(Graphics_Core::instance().get_device()->CreateQuery(&desc, timer.end.GetAddressOf())))
			continue;
		context->Begin(timer.disjoint.Get());
		context->End(timer.begin.Get());
		active_gpu_timer_ = static_cast<int>(i);
		break;
	}
}

void ModelManager::end_profile(ID3D11DeviceContext* context)
{
	if (!profile_active_) return;
	const double cpu_ms = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - cpu_profile_begin_).count();
	auto& average = performance_[profile_occlusion_enabled_ ? 1 : 0];
	average.queries_per_frame = average.cpu_samples ? average.queries_per_frame * 0.9 +
		static_cast<double>(last_occlusion_queries_issued_) * 0.1 :
		static_cast<double>(last_occlusion_queries_issued_);
	average.cpu_ms = average.cpu_samples ? average.cpu_ms * 0.9 + cpu_ms * 0.1 : cpu_ms;
	++average.cpu_samples;
	average.skipped_triangles = camera_meshlet_stats_.skipped_triangles +
		camera_meshlet_stats_.occlusion_skipped_triangles;
	if (context && active_gpu_timer_ >= 0)
	{
		auto& timer = gpu_timers_[active_gpu_timer_];
		context->End(timer.end.Get());
		context->End(timer.disjoint.Get());
		timer.pending = true;
		timer.occlusion_enabled = profile_occlusion_enabled_;
		timer.generation = profile_generation_;
	}
	active_gpu_timer_ = -1;
	profile_active_ = false;
}

void ModelManager::render_all(pass_mode pass)
{
	// Prepare frustum from active camera
	if (pass == pass_mode::deferred_geometry) camera_meshlet_stats_ = {};
	if (pass == pass_mode::directional_shadow) directional_meshlet_stats_ = {};
	Gltf_Model::meshlet_culling_stats* meshlet_stats = nullptr;
	if (pass == pass_mode::deferred_geometry || pass == pass_mode::forward_opaque ||
		pass == pass_mode::forward_transparency)
		meshlet_stats = &camera_meshlet_stats_;
	else if (pass == pass_mode::directional_shadow)
		meshlet_stats = &directional_meshlet_stats_;

	auto active_cam = CameraManager::instance().get_active_camera();
	if (pass == pass_mode::deferred_geometry)
	{
		++frame_index_;
		if (active_cam)
		{
			DirectX::XMFLOAT4X4 current;
			const Camera& camera = active_cam->get_camera();
			DirectX::XMStoreFloat4x4(&current, camera.view * camera.projection);
			if (have_previous_camera_view_projection_ &&
				std::memcmp(&current, &previous_camera_view_projection_, sizeof(current)) == 0)
				++stationary_camera_frames_;
			else
			{
				stationary_camera_frames_ = 0;
				previous_camera_view_projection_ = current;
				have_previous_camera_view_projection_ = true;
				for (auto& [key, instance] : instances_)
				{
					if (instance.occlusion.valid)
					{
						instance.occlusion.valid = false;
						++instance.occlusion.generation;
					}
				}
			}
		}
		else stationary_camera_frames_ = 0;
	}
	Frustum cam_frustum{};
	bool have_frustum = false;
	if (active_cam) {
		// Only use camera frustum for visible scene passes
		if (pass == pass_mode::deferred_geometry ||
			pass == pass_mode::forward_opaque ||
			pass == pass_mode::forward_transparency) {
			const Camera& cam = active_cam->get_camera();
			cam_frustum = ExtractFrustum(cam.view, cam.projection);
			if (camera_culling_frozen_) cam_frustum = frozen_camera_frustum_;
			have_frustum = true;
		}
	}
	// Additionally, for directional shadow pass, use the light's view-projection for culling if available
	if (pass == pass_mode::directional_shadow) {
		auto& shadower = Graphics_Core::instance().post_procss.GetShadow();
		DirectX::XMFLOAT4X4 lightVP = shadower.get_light_view_projection();
		DirectX::XMMATRIX m = DirectX::XMLoadFloat4x4(&lightVP);
		cam_frustum = ExtractFrustumFromMatrix(m);
		have_frustum = true;
	}
	// Point shadow: use per-face view-projection matrices
	bool point_shadow_pass = (pass == pass_mode::shadow);
	shadow* shadower_ptr = nullptr;
	if (point_shadow_pass) {
		shadower_ptr = &Graphics_Core::instance().post_procss.GetShadow();
	}
	int culled = 0;
	int total = 0;
	int point_face_culled[6] = { 0,0,0,0,0,0 };
	int point_face_total[6] = { 0,0,0,0,0,0 };
	for (auto& [key, inst] : instances_)
	{
		++total;
		if (!inst.visible) continue;
		if (pass == pass_mode::deferred_geometry && inst.occlusion.valid &&
			std::memcmp(&inst.occlusion.world, &inst.world_transform, sizeof(inst.world_transform)) != 0)
		{
			inst.occlusion.valid = false;
			++inst.occlusion.generation;
		}

		Gltf_Model* model = get_model(inst.model_key);
		if (!model)
		{
			continue;
		}

		// ===== Frustum Culling =====
		bool is_visible = true;
		DirectX::XMFLOAT3 aabb_min, aabb_max;
		DirectX::XMVECTOR box_min, box_max;
		bool aabb_computed = false;

		// Compute AABB once if needed
		if (!inst.disable_frustum_culling && model->has_bbox &&
			(have_frustum || point_shadow_pass)) {
			model->compute_instance_aabb(
				inst.world_transform,
				inst.is_animation,
				inst.animation_time,
				inst.anim_mode,
				inst.animation_index,
				aabb_min,
				aabb_max);
			box_min = DirectX::XMLoadFloat3(&aabb_min);
			box_max = DirectX::XMLoadFloat3(&aabb_max);
			aabb_computed = true;
		}

		// For camera-based passes, use camera frustum
		if (have_frustum && aabb_computed) {
			if (!AABBIntersectsFrustum(cam_frustum, box_min, box_max)) {
				is_visible = false;
				++culled;
			}
		}

		// For point shadow, test against each of the 6 face frustums
		if (point_shadow_pass && shadower_ptr && aabb_computed && is_visible) {
			for (int face = 0; face < 6; ++face) {
				DirectX::XMFLOAT4X4 face_vp = shadower_ptr->get_point_face_viewproj(face);
				DirectX::XMMATRIX m = DirectX::XMLoadFloat4x4(&face_vp);
				Frustum face_frustum = ExtractFrustumFromMatrix(m);

				++point_face_total[face];

				if (!AABBIntersectsFrustum(face_frustum, box_min, box_max)) {
					++point_face_culled[face];
				}
			}
		}

		if (!is_visible) {
			continue;
		}
		const Frustum* meshlet_frustum = meshlet_culling_enabled_ && have_frustum && !inst.disable_frustum_culling
			? &cam_frustum : nullptr;
		bool use_occlusion = false;
		if (occlusion_culling_enabled_ && meshlet_culling_enabled_ && !camera_culling_frozen_ &&
			pass == pass_mode::deferred_geometry && !inst.is_animation && !inst.disable_frustum_culling &&
			inst.cached_animated_nodes.empty() && inst.occlusion.valid &&
			inst.occlusion.model == model && active_cam)
		{
			DirectX::XMFLOAT4X4 camera_view_projection;
			const Camera& camera = active_cam->get_camera();
			DirectX::XMStoreFloat4x4(&camera_view_projection, camera.view * camera.projection);
			use_occlusion = std::memcmp(&inst.occlusion.view_projection, &camera_view_projection,
				sizeof(camera_view_projection)) == 0 &&
				std::memcmp(&inst.occlusion.world, &inst.world_transform, sizeof(inst.world_transform)) == 0;
		}
		{

			std::string marker_name = "DrawAllModel: " + key;

			// ワイド文字への変換バッファをスタックに用意
			wchar_t w_buffer[256];
			size_t convertedChars = 0;

			// char* から wchar_t* へ変換
			const errno_t conversion_result = mbstowcs_s(
				&convertedChars, w_buffer, _countof(w_buffer), marker_name.c_str(), _TRUNCATE);
			if (conversion_result != 0 || convertedChars == 0)
			{
				w_buffer[0] = L'\0';
			}

			// 変換したワイド文字列ポインタを渡す
			DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, w_buffer);

			if (!inst.cached_animated_nodes.empty())
			{
				model->render_with_nodes(
					Graphics_Core::instance().get_device_context(),
					inst.world_transform,
					pass,
					&inst.cached_animated_nodes,
					&inst.cached_joint_matrices,
					meshlet_frustum,
					meshlet_stats,
					&inst.occlusion,
					use_occlusion);
			}
			else
			{
				model->render(
					Graphics_Core::instance().get_device_context(),
					inst.world_transform,
					pass,
					inst.is_animation,
					inst.animation_time,
					inst.anim_mode,
					inst.animation_index,
					meshlet_frustum,
					meshlet_stats,
					&inst.occlusion,
					use_occlusion);
			}
		}
	}

	// store stats
	this->last_culled_count = culled;
	this->last_total_count = total;
	for (int i = 0; i < 6; ++i) {
		this->last_point_face_culled[i] = point_face_culled[i];
		this->last_point_face_total[i] = point_face_total[i];
	}
}

void ModelManager::issue_occlusion_queries(ID3D11DeviceContext* context)
{
	last_occlusion_queries_issued_ = 0;
	if (!context || !occlusion_culling_enabled_ || !meshlet_culling_enabled_ || camera_culling_frozen_)
		return;
	if (stationary_camera_frames_ < 2 ||
		(last_query_frame_ && frame_index_ - last_query_frame_ < 4)) return;
	last_query_frame_ = frame_index_;
	const auto active_camera = CameraManager::instance().get_active_camera();
	if (!active_camera) return;
	ID3D11VertexShader* box_shader = Resource_Manager::instance().get_native_shader<Vertex_Shader>("OCCLUSION_BOX_VS");
	if (!box_shader) return;
	if (!occlusion_box_cbuffer_)
	{
		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = sizeof(DirectX::XMFLOAT4X4);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		if (FAILED(Graphics_Core::instance().get_device()->CreateBuffer(&desc, nullptr,
			occlusion_box_cbuffer_.GetAddressOf()))) return;
	}

	Microsoft::WRL::ComPtr<ID3D11VertexShader> previous_vs;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> previous_ps;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> previous_layout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> previous_cb;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> previous_depth;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> previous_rasterizer;
	Microsoft::WRL::ComPtr<ID3D11Predicate> previous_predicate;
	UINT previous_stencil_ref = 0;
	BOOL previous_predicate_value = FALSE;
	D3D11_PRIMITIVE_TOPOLOGY previous_topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	context->VSGetShader(previous_vs.GetAddressOf(), nullptr, nullptr);
	context->PSGetShader(previous_ps.GetAddressOf(), nullptr, nullptr);
	context->IAGetInputLayout(previous_layout.GetAddressOf());
	context->IAGetPrimitiveTopology(&previous_topology);
	context->VSGetConstantBuffers(0, 1, previous_cb.GetAddressOf());
	context->OMGetDepthStencilState(previous_depth.GetAddressOf(), &previous_stencil_ref);
	context->RSGetState(previous_rasterizer.GetAddressOf());
	context->GetPredication(previous_predicate.GetAddressOf(), &previous_predicate_value);

	context->SetPredication(nullptr, FALSE);
	context->IASetInputLayout(nullptr);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetShader(box_shader, nullptr, 0);
	context->PSSetShader(nullptr, nullptr, 0);
	context->VSSetConstantBuffers(0, 1, occlusion_box_cbuffer_.GetAddressOf());
	Render_State::instance().set_rasterizer_state(context, Rasterizer_State::Cull_None_CCW);
	const Camera& camera = active_camera->get_camera();
	Render_State::instance().set_depth_stencil_state(context,
		camera.isReversed_Z ? Depth_State::Reversed_Z_Test_Enable_Write_Disable :
		Depth_State::Test_Enable_Write_Disable);
	const DirectX::XMMATRIX view_projection = camera.view * camera.projection;
	const Frustum frustum = ExtractFrustum(camera.view, camera.projection);
	std::vector<std::pair<Gltf_Model*, ModelInstance*>> candidates;
	for (auto& [key, instance] : instances_)
	{
		if (!instance.visible || instance.is_animation || instance.disable_frustum_culling ||
			!instance.cached_animated_nodes.empty()) continue;
		Gltf_Model* model = get_model(instance.model_key);
		if (model) candidates.emplace_back(model, &instance);
	}
	if (!candidates.empty())
	{
		UINT remaining_budget = 128;
		const size_t start = query_instance_cursor_ % candidates.size();
		query_instance_cursor_ = (start + 1) % candidates.size();
		for (size_t i = 0; i < candidates.size() && remaining_budget; ++i)
		{
			auto& [model, instance] = candidates[(start + i) % candidates.size()];
			const UINT quota = std::min(remaining_budget, 32u);
			const UINT issued = model->issue_occlusion_queries(context, instance->world_transform,
				view_projection, camera.position, frustum, instance->occlusion,
				occlusion_box_cbuffer_.Get(), quota);
			last_occlusion_queries_issued_ += issued;
			remaining_budget -= issued;
		}
	}

	context->SetPredication(previous_predicate.Get(), previous_predicate_value);
	context->VSSetShader(previous_vs.Get(), nullptr, 0);
	context->PSSetShader(previous_ps.Get(), nullptr, 0);
	context->IASetInputLayout(previous_layout.Get());
	context->IASetPrimitiveTopology(previous_topology);
	context->VSSetConstantBuffers(0, 1, previous_cb.GetAddressOf());
	context->OMSetDepthStencilState(previous_depth.Get(), previous_stencil_ref);
	context->RSSetState(previous_rasterizer.Get());
}

void ModelManager::render(const std::string& key, pass_mode pass)
{
	auto it = instances_.find(key);
	if (it == instances_.end() || !it->second.visible) return;

	const ModelInstance& inst = it->second;
	Gltf_Model* model = get_model(inst.model_key);
	if (!model) return;

	model->render(
		Graphics_Core::instance().get_device_context(),
		inst.world_transform,
		pass,
		inst.is_animation,
		inst.animation_time,
		inst.anim_mode,
		inst.animation_index);
}
void ModelManager::draw_imgui()
{
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("Meshlet Debug", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Button("Activate Meshlet Debug Camera"))
		{
			auto& cameras = CameraManager::instance();
			const Camera& source = cameras.get_active_camera()->get_camera();
			auto debug_camera = std::make_shared<SpectatorCamera>(Graphics_Core::instance().get_window_handle());
			debug_camera->get_camera() = source;
			const DirectX::XMVECTOR direction = DirectX::XMVectorSubtract(source.target, source.position);
			const float x = DirectX::XMVectorGetX(direction);
			const float y = DirectX::XMVectorGetY(direction);
			const float z = DirectX::XMVectorGetZ(direction);
			const float yaw = DirectX::XMConvertToDegrees(atan2f(z, x));
			const float pitch = std::clamp(DirectX::XMConvertToDegrees(atan2f(y, sqrtf(x * x + z * z))),
				-89.0f, 89.0f);
			debug_camera->set_rotation(yaw, pitch, source.position, source.target);
			cameras.register_camera("Meshlet Debug", debug_camera);
			cameras.switch_camera("Meshlet Debug");
		}
		ImGui::TextDisabled("Move: WASD / Q,E   Look: right mouse drag   Fast: Shift");
		ImGui::Checkbox("Enable Meshlet Culling", &meshlet_culling_enabled_);
		ImGui::Checkbox("Enable Occlusion Culling", &occlusion_culling_enabled_);
		ImGui::Checkbox("Measure A/B Timings", &profile_enabled_);
		if (ImGui::Button("Reset A/B Timings"))
		{
			performance_ = {};
			++profile_generation_;
		}
		for (int mode = 0; mode < 2; ++mode)
		{
			const auto& p = performance_[mode];
			if (!p.cpu_samples) continue;
			ImGui::Text("Occlusion %s: CPU %.2f ms (%llu frames)", mode ? "ON" : "OFF",
				p.cpu_ms, static_cast<unsigned long long>(p.cpu_samples));
			if (p.gpu_samples)
				ImGui::Text("  GPU %.2f ms (%llu samples)", p.gpu_ms,
					static_cast<unsigned long long>(p.gpu_samples));
			else ImGui::TextDisabled("  GPU timing pending");
			ImGui::Text("  %.1f box queries/frame / %llu triangles skipped (latest)", p.queries_per_frame,
				static_cast<unsigned long long>(p.skipped_triangles));
		}
		ImGui::TextDisabled("Timings cover deferred model drawing and box queries.");
		ImGui::TextDisabled("Compare ON/OFF while the camera and scene stay still.");
		if (ImGui::Button(camera_culling_frozen_ ? "Release Camera Culling Frustum" : "Freeze Camera Culling Frustum"))
		{
			camera_culling_frozen_ = !camera_culling_frozen_;
			if (camera_culling_frozen_)
			{
				const Camera& camera = CameraManager::instance().get_active_camera()->get_camera();
				frozen_camera_frustum_ = ExtractFrustum(camera.view, camera.projection);
			}
		}
		ImGui::Text("Camera frustum: %s", camera_culling_frozen_ ? "frozen" : "live");
		ImGui::Text("Camera: %llu tested / %llu culled",
			static_cast<unsigned long long>(camera_meshlet_stats_.tested),
			static_cast<unsigned long long>(camera_meshlet_stats_.culled));
		ImGui::Text("  %llu triangles skipped / %llu draws",
			static_cast<unsigned long long>(camera_meshlet_stats_.skipped_triangles),
			static_cast<unsigned long long>(camera_meshlet_stats_.draw_calls));
		ImGui::Text("  %llu whole draws / %llu fragmented fallbacks",
			static_cast<unsigned long long>(camera_meshlet_stats_.whole_primitive_fast_paths),
			static_cast<unsigned long long>(camera_meshlet_stats_.fragmented_fallbacks));
		ImGui::Text("Occlusion: %llu groups tested / %llu culled",
			static_cast<unsigned long long>(camera_meshlet_stats_.occlusion_tested),
			static_cast<unsigned long long>(camera_meshlet_stats_.occlusion_culled));
		ImGui::Text("  %llu triangles skipped",
			static_cast<unsigned long long>(camera_meshlet_stats_.occlusion_skipped_triangles));
		ImGui::Text("  %u box queries issued", last_occlusion_queries_issued_);
		ImGui::Text("Directional shadow: %llu tested / %llu culled",
			static_cast<unsigned long long>(directional_meshlet_stats_.tested),
			static_cast<unsigned long long>(directional_meshlet_stats_.culled));
		ImGui::Text("  %llu triangles skipped / %llu draws",
			static_cast<unsigned long long>(directional_meshlet_stats_.skipped_triangles),
			static_cast<unsigned long long>(directional_meshlet_stats_.draw_calls));
		ImGui::TextDisabled("Occlusion uses previous-frame queries for static opaque groups; moving the camera shows all groups.");
	}

	// ===== Loaded Models List =====
	if (ImGui::CollapsingHeader("Loaded Models"))
	{
		ImGui::Indent();
		for (auto& [key, model] : model_cache_)
		{
			ImGui::BulletText("%s", key.c_str());
		}
		if (model_cache_.empty())
			ImGui::TextDisabled("No models loaded.");
		ImGui::Unindent();
	}

	// ===== Instance List/Edit =====
	if (ImGui::CollapsingHeader("Instances"))
	{
		ImGui::Indent();
		for (auto& [key, inst] : instances_)
		{
			if (!ImGui::TreeNode(key.c_str())) continue;

			ImGui::TextDisabled("model_key: %s", inst.model_key.c_str());
			ImGui::Checkbox("Visible", &inst.visible);

			// ===== Animation Control =====
			if (ImGui::TreeNode("Animation"))
			{
				ImGui::Checkbox("Enable", &inst.is_animation);

				if (inst.is_animation)
				{
					Gltf_Model* model = get_model(inst.model_key);
					if (model && !model->animations.empty())
					{
						// Animation selection
						int anim_count = static_cast<int>(model->animations.size());
						ImGui::SliderInt("Index", &inst.animation_index, 0, anim_count - 1);

						float duration = model->animations[inst.animation_index].duration;
						ImGui::SliderFloat("Time", &inst.animation_time, 0.0f, duration);
						ImGui::SliderFloat("Speed", &inst.animation_speed, 0.0f, 5.0f);
						ImGui::Checkbox("Loop", &inst.loop_animation);

						// Play/Stop/Reset
						if (ImGui::Button("Play"))  inst.is_animation = true;
						ImGui::SameLine();
						if (ImGui::Button("Stop"))  inst.is_animation = false;
						ImGui::SameLine();
						if (ImGui::Button("Reset")) inst.animation_time = 0.0f;
					}
					else
					{
						ImGui::TextDisabled("No animations.");
					}
				}
				ImGui::TreePop();
			}

			// ===== Transform Edit =====
			if (ImGui::TreeNode("Transform"))
			{
				// Directly edit world_transform translation/rotation/scale
				float* m = &inst.world_transform._11;
				ImGui::DragFloat3("Translation", m + 12, 0.01f);
				float scale[3] = { m[0], m[5], m[10] };
				if (ImGui::DragFloat3("Scale", scale, 0.01f, 0.001f, 100.0f))
				{
					m[0] = scale[0];
					m[5] = scale[1];
					m[10] = scale[2];
				}
				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

		if (instances_.empty())
			ImGui::TextDisabled("No instances.");

		ImGui::Unindent();
		// Show culling stats
		ImGui::Separator();
		ImGui::Text("Culling: Culled %d / Total %d", last_culled_count, last_total_count);
		if (Graphics_Core::instance().post_procss.GetShadow().point_shadow_front)
		{
			ImGui::Text("Point Shadow Face Stats:");
			for (int i = 0; i < 6; ++i)
			{
				ImGui::Text(" Face %d: Culled %d / Total %d", i, last_point_face_culled[i], last_point_face_total[i]);
			}
		}
	}
#endif // USE_IMGUI
}
