#pragma once
#include "camera_base.h"
#include <Windows.h>
#include <DirectXMath.h>
#include "Engine/system/graphics_core.h"
#include "Engine/Graphics/UI/ImGui/ImGui.h"

namespace dx = DirectX;

/**
 * @brief 対象追従カメラ（TPS/三人称視点など）クラス
 */
class Follow_Camera : public Camera_Base {
private:
	HWND _hwnd{};

	// 追従対象の座標
	dx::XMVECTOR _target_position{ dx::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f) };

	// ターゲットからの相対距離・高さ（オフセット）
	dx::XMVECTOR _offset{ dx::XMVectorSet(0.0f, 4.0f, -8.0f, 0.0f) };

	float _lerp_factor = 5.0f; // カメラ遅延追従のスムーズさ（補間係数）

public:
	Follow_Camera(HWND hwnd = Graphics_Core::instance().get_window_handle())
		: _hwnd(hwnd)
	{
		// 初期姿勢を安定させるための初期位置設定
		camera_.position = dx::XMVectorAdd(_target_position, _offset);
		camera_.target = _target_position;
		camera_.up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	}

	/**
	 * @brief 追従対象の座標を更新する
	 */
	void set_target_position(dx::XMVECTOR position) {
		_target_position = position;
	}

	void update(float deltaTime) override {
		// 本来あるべき理想のカメラ位置（ターゲット座標 + オフセット量）
		dx::XMVECTOR ideal_position = dx::XMVectorAdd(_target_position, _offset);

		// 線形補間（Lerp）を用いて、カメラが少し遅れて滑らかに追いつく処理
		float t = _lerp_factor * deltaTime;
		if (t > 1.0f) t = 1.0f; // オーバーシュート防止

		dx::XMVECTOR diff = dx::XMVectorSubtract(ideal_position, camera_.position);
		camera_.position = dx::XMVectorMultiplyAdd(dx::XMVectorReplicate(t), diff, camera_.position);

		// 注視点は常にターゲットを向き続ける
		camera_.target = _target_position;

		// カメラの Up ベクトルを常に直交化させて安定させる
		dx::XMVECTOR world_up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		dx::XMVECTOR forward = dx::XMVector3Normalize(dx::XMVectorSubtract(camera_.target, camera_.position));
		dx::XMVECTOR right = dx::XMVector3Normalize(dx::XMVector3Cross(world_up, forward));
		camera_.up = dx::XMVector3Normalize(dx::XMVector3Cross(forward, right));
	}

	/**
	 * @brief [オーバーライド] ImGui用デバッグUIの描画
	 * 統合されたウインドウ（タブ）の中に自動配置されます
	 */
	void draw_imgui() override {
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "--- Follow Camera Settings ---");

		// 1. 追従の滑らかさ (Lerp Factor) の調整
		// 値が大きいほどキビキビ追従し、小さいほどマイルド（ゴム紐のような）追従になります
		ImGui::SliderFloat("Lerp Smoothness", &_lerp_factor, 0.1f, 20.0f, "%.1f");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Higher values make the camera catch up faster.");
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 2. ターゲットからのオフセット（距離・高さ）の調整
		dx::XMFLOAT4 offset_f;
		dx::XMStoreFloat4(&offset_f, _offset);

		float offset_arr[3] = { offset_f.x, offset_f.y, offset_f.z };

		ImGui::Text("Camera Offset (Relative to Target):");
		ImGui::DragFloat3("##Offset", offset_arr, 0.05f, -20.0f, 20.0f, "%.2f");

		// 変更があったら XMVECTOR に書き戻す
		if (ImGui::IsItemEdited()) {
			_offset = dx::XMVectorSet(offset_arr[0], offset_arr[1], offset_arr[2], 0.0f);
		}

		// プリセットボタン（ワンクリックで一般的なアングルに戻せる機能）
		ImGui::Text("Presets:");
		if (ImGui::Button("Standard TPS")) {
			_offset = dx::XMVectorSet(0.0f, 3.5f, -6.0f, 0.0f);
			_lerp_factor = 5.0f;
		}
		ImGui::SameLine();
		if (ImGui::Button("Bird's Eye (Top Down)")) {
			_offset = dx::XMVectorSet(0.0f, 12.0f, -0.1f, 0.0f); // ほぼ真上
			_lerp_factor = 8.0f;
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 3. 現在の状態をモニタリング（デバッグ用）
		dx::XMFLOAT4 current_pos;
		dx::XMStoreFloat4(&current_pos, camera_.position);
		ImGui::Text("Current Position: X:%.2f, Y:%.2f, Z:%.2f", current_pos.x, current_pos.y, current_pos.z);
	}
};