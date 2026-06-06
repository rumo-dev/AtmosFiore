#pragma once
#include "camera_base.h"
#include <Windows.h>
#include <DirectXMath.h>
#include "Engine/system/graphics_core.h"
#include "Engine/Graphics/UI/ImGui/ImGui.h"

namespace dx = DirectX;

/**
 * @brief スペクテイター（凝視・デバッグ自由移動）カメラクラス
 */
class Spectator_Camera : public Camera_Base {
private:
	// 初期状態リセット用のバックアップパラメータ
	dx::XMVECTOR default_position;
	dx::XMVECTOR default_yaw_pitch_radius; // 初期アングル保持用 (X: yaw, Y: pitch)

	bool _is_camera_active = true;
	HWND _hwnd{};

	// マウス回転用の変数
	bool _first_mouse = true;
	POINT _last_mouse_pos{};
	float _yaw = 90.0f;   // ヨー角（初期方向を向くよう調整）
	float _pitch = 0.0f;   // ピッチ角

	// ImGui から調整できるように const を外しました
	float _base_move_speed = 10.0f;  // 秒速10ユニット（デフォルト）
	float _mouse_sensitivity = 0.1f;

public:
	Spectator_Camera(HWND hwnd = Graphics_Core::instance().get_window_handle())
		: _hwnd(hwnd)
	{
		// 初期状態のパラメータを保存
		default_position = camera_.position;
		default_yaw_pitch_radius = dx::XMVectorSet(_yaw, _pitch, 0.0f, 0.0f);
	}

	void update(float deltaTime) override {
		if (!_is_camera_active) return;

		// ゲームウィンドウが非アクティブな時は入力を無視する（ウィンドウ移動時などの暴走防止）
		if (GetActiveWindow() != _hwnd) {
			_first_mouse = true;
			return;
		}

		// -------------------------------------------------------------
		// 1. マウスによる視点回転（右クリックが押されている場合のみ）
		// -------------------------------------------------------------
		if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
			POINT current_mouse_pos;
			GetCursorPos(&current_mouse_pos);
			if (_hwnd) {
				ScreenToClient(_hwnd, &current_mouse_pos);
			}

			// 右クリックを押した最初のフレームは、座標を同期するだけで計算をスキップ（カメラの跳ね防止）
			if (_first_mouse) {
				_last_mouse_pos = current_mouse_pos;
				_first_mouse = false;
			}
			else {
				// マウスの移動量を計算 (符号を反転させ、直感的なドラッグ方向に調整可能)
				float offsetX = static_cast<float>(_last_mouse_pos.x - current_mouse_pos.x);
				float offsetY = static_cast<float>(_last_mouse_pos.y - current_mouse_pos.y); // Y軸反転対応
				_last_mouse_pos = current_mouse_pos;

				// 回転角の更新
				_yaw += offsetX * _mouse_sensitivity;
				_pitch += offsetY * _mouse_sensitivity;

				// ピッチ（上下制限）のクランプ処理
				if (_pitch > 89.0f)  _pitch = 89.0f;
				if (_pitch < -89.0f) _pitch = -89.0f;
			}
		}
		else {
			_first_mouse = true; // 右クリックを離したら視点移動フラグをリセット
		}

		// ヨー・ピッチから前方向（Forward）ベクトルを計算
		float pitch_rad = dx::XMConvertToRadians(_pitch);
		float yaw_rad = dx::XMConvertToRadians(_yaw);

		dx::XMVECTOR forward = dx::XMVectorSet(
			cosf(pitch_rad) * cosf(yaw_rad),
			sinf(pitch_rad),
			cosf(pitch_rad) * sinf(yaw_rad),
			0.0f
		);
		forward = dx::XMVector3Normalize(forward);

		// -------------------------------------------------------------
		// 2. キーボードによるカメラ座標の移動処理 (W, A, S, D, Q, E)
		// -------------------------------------------------------------
		// Shiftキーが押されていたら速度を4倍にする（ブースト機能）
		float current_speed = _base_move_speed;
		if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
			current_speed *= 4.0f;
		}
		float distance = current_speed * deltaTime;

		// 世界基準の上方向（0, 1, 0）から、現在の視線に直交する右・上ベクトルを常に正確に計算
		dx::XMVECTOR world_up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		dx::XMVECTOR right = dx::XMVector3Normalize(dx::XMVector3Cross(world_up, forward));
		dx::XMVECTOR up = dx::XMVector3Normalize(dx::XMVector3Cross(forward, right));

		// W / S : 前後移動
		if (GetAsyncKeyState('W') & 0x8000) camera_.position = dx::XMVectorMultiplyAdd(dx::XMVectorReplicate(distance), forward, camera_.position);
		if (GetAsyncKeyState('S') & 0x8000) camera_.position = dx::XMVectorMultiplyAdd(dx::XMVectorReplicate(-distance), forward, camera_.position);

		// A / D : 左右平行移動
		if (GetAsyncKeyState('A') & 0x8000) camera_.position = dx::XMVectorMultiplyAdd(dx::XMVectorReplicate(-distance), right, camera_.position);
		if (GetAsyncKeyState('D') & 0x8000) camera_.position = dx::XMVectorMultiplyAdd(dx::XMVectorReplicate(distance), right, camera_.position);

		// Q / E : 垂直上下移動（絶対空間のY軸）
		if (GetAsyncKeyState('E') & 0x8000) camera_.position = dx::XMVectorMultiplyAdd(dx::XMVectorReplicate(distance), world_up, camera_.position);
		if (GetAsyncKeyState('Q') & 0x8000) camera_.position = dx::XMVectorMultiplyAdd(dx::XMVectorReplicate(-distance), world_up, camera_.position);

		// 注視点（Target）とカメラのUpベクトルを更新
		camera_.target = dx::XMVectorAdd(camera_.position, forward);
		camera_.up = up;
	}
	void set_rotation(float yaw_deg, float pitch_deg, const dx::XMVECTOR& pos, const dx::XMVECTOR& tar) {
		_yaw = yaw_deg;
		_pitch = pitch_deg;
		_first_mouse = true; // マウスの跳ね返り防止

		// 構造体（camera_）の生データを直接書き換える
		camera_.position = pos;
		camera_.target = tar;

		// ビュー行列をその場で強制再計算（これを行わないと1フレーム画面がブレます）
		camera_.view = dx::XMMatrixLookAtLH(camera_.position, camera_.target, camera_.up);
	}
	/**
	 * @brief [オーバーライド] ImGui用デバッグUIの描画
	 * 統合されたウインドウ（タブ）の中に自動配置されます
	 */
	void draw_imgui() override {
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "--- Spectator Camera Settings ---");

		// 1. 移動・回転の感度調整
		ImGui::SliderFloat("Move Speed (m/s)", &_base_move_speed, 1.0f, 50.0f, "%.1f");
		ImGui::TextDisabled("*(Hold [SHIFT] to sprint at 4x speed: %.1f m/s)", _base_move_speed * 4.0f);

		ImGui::SliderFloat("Mouse Sensitivity", &_mouse_sensitivity, 0.01f, 1.0f, "%.2f");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 2. カメラのトランスフォームモニタリング
		dx::XMFLOAT4 pos;
		dx::XMStoreFloat4(&pos, camera_.position);
		ImGui::Text("Current Position:");
		ImGui::BulletText("X: %.2f", pos.x);
		ImGui::BulletText("Y: %.2f (Height)", pos.y);
		ImGui::BulletText("Z: %.2f", pos.z);

		ImGui::TextDisabled("Orientation: Yaw: %.1f, Pitch: %.1f", _yaw, _pitch);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 3. 初期状態リセットボタン（マップ外にすっ飛んでいって迷子になったときの救済用）
		if (ImGui::Button("Reset to Default Spawn Position", ImVec2(-1, 30))) {
			camera_.position = default_position;
			_yaw = dx::XMVectorGetX(default_yaw_pitch_radius);
			_pitch = dx::XMVectorGetY(default_yaw_pitch_radius);
			_first_mouse = true;
		}
	}
};
