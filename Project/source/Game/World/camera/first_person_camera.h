#pragma once
#include "camera_base.h"
#include <Windows.h>
#include <DirectXMath.h>
#include <algorithm>

#include "Engine/Graphics/UI/ImGui/ImGui.h"

namespace dx = DirectX;

/**
 * @brief ファーストパーソン（一人称・FPS）カメラクラス
 */
class First_Person_Camera : public Camera_Base {
private:
	HWND _hwnd{};
	bool _first_mouse = true;
	POINT _last_mouse_pos{};

	float _yaw = -90.0f;
	float _pitch = 0.0f;
	float _mouse_sensitivity = 0.1f; // const を外して ImGui から調整可能に

	dx::XMVECTOR _character_head_pos{ dx::XMVectorSet(0.0f, 1.8f, 0.0f, 1.0f) };
	dx::XMVECTOR _current_forward{ dx::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f) };

public:
	First_Person_Camera(HWND hwnd = nullptr) : _hwnd(hwnd) {
		camera_.position = _character_head_pos;
		camera_.target = dx::XMVectorSet(0.0f, 1.8f, 1.0f, 1.0f);
		camera_.up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	}

	/**
	 * @brief 外部（プレイヤー制御クラスなど）から最新の頭部座標を設定する
	 */
	void set_character_head_position(const dx::XMVECTOR& pos) {
		_character_head_pos = pos;
	}

	/**
	 * @brief 最新の視線ベクトル（キャラの移動方向計算用）を取得する
	 */
	dx::XMVECTOR get_forward_vector() const {
		return _current_forward;
	}

	/**
	 * @brief マネージャーから毎フレーム呼ばれる主更新処理
	 */
	void update(float deltaTime) override {
		// 1. 右クリックドラッグによる視線回転処理
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

				// 真上・真下を向いた際の首の反転を防止
				_pitch = std::clamp(_pitch, -85.0f, 85.0f);
			}
		}
		else {
			_first_mouse = true;
		}

		// 2. ヨー・ピッチから前方向（Forward）ベクトルを算出
		float pitch_rad = dx::XMConvertToRadians(_pitch);
		float yaw_rad = dx::XMConvertToRadians(_yaw);

		dx::XMVECTOR forward = dx::XMVectorSet(
			cosf(pitch_rad) * cosf(yaw_rad),
			sinf(pitch_rad),
			cosf(pitch_rad) * sinf(yaw_rad),
			0.0f
		);
		_current_forward = dx::XMVector3Normalize(forward);

		// 3. カメラ位置を同期された頭部座標へ固定
		camera_.position = _character_head_pos;
		camera_.target = dx::XMVectorAdd(camera_.position, _current_forward);

		// 直交するUpベクトルの再計算
		dx::XMVECTOR world_up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		dx::XMVECTOR right = dx::XMVector3Normalize(dx::XMVector3Cross(world_up, _current_forward));
		camera_.up = dx::XMVector3Normalize(dx::XMVector3Cross(_current_forward, right));
	}

	/**
	 * @brief [オーバーライド] ImGui用デバッグUIの描画
	 * 統合されたウインドウ（タブ）の中に自動配置されます
	 */
	void draw_imgui() override {
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "--- First Person Camera Settings ---");

		// マウス感度のリアルタイム調整（constを外したことで変更可能に）
		ImGui::SliderFloat("Mouse Sensitivity", &_mouse_sensitivity, 0.01f, 1.0f, "%.2f");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 現在のパラメータの可視化（デバッグ用）
		ImGui::Text("Camera Orientation:");
		ImGui::BulletText("Yaw   : %.1f deg", _yaw);
		ImGui::BulletText("Pitch : %.1f deg", _pitch);

		dx::XMFLOAT4 pos;
		dx::XMStoreFloat4(&pos, camera_.position);
		ImGui::BulletText("Position : X:%.2f, Y:%.2f, Z:%.2f", pos.x, pos.y, pos.z);
	}
};