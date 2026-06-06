
#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "camera_base.h"

/**
 * @brief カメラ管理マネージャークラス（シングルトン）
 */
class Camera_Manager {
private:
	std::unordered_map<std::string, std::shared_ptr<Camera_Base>> _cameras;
	std::shared_ptr<Camera_Base> _active_camera;

	Camera_Manager() = default;

public:
	static Camera_Manager& instance() {
		static Camera_Manager ins;
		return ins;
	}

	// カメラの登録
	void register_camera(const std::string& name, std::shared_ptr<Camera_Base> camera) {
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
	std::shared_ptr<Camera_Base> get_active_camera() {
		return _active_camera;
	}

	// 現在アクティブなカメラの更新と行列計算
	void update(float deltaTime) {
		if (_active_camera) {
			_active_camera->update(deltaTime);
			_active_camera->get_camera().identity(); // 行列の自動更新
		}
	}
	// camera_manager.h への追加・修正

public:
	/**
	 * @brief すべてのカメラの ImGui 管理UIを一括描画する
	 */
	void draw_imgui() {
		// ★ウインドウを新しく作らない（ImGui::Begin を削除）
		// これにより、呼び出し元の「Dashboard」や「Scene」といったタブの中に直接描画されます。

		// --- 1. アクティブカメラの切り替えコンボボックス ---
		ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "[ Active Camera Selection ]");

		std::string current_name = get_active_camera_name();
		if (ImGui::BeginCombo("Active Camera", current_name.c_str())) {
			for (const auto& pair : _cameras) {
				bool is_selected = (pair.first == current_name);
				if (ImGui::Selectable(pair.first.c_str(), is_selected)) {
					switch_camera(pair.first);
				}
				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();

		// --- 2. 各カメラ固有のデバッグUI呼び出し ---
		if (ImGui::BeginTabBar("CameraTabs")) {
			for (const auto& pair : _cameras) {
				if (ImGui::BeginTabItem(pair.first.c_str())) {
					if (pair.second == _active_camera) {
						ImGui::TextDisabled("( Currently Active )");
					}

					// 各カメラのUI
					pair.second->draw_imgui();

					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}

		// ★ ImGui::End(); も削除
	}
};
