#pragma once
#include "camera_base.h"
#include <Windows.h>
#include <DirectXMath.h>
#include <algorithm>
#include "Engine/Graphics/UI/ImGui/ImGui.h"

namespace dx = DirectX;

/**
 * @brief サードパーソン（三人称・キャラクター追従）カメラクラス
 */
class Third_Person_Camera : public Camera_Base {
private:
	HWND _hwnd{};
	bool _first_mouse = true;
	POINT _last_mouse_pos{};

	float _yaw = -90.0f;
	float _pitch = 20.0f;             // 初期俯仰角（少し見下ろす角度）
	float _distance = 5.0f;           // キャラクターからの基本距離

	// ImGui から調整できるように const を外しました
	float _mouse_sensitivity = 0.1f;
	float _lerp_factor = 0.1f;        // 追従の滑らかさ（1.0fで遅延なし固定）

	// 上下の旋回クランプ限界値
	float _min_pitch = -10.0f;
	float _max_pitch = 80.0f;

	// 追従対象の最新座標を保持する変数
	dx::XMVECTOR _target_position{ dx::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f) };

public:
	Third_Person_Camera(HWND hwnd = nullptr) : _hwnd(hwnd) {
		camera_.position = dx::XMVectorSet(0.0f, 0.0f, -5.0f, 1.0f);
		camera_.target = dx::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		camera_.up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	}

	/**
	 * @brief 外部（プレイヤー制御クラスなど）から最新のキャラクター座標を設定する
	 */
	void set_target_position(const dx::XMVECTOR& target_pos) {
		_target_position = target_pos;
	}

	/**
	 * @brief マネージャーから毎フレーム呼ばれる主更新処理
	 */
	void update(float deltaTime) override {
		// 1. 右クリックドラッグによる視点旋回処理
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
				float offsetY = static_cast<float>(current_mouse_pos.y - current_mouse_pos.y);
				_last_mouse_pos = current_mouse_pos;

				_yaw += offsetX * _mouse_sensitivity;
				_pitch += offsetY * _mouse_sensitivity;

				// 地面にめり込んだり真上で行き過ぎたりしないようクランプ
				_pitch = std::clamp(_pitch, _min_pitch, _max_pitch);
			}
		}
		else {
			_first_mouse = true;
		}

		// 2. 回転角（ヨー・ピッチ）から球面座標のオフセットベクトルを計算
		float pitch_rad = dx::XMConvertToRadians(_pitch);
		float yaw_rad = dx::XMConvertToRadians(_yaw);

		dx::XMVECTOR offset = dx::XMVectorSet(
			cosf(pitch_rad) * cosf(yaw_rad),
			sinf(pitch_rad),
			cosf(pitch_rad) * sinf(yaw_rad),
			0.0f
		);
		offset = dx::XMVectorScale(dx::XMVector3Normalize(offset), _distance);

		// 3. 目標とするカメラ位置（キャラクター座標 + オフセット）
		dx::XMVECTOR desired_position = dx::XMVectorAdd(_target_position, offset);

		// 4. 線形補間(Lerp)を用いて滑らかに追従
		camera_.position = dx::XMVectorLerp(camera_.position, desired_position, _lerp_factor);

		// 注視点は常にキャラクターの座標
		camera_.target = _target_position;

		// ビュー行列が崩れないよう、直交する正確なUpベクトルを再計算
		dx::XMVECTOR world_up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		dx::XMVECTOR forward = dx::XMVector3Normalize(dx::XMVectorSubtract(camera_.target, camera_.position));
		dx::XMVECTOR right = dx::XMVector3Normalize(dx::XMVector3Cross(world_up, forward));
		camera_.up = dx::XMVector3Normalize(dx::XMVector3Cross(forward, right));
	}

	void set_distance(float distance) { _distance = (std::max)(1.0f, distance); }
	float get_distance() const { return _distance; }
	void set_rotation(float yaw_deg, float pitch_deg, const dx::XMVECTOR& pos, const dx::XMVECTOR& tar) {
		_yaw = yaw_deg;
		_pitch = pitch_deg;
		_first_mouse = true;

		camera_.position = pos;
		camera_.target = tar;

		// ターゲット（キャラクター）とカメラの間の物理的な距離を計算して同期
		dx::XMVECTOR sub = dx::XMVectorSubtract(pos, tar);
		_distance = dx::XMVectorGetX(dx::XMVector3Length(sub));

		// ビュー行列を即座に計算
		camera_.view = dx::XMMatrixLookAtLH(camera_.position, camera_.target, camera_.up);
	}
	/**
	 * @brief [オーバーライド] ImGui用デバッグUIの描画
	 * 統合されたウインドウ（タブ）の中に自動配置されます
	 */
	void draw_imgui() override {
		ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "--- Third Person Camera Settings ---");

		// 1. キャラクターからのカメラの距離調整
		if (ImGui::SliderFloat("Camera Distance", &_distance, 1.0f, 20.0f, "%.1f m")) {
			if (_distance < 1.0f) _distance = 1.0f;
		}

		// 2. 追従ラグ（滑らかさ）の調整
		ImGui::SliderFloat("Lerp Factor (Lag)", &_lerp_factor, 0.01f, 1.0f, "%.2f");
		ImGui::TextDisabled("*(1.0f = Rigid/No Delay, Lower = Smooth/Elastic)");

		// 3. マウス回転感度
		ImGui::SliderFloat("Mouse Sensitivity", &_mouse_sensitivity, 0.01f, 1.0f, "%.2f");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 4. 高度な設定：上下回転角度のクランプ制限調整
		if (ImGui::TreeNode("Pitch Angle Limits")) {
			ImGui::DragFloat("Min Pitch (Look Up)", &_min_pitch, 0.5f, -80.0f, 0.0f, "%.1f deg");
			ImGui::DragFloat("Max Pitch (Look Down)", &_max_pitch, 0.5f, 10.0f, 85.0f, "%.1f deg");
			ImGui::TreePop();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 5. ステータスモニター
		ImGui::Text("Camera Status:");
		ImGui::BulletText("Yaw   : %.1f deg", _yaw);
		ImGui::BulletText("Pitch : %.1f deg", _pitch);

		dx::XMFLOAT4 p;
		dx::XMStoreFloat4(&p, _target_position);
		ImGui::BulletText("Target Pos: X:%.1f, Y:%.1f, Z:%.1f", p.x, p.y, p.z);
	}
};