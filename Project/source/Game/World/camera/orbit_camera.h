#pragma once
#include "camera_base.h"
#include <Windows.h>
#include <DirectXMath.h>
#include <algorithm>
#include "Engine/Graphics/UI/ImGui/ImGui.h"

namespace dx = DirectX;

/**
 * @brief オービット（注視点中心・ターンテーブル）カメラクラス
 */
class Orbit_Camera : public Camera_Base {
private:
	HWND _hwnd{};
	bool _first_mouse = true;
	POINT _last_mouse_pos{};

	float _yaw = -90.0f;
	float _pitch = 30.0f;
	float _radius = 10.0f;             // 注視点からの距離（半径）

	float _mouse_sensitivity = 0.1f;
	float _zoom_speed = 5.0f;

	float _min_radius = 1.0f;
	float _max_radius = 100.0f;

	// --- [追加] 自動回転用の変数 ---
	bool  _enable_auto_rotate = false;  // 自動回転のON/OFFフラグ
	float _auto_rotate_speed = 15.0f;  // 回転速度（度/秒）

public:
	/**
	 * @param hwnd ウィンドウハンドル
	 * @param initial_target 注視する中心の初期座標
	 */
	Orbit_Camera(HWND hwnd = nullptr, const dx::XMVECTOR& initial_target = dx::XMVectorZero())
		: _hwnd(hwnd)
	{
		camera_.target = initial_target;
		camera_.position = dx::XMVectorSet(0.0f, 5.0f, -10.0f, 1.0f);
		camera_.up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	}

	void update(float deltaTime) override {
		// 1. 右クリックドラッグによるオブジェクト周囲の旋回
		if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
			POINT current_mouse_pos;
			GetCursorPos(&current_mouse_pos);
			if (_hwnd) ScreenToClient(_hwnd, &current_mouse_pos);

			if (_first_mouse) {
				_last_mouse_pos = current_mouse_pos;
				_first_mouse = false;
			}
			else {
				float offsetX = static_cast<float>(current_mouse_pos.x - _last_mouse_pos.x);
				float offsetY = static_cast<float>(_last_mouse_pos.y - current_mouse_pos.y);
				_last_mouse_pos = current_mouse_pos;

				_yaw += offsetX * _mouse_sensitivity;
				_pitch += offsetY * _mouse_sensitivity;
				_pitch = std::clamp(_pitch, -89.0f, 89.0f); // 真上・真下をまたがない制限
			}
		}
		else {
			_first_mouse = true;

			// ★[追加] マウス操作をしていない、かつ自動回転が有効な場合にヨー角を進める
			if (_enable_auto_rotate) {
				_yaw += _auto_rotate_speed * deltaTime;

				// 360度を超えたらループ（数値の肥大化防止）
				if (_yaw > 360.0f) _yaw -= 360.0f;
				if (_yaw < -360.0f) _yaw += 360.0f;
			}
		}

		// 2. キー入力によるズーム処理 (O:遠ざかる, P:近づく)
		if (GetAsyncKeyState('O') & 0x8000) _radius += _zoom_speed * deltaTime;
		if (GetAsyncKeyState('P') & 0x8000) _radius -= _zoom_speed * deltaTime;
		_radius = std::clamp(_radius, _min_radius, _max_radius); // 制限値を使用

		// 3. 計算された角度と距離から、カメラの配置座標を割り出す
		float pitch_rad = dx::XMConvertToRadians(_pitch);
		float yaw_rad = dx::XMConvertToRadians(_yaw);

		dx::XMVECTOR offset = dx::XMVectorSet(
			cosf(pitch_rad) * cosf(yaw_rad),
			sinf(pitch_rad),
			cosf(pitch_rad) * sinf(yaw_rad),
			0.0f
		);
		offset = dx::XMVectorScale(dx::XMVector3Normalize(offset), _radius);

		// 位置 = ターゲット座標 + オフセットベクトル
		camera_.position = dx::XMVectorAdd(camera_.target, offset);

		// Upベクトルの直交化
		dx::XMVECTOR world_up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		dx::XMVECTOR forward = dx::XMVector3Normalize(dx::XMVectorSubtract(camera_.target, camera_.position));
		dx::XMVECTOR right = dx::XMVector3Normalize(dx::XMVector3Cross(world_up, forward));
		camera_.up = dx::XMVector3Normalize(dx::XMVector3Cross(forward, right));
	}

	void set_target(const dx::XMVECTOR& new_target) { camera_.target = new_target; }

	/**
	 * @brief [オーバーライド] ImGui用デバッグUIの描画
	 */
	void draw_imgui() override {
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.8f, 1.0f), "--- Orbit Camera Settings ---");

		// --- [追加] 自動回転コントロールセクション ---
		ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[ Auto Rotation ]");
		ImGui::Checkbox("Enable Auto Rotate", &_enable_auto_rotate);

		// 回転速度のスライダー（マイナス値にすれば逆回転も可能です）
		ImGui::SliderFloat("Rotate Speed (deg/s)", &_auto_rotate_speed, -180.0f, 180.0f, "%.1f");

		if (ImGui::Button("Reset Rotation Speed")) {
			_auto_rotate_speed = 15.0f;
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 1. 旋回半径（注視点からの距離）の直接スライダー調整
		ImGui::SliderFloat("Radius (Distance)", &_radius, _min_radius, _max_radius, "%.2f");

		// 2. マウス感度とキーボードズーム速度の変更
		ImGui::SliderFloat("Mouse Sensitivity", &_mouse_sensitivity, 0.01f, 1.0f, "%.2f");
		ImGui::SliderFloat("Keyboard Zoom Speed", &_zoom_speed, 1.0f, 20.0f, "%.1f m/s");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 3. 注視点（ターゲット）座標の直接編集
		dx::XMFLOAT4 target_f;
		dx::XMStoreFloat4(&target_f, camera_.target);
		float target_arr[3] = { target_f.x, target_f.y, target_f.z };

		ImGui::Text("Target LookAt Position:");
		if (ImGui::DragFloat3("##OrbitTarget", target_arr, 0.1f, -100.0f, 100.0f, "%.2f")) {
			camera_.target = dx::XMVectorSet(target_arr[0], target_arr[1], target_arr[2], 1.0f);
		}

		ImGui::SameLine();
		if (ImGui::Button("Reset to Origin")) {
			camera_.target = dx::XMVectorZero();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 4. クランプ設定の変更
		if (ImGui::TreeNode("Advanced Limits (Radius)")) {
			ImGui::DragFloat("Min Radius Limit", &_min_radius, 0.1f, 0.1f, 10.0f, "%.1f");
			ImGui::DragFloat("Max Radius Limit", &_max_radius, 1.0f, 10.0f, 500.0f, "%.1f");
			ImGui::TreePop();
		}

		// 5. ステータスモニタ
		ImGui::TextDisabled("Angles: Yaw: %.1f, Pitch: %.1f", _yaw, _pitch);
	}
};