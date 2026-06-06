#include "Model_Manager.h"
#include <algorithm>
#include "Engine/system/graphics_core.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"
#include "Engine/Utilities/math_util.h"
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
	for (auto& [key, inst] : instances_)
	{
		if (!inst.is_animation) continue;

		Gltf_Model* model = get_model(inst.model_key);
		if (!model || model->animations.empty()) continue;

		float duration = 0.0f;

		if (inst.anim_mode == Gltf_Model::animation_mode::single)
		{
			if (inst.animation_index >= static_cast<int>(model->animations.size())) continue;
			duration = model->animations[inst.animation_index].duration;
		}
		else // all
		{
			// Use max duration for all animations loop
			for (const auto& anim : model->animations)
				duration = std::max(duration, anim.duration);
		}

		inst.animation_time += delta_time * inst.animation_speed;

		if (inst.loop_animation)
			inst.animation_time = std::fmod(inst.animation_time, duration);
		else
			inst.animation_time = std::min(inst.animation_time, duration);
	}
}

void ModelManager::render_all(pass_mode pass)
{
	// Prepare frustum from active camera
	auto active_cam = Camera_Manager::instance().get_active_camera();
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

		if (!model->has_bbox) {
			continue;
		}

		// ===== Frustum Culling =====
		bool is_visible = true;
		DirectX::XMFLOAT3 aabb_min, aabb_max;
		DirectX::XMVECTOR box_min, box_max;
		bool aabb_computed = false;

		// Compute AABB once if needed
		if ((have_frustum || point_shadow_pass) && model->has_bbox) {
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

		model->render(
			Graphics_Core::instance().get_device_context(),
			inst.world_transform,
			pass,
			inst.is_animation,
			inst.animation_time,
			inst.anim_mode,
			inst.animation_index);
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
				ImGui::DragFloat3("Scale",
					new float[3] {m[0], m[5], m[10]}, 0.01f, 0.001f, 100.0f);
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
