
#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "camera_base.h"
#include "Engine/Graphics/UI/DebugMenu/CustomWidgets.h"
#include "null_camera.h"

/**
 * @brief カメラ管理マネージャークラス（シングルトン）
 */
class CameraManager {
private:
	std::unordered_map<std::string, std::shared_ptr<ICamera>> _cameras;
	std::shared_ptr<ICamera> _active_camera;
	std::shared_ptr<ICamera> _dummy_camera; // Fallback camera

	CameraManager() {
		// Initialize with a dummy camera implementation if necessary
		_dummy_camera = std::make_shared<NullCamera>();
	}
	//Camera_Manager() = default;

public:
	static CameraManager& instance() {
		static CameraManager ins;
		return ins;
	}

	// カメラの登録
	void register_camera(const std::string& name, std::shared_ptr<ICamera> camera) {
		_cameras[name] = camera;
		if (!_active_camera) {
			_active_camera = camera; // 最初の一台を自動的にアクティブにする
		}
	}
	std::string get_active_camera_name() {
		if (_active_camera) {
			for (const auto& pair : _cameras) {
				if (pair.second == _active_camera) {
					return pair.first;
				}
			}
		}
		return "";
	}
	// カメラの切り替え
	bool switch_camera(const std::string& name) {
		auto it = _cameras.find(name);
		if (it != _cameras.end()) {
			_active_camera = it->second;
			return true;
		}
		return false;
	}

	// 現在アクティブなカメラの取得
	std::shared_ptr<ICamera> get_active_camera() {
		if (!_active_camera)
		{
			return _dummy_camera;
		}
		return _active_camera;
	}

	// 現在アクティブなカメラの更新と行列計算
	void update(float deltaTime, float viewport_aspect_ratio, float viewport_height) {
		if (_active_camera) {
			_active_camera->update(deltaTime);
			_active_camera->get_camera().apply_auto_settings(deltaTime);
			_active_camera->get_camera().updateShake(deltaTime);
			_active_camera->get_camera().update_derived_parameters(viewport_aspect_ratio, viewport_height);
			_active_camera->get_camera().identity(); // 行列の自動更新
			_active_camera->get_camera().UpdateAudioListener(deltaTime);
		}

	}

public:
	/**
	 * @brief すべてのカメラの ImGui 管理UIを一括描画する
	 */
	void draw_imgui() {
		// --- 1. アクティブカメラの切り替え ---
		CustomUI::SectionLabel("[ Active Camera Selection ]");

		std::string current_name = get_active_camera_name();
		const char* preview_value = current_name.c_str();

		// CustomUI::Combo を活用
		if (ImGui::BeginCombo("Active Camera", preview_value)) {
			for (const auto& pair : _cameras) {
				bool is_selected = (pair.first == current_name);
				if (ImGui::Selectable(pair.first.c_str(), is_selected)) {
					switch_camera(pair.first);
				}
			}
			ImGui::EndCombo();
		}

		CustomUI::Separator();

		// --- 2. タブによるカメラ管理 ---
		if (ImGui::BeginTabBar("CameraTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
			for (const auto& pair : _cameras) {
				if (ImGui::BeginTabItem(pair.first.c_str())) {

					if (pair.second == _active_camera) {
						ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "● Currently Active");
					}
					else {
						ImGui::TextDisabled("○ Inactive");
					}

					ImGui::Spacing();

					// 各カメラ固有のUI描画
					pair.second->draw_imgui();

					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
	}
};
