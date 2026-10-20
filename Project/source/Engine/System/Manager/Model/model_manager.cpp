#include "Model_Manager.h"
#include <algorithm>
#include "Engine/system/graphics_core.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"
#include "Engine/Utilities/math_util.h"
#include "Engine/System/job_system.h"
#include "Game/World/camera/camera_manager.h"
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
void ModelManager::render_all(pass_mode pass)
{
	// Prepare frustum from active camera

	auto active_cam = CameraManager::instance().get_active_camera();
	Frustum cam_frustum{};
	bool have_frustum = false;
	if (active_cam) {
		// Only use camera frustum for visible scene passes
		if (pass == pass_mode::deferred_geometry ||
			pass == pass_mode::forward_opaque ||
			pass == pass_mode::forward_transparency) {
			const Camera& cam = active_cam->get_camera();
			cam_frustum = ExtractFrustum(cam.view, cam.projection);
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
					&inst.cached_joint_matrices);
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
					inst.animation_index);
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
