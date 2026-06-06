#pragma once
#include "camera_base.h"
#include <Windows.h>
#include <DirectXMath.h>
#include <algorithm>
#include "Engine/Graphics/UI/ImGui/ImGui.h"

namespace dx = DirectX;

/**
 * @brief クォータービュー（定点・俯瞰・RTS）カメラクラス
 */
class Quarter_View_Camera : public Camera_Base {
private:
	// ImGui から調整できるように const を外しました
	float _fixed_pitch = 45.0f;
	float _fixed_yaw = -45.0f;
	float _offset_distance = 15.0f; // 見下ろす高さと距離の基準

	float _scroll_speed = 15.0f;

public:
	/**
	 * @param initial_focus_point 初期状態のターゲット（地面の中心点など）
	 */
	Quarter_View_Camera(const dx::XMVECTOR& initial_focus_point = dx::XMVectorZero()) {
		camera_.target = initial_focus_point;
		update_camera_position();
	}

	void update(float deltaTime) override {
		float move_dist = _scroll_speed * deltaTime;

		// 画面の並行スクロール移動ベクトルの定義
		// （固定アングルの向きに合わせて、斜めに気持ちよくスクロールするように調整）
		dx::XMVECTOR forward_scrolling = dx::XMVectorSet(1.0f, 0.0f, 1.0f, 0.0f);  // 斜め前方向
		dx::XMVECTOR right_scrolling = dx::XMVectorSet(1.0f, 0.0f, -1.0f, 0.0f); // 斜め右方向
		forward_scrolling = dx::XMVector3Normalize(forward_scrolling);
		right_scrolling = dx::XMVector3Normalize(right_scrolling);

		// WASD入力で、見見下ろしている中心（ターゲット）側を移動させる
		if (GetAsyncKeyState('W') & 0x8000) camera_.target = dx::XMVectorMultiplyAdd(dx::XMVectorReplicate(-move_dist), right_scrolling, camera_.target);
		if (GetAsyncKeyState('S') & 0x8000) camera_.target = dx::XMVectorMultiplyAdd(dx::XMVectorReplicate(move_dist), right_scrolling, camera_.target);
		if (GetAsyncKeyState('A') & 0x8000) camera_.target = dx::XMVectorMultiplyAdd(dx::XMVectorReplicate(-move_dist), forward_scrolling, camera_.target);
		if (GetAsyncKeyState('D') & 0x8000) camera_.target = dx::XMVectorMultiplyAdd(dx::XMVectorReplicate(move_dist), forward_scrolling, camera_.target);

		// ターゲットの移動に追従して、カメラの位置を一定のオフセットで同期
		update_camera_position();
	}

	/**
	 * @brief [オーバーライド] ImGui用デバッグUIの描画
	 * 統合されたウインドウ（タブ）の中に自動配置されます
	 */
	void draw_imgui() override {
		ImGui::TextColored(ImVec4(0.7f, 0.4f, 1.0f, 1.0f), "--- Quarter View Camera Settings ---");

		// 1. 基本的なスクロール移動速度の調整
		ImGui::SliderFloat("Scroll Speed", &_scroll_speed, 1.0f, 50.0f, "%.1f m/s");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 2. カメラのアングル調整（リアルタイムに俯瞰度合いやカメラの向きを変えられます）
		ImGui::Text("Camera Angle & Distance:");

		bool changed = false;
		changed |= ImGui::SliderFloat("Pitch (Look Down)", &_fixed_pitch, 10.0f, 89.0f, "%.1f deg");
		changed |= ImGui::SliderFloat("Yaw (Rotation)", &_fixed_yaw, -180.0f, 180.0f, "%.1f deg");
		changed |= ImGui::SliderFloat("Height Distance", &_offset_distance, 5.0f, 50.0f, "%.1f m");

		// パラメータが変更されたら、即座に位置計算に反映させる
		if (changed) {
			update_camera_position();
		}

		// アングルのクイックプリセット
		ImGui::Text("Angle Presets:");
		if (ImGui::Button("Classic Isometric (-45 deg)")) {
			_fixed_pitch = 35.264f; // 数学的に正しいアイソメトリックのピッチ角
			_fixed_yaw = -45.0f;
			update_camera_position();
		}
		ImGui::SameLine();
		if (ImGui::Button("True Top-Down (90 deg)")) {
			_fixed_pitch = 89.9f; // 完全な真上（90度だとジンバルロックの懸念があるため安全値）
			_fixed_yaw = 0.0f;
			update_camera_position();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 3. 現在マップのどのあたりを見ているかの座標モニタリング（リセット機能付き）
		dx::XMFLOAT4 target_f;
		dx::XMStoreFloat4(&target_f, camera_.target);
		ImGui::Text("Current Focus Point: X:%.2f, Z:%.2f", target_f.x, target_f.z);

		if (ImGui::Button("Reset Focus to Center")) {
			camera_.target = dx::XMVectorZero();
			update_camera_position();
		}
	}

private:
	void update_camera_position() {
		float pitch_rad = dx::XMConvertToRadians(_fixed_pitch);
		float yaw_rad = dx::XMConvertToRadians(_fixed_yaw);

		// 固定角度からオフセットベクトルを計算
		dx::XMVECTOR offset = dx::XMVectorSet(
			cosf(pitch_rad) * cosf(yaw_rad),
			sinf(pitch_rad),
			cosf(pitch_rad) * sinf(yaw_rad),
			0.0f
		);
		offset = dx::XMVectorScale(dx::XMVector3Normalize(offset), _offset_distance);

		// 位置 ＝ 最新のターゲット座標 ＋ 固定オフセット
		camera_.position = dx::XMVectorAdd(camera_.target, offset);

		// 完全に固定された俯瞰視点のため、Upは常にワールド上向きで固定
		camera_.up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	}
};