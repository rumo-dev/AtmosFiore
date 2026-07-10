#pragma once
#include <Windows.h>
#include <DirectXMath.h>
#include <algorithm>
#include <vector>
#include <string>
#include "Engine/utilities/dx_common.h"
#include "Engine/utilities/math_util.h"
#include "Engine/utilities/collision_util.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"

namespace dx = DirectX;

/**
 * @brief プレイヤークラス（ドローン版）
 *
 * クアッドコプターのような挙動：
 * - カメラのYaw方向を向く（向きはカメラに追従）
 * - WASDの移動方向はカメラ基準（変更なし）
 * - モデルの前方軸に応じて傾き方向だけを変換
 * - 左右移動は向きを変えずに平行移動
 * - スポットライトはカメラの向きに追従
 */
class Player
{
public:

	void initialize(HWND hwnd,
		dx::XMVECTOR start_pos = dx::XMVectorSet(0.0f, 5.0f, 0.0f, 1.0f),
		int spotlight_index = -1)
	{
		hwnd_ = hwnd;
		position_ = start_pos;
		spotlight_index_ = spotlight_index;

		// 初期姿勢（Z軸正方向が前）
		up_vector_ = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		forward_vector_ = dx::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		right_vector_ = dx::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

		is_dead_ = false;
		animation_index_ = 0;
		animation_speed_ = 1.0f;

		yaw_ = 0.0f;
		pitch_ = 0.0f;
		target_roll_ = 0.0f;
		current_roll_ = 0.0f;
		current_pitch_tilt_ = 0.0f;

		// デフォルトモデル名
		model_name_ = "Spider";
		scale_ = dx::XMVectorSet(0.3f, 0.3f, 0.3f, 1.0f);
	}

	void update(float elapsed_time, float camera_yaw_deg, float camera_pitch_deg = 0.0f,
		const std::vector<dx::XMFLOAT3>* collision_triangles = nullptr)
	{
		yaw_ = camera_yaw_deg;
		pitch_ = camera_pitch_deg;
		HandleMovement_(elapsed_time, collision_triangles);
	}

	// ======================================================
	//  ゲッター / セッター
	// ======================================================

	dx::XMVECTOR get_position() const { return position_; }

	dx::XMVECTOR get_head_position() const
	{
		return dx::XMVectorAdd(position_, dx::XMVectorScale(up_vector_, eye_height_));
	}

	dx::XMVECTOR get_forward() const { return forward_vector_; }

	int get_spotlight_index() const { return spotlight_index_; }

	const std::string& get_model_name() const { return model_name_; }
	void set_model_name(const std::string& name) { model_name_ = name; }

	int get_model_forward_axis() const { return model_forward_axis_; }
	void set_model_forward_axis(int axis) { model_forward_axis_ = axis; }

	dx::XMVECTOR get_scale() const { return scale_; }
	void set_scale(dx::XMVECTOR scale) { scale_ = scale; }
	void set_scale(float uniform_scale) { scale_ = dx::XMVectorSet(uniform_scale, uniform_scale, uniform_scale, 1.0f); }

	dx::XMFLOAT4X4 get_world_transform() const
	{
		dx::XMVECTOR U = dx::XMVector3Normalize(up_vector_);
		dx::XMVECTOR F = dx::XMVector3Normalize(forward_vector_);
		dx::XMVECTOR R = dx::XMVector3Normalize(dx::XMVector3Cross(U, F));
		F = dx::XMVector3Cross(R, U);

		// ============================================================
		// モデルの向き補正（モデルがX軸正方向を向いている場合）
		// ============================================================
		if (model_forward_axis_ == 1) // X軸正方向が前
		{
			dx::XMMATRIX rot_correction = dx::XMMatrixRotationY(-dx::XM_PIDIV2);
			F = dx::XMVector3TransformNormal(F, rot_correction);
			R = dx::XMVector3TransformNormal(R, rot_correction);
			U = dx::XMVector3TransformNormal(U, rot_correction);
		}
		else if (model_forward_axis_ == 2) // Z軸負方向が前
		{
			dx::XMMATRIX rot_correction = dx::XMMatrixRotationY(dx::XM_PI);
			F = dx::XMVector3TransformNormal(F, rot_correction);
			R = dx::XMVector3TransformNormal(R, rot_correction);
			U = dx::XMVector3TransformNormal(U, rot_correction);
		}
		else if (model_forward_axis_ == 3) // X軸負方向が前
		{
			dx::XMMATRIX rot_correction = dx::XMMatrixRotationY(dx::XM_PIDIV2);
			F = dx::XMVector3TransformNormal(F, rot_correction);
			R = dx::XMVector3TransformNormal(R, rot_correction);
			U = dx::XMVector3TransformNormal(U, rot_correction);
		}
		// _model_forward_axis == 0 はZ軸正方向（デフォルト、何もしない）

		dx::XMMATRIX rot_matrix = dx::XMMatrixIdentity();
		rot_matrix.r[0] = R;
		rot_matrix.r[1] = U;
		rot_matrix.r[2] = F;

		// ★ スケール行列を追加
		dx::XMMATRIX scale_matrix = dx::XMMatrixScalingFromVector(scale_);

		dx::XMMATRIX C = dx::XMLoadFloat4x4(&COORDINATE_SYSTEM_TABLE[0]);
		dx::XMMATRIX T = dx::XMMatrixTranslationFromVector(position_);
		dx::XMMATRIX M = C * scale_matrix * rot_matrix * T;

		dx::XMFLOAT4X4 out;
		dx::XMStoreFloat4x4(&out, M);
		return out;
	}

	int get_animation_index() const { return animation_index_; }
	float get_animation_speed() const { return animation_speed_; }

	void set_dead(bool dead) { is_dead_ = dead; }
	bool is_dead() const { return is_dead_; }
	void trigger_idle_large() {}

	// ======================================================
	//  ImGui
	// ======================================================

	void draw_imgui()
	{
		ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "--- Drone Settings ---");

		// モデル名の入力
		char buffer[128];
		strcpy_s(buffer, model_name_.c_str());
		if (ImGui::InputText("Model Name", buffer, sizeof(buffer)))
		{
			model_name_ = buffer;
		}

		// モデルの向き補正
		const char* axis_items[] = { "+Z (Forward)", "+X (Right)", "-Z (Back)", "-X (Left)" };
		ImGui::Combo("Forward Axis", &model_forward_axis_, axis_items, 4);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// ★ スケール調整（追加）
		ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Scale:");

		// ユニフォームスケール（一括調整）
		float uniform_scale = dx::XMVectorGetX(scale_);
		if (ImGui::SliderFloat("Uniform Scale", &uniform_scale, 0.1f, 5.0f, "%.2f"))
		{
			set_scale(uniform_scale);
		}

		// 個別軸スケール（詳細調整）
		dx::XMFLOAT3 scale_float;
		dx::XMStoreFloat3(&scale_float, scale_);
		bool scale_changed = false;
		scale_changed |= ImGui::SliderFloat("Scale X", &scale_float.x, 0.1f, 5.0f, "%.2f");
		scale_changed |= ImGui::SliderFloat("Scale Y", &scale_float.y, 0.1f, 5.0f, "%.2f");
		scale_changed |= ImGui::SliderFloat("Scale Z", &scale_float.z, 0.1f, 5.0f, "%.2f");
		if (scale_changed)
		{
			scale_ = dx::XMVectorSet(scale_float.x, scale_float.y, scale_float.z, 1.0f);
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::SliderFloat("Move Speed", &speed_, 0.5f, 30.0f, "%.1f m/s");
		ImGui::SliderFloat("Vertical Speed", &vertical_speed_, 0.5f, 20.0f, "%.1f m/s");
		ImGui::SliderFloat("Sprint Multiplier", &sprint_mult_, 1.0f, 5.0f, "%.1f x");
		ImGui::SliderFloat("Yaw Follow Speed", &yaw_follow_speed_, 1.0f, 20.0f, "%.1f");
		ImGui::SliderFloat("Tilt Speed", &tilt_speed_, 1.0f, 15.0f, "%.1f");
		ImGui::SliderFloat("Max Tilt Angle", &max_tilt_angle_, 5.0f, 45.0f, "%.1f deg");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Collision:");
		ImGui::SliderFloat("Sphere Radius", &sphere_radius_, 0.1f, 2.0f, "%.2f m");
		ImGui::Checkbox("Enable Collision", &enable_collision_);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Status:");
		dx::XMFLOAT4 p;
		dx::XMStoreFloat4(&p, position_);
		ImGui::BulletText("Position : X:%.2f  Y:%.2f  Z:%.2f", p.x, p.y, p.z);
		ImGui::BulletText("Yaw      : %.1f deg", yaw_);
		ImGui::BulletText("Pitch    : %.1f deg", pitch_);
		ImGui::BulletText("Roll     : %.1f deg", current_roll_);
		ImGui::BulletText("Tilt     : %.1f deg", current_pitch_tilt_);
		ImGui::BulletText("Dead     : %s", is_dead_ ? "Yes" : "No");
		ImGui::BulletText("Model    : %s", model_name_.c_str());
		ImGui::BulletText("Axis     : %s", axis_items[model_forward_axis_]);

		// ★ スケール情報を表示
		dx::XMFLOAT3 s;
		dx::XMStoreFloat3(&s, scale_);
		ImGui::BulletText("Scale    : X:%.2f  Y:%.2f  Z:%.2f", s.x, s.y, s.z);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Spotlight:");
		ImGui::SliderFloat("Light Radius", &light_radius_, 0.5f, 50.0f, "%.1f");
		ImGui::SliderFloat("Light Intensity", &light_intensity_, 0.0f, 20.0f, "%.1f");
		ImGui::SliderFloat("Light Inner Angle", &light_inner_angle_, 0.01f, 1.5f, "%.2f rad");
		ImGui::SliderFloat("Light Outer Angle", &light_outer_angle_, 0.01f, 1.6f, "%.2f rad");
		ImGui::ColorEdit3("Light Color", (float*)&light_color_);
		ImGui::SliderFloat("Light Height Offset", &light_height_offset_, 0.0f, 3.0f, "%.2f m");
	}

	// ======================================================
	//  ライト情報（カメラの向きに追従）
	// ======================================================

	dx::XMFLOAT3 get_light_position() const
	{
		dx::XMVECTOR base = dx::XMVectorAdd(position_, dx::XMVectorScale(up_vector_, -light_height_offset_));
		dx::XMFLOAT3 out;
		dx::XMStoreFloat3(&out, base);
		return out;
	}

	dx::XMFLOAT3 get_light_direction() const
	{
		float yaw_rad = dx::XMConvertToRadians(yaw_);
		float pitch_rad = dx::XMConvertToRadians(pitch_);

		// カメラの向きに追従（ピッチの符号を修正）
		dx::XMVECTOR dir = dx::XMVectorSet(
			cosf(pitch_rad) * sinf(yaw_rad),
			sinf(pitch_rad),  // ★ -sinf → sinf に修正
			cosf(pitch_rad) * cosf(yaw_rad),
			0.0f
		);
		dir = dx::XMVector3Normalize(dir);

		dx::XMFLOAT3 out;
		dx::XMStoreFloat3(&out, dir);
		return out;
	}

	float             get_light_radius()       const { return light_radius_; }
	float             get_light_intensity()    const { return light_intensity_; }
	float             get_light_inner_angle()  const { return light_inner_angle_; }
	float             get_light_outer_angle()  const { return light_outer_angle_; }
	dx::XMFLOAT4      get_light_color()        const { return light_color_; }

private:

	/**
	 * @brief モデルの前方軸に応じて傾き方向を変換する（修正版）
	 *
	 * 各軸における変換規則：
	 * - 軸 +Z (0): ピッチ→ピッチ, ロール→ロール
	 * - 軸 +X (1): ピッチ→-ロール, ロール→ピッチ
	 * - 軸 -Z (2): ピッチ→-ピッチ, ロール→-ロール
	 * - 軸 -X (3): ピッチ→ロール, ロール→-ピッチ
	 */
	void ConvertTiltByAxis_(float& _pitch_tilt, float& _roll) const
	{
		float orig_pitch = _pitch_tilt;
		float orig_roll = _roll;

		switch (model_forward_axis_)
		{
		case 0: // +Z (デフォルト)
			// そのまま
			break;
		case 1: // +X (モデルが右を向いている)
			// Z→X 軸変換: ピッチ→-ロール, ロール→ピッチ
			_pitch_tilt = -orig_roll;
			_roll = orig_pitch;
			break;
		case 2: // -Z (モデルが後ろを向いている)
			// Z→-Z 軸変換: 両方反転
			_pitch_tilt = -orig_pitch;
			_roll = -orig_roll;
			break;
		case 3: // -X (モデルが左を向いている)
			// Z→-X 軸変換: ピッチ→ロール, ロール→-ピッチ
			_pitch_tilt = orig_roll;
			_roll = -orig_pitch;
			break;
		}
	}

	void HandleMovement_(float _elapsed_time,
		const std::vector<dx::XMFLOAT3>* _collision_triangles = nullptr)
	{
		if (is_dead_)
		{
			animation_index_ = 3;
			animation_speed_ = 1.0f;
			return;
		}

		animation_index_ = 0;
		animation_speed_ = 1.0f;

		float yaw_rad = dx::XMConvertToRadians(yaw_);
		float max_tilt_rad = dx::XMConvertToRadians(max_tilt_angle_);

		dx::XMVECTOR cam_forward = dx::XMVectorSet(sinf(yaw_rad), 0.0f, cosf(yaw_rad), 0.0f);
		dx::XMVECTOR cam_right = dx::XMVectorSet(cosf(yaw_rad), 0.0f, -sinf(yaw_rad), 0.0f);

		// ---- 入力収集（移動方向はカメラ基準で変更なし） ----
		float forward_input = 0.0f;
		float right_input = 0.0f;
		bool is_moving = false;

		if (GetAsyncKeyState('W') & 0x8000) { forward_input += 1.0f; is_moving = true; }
		if (GetAsyncKeyState('S') & 0x8000) { forward_input -= 1.0f; is_moving = true; }
		if (GetAsyncKeyState(VK_UP) & 0x8000) { forward_input += 1.0f; is_moving = true; }
		if (GetAsyncKeyState(VK_DOWN) & 0x8000) { forward_input -= 1.0f; is_moving = true; }

		if (GetAsyncKeyState('D') & 0x8000) { right_input += 1.0f; is_moving = true; }
		if (GetAsyncKeyState('A') & 0x8000) { right_input -= 1.0f; is_moving = true; }
		if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { right_input += 1.0f; is_moving = true; }
		if (GetAsyncKeyState(VK_LEFT) & 0x8000) { right_input -= 1.0f; is_moving = true; }

		float vertical_input = 0.0f;
		if (GetAsyncKeyState(VK_SPACE) & 0x8000) { vertical_input = 1.0f; is_moving = true; }
		if (GetAsyncKeyState(VK_SHIFT) & 0x8000) { vertical_input = -1.0f; is_moving = true; }

		// ---- 移動ベクトル（カメラ基準、変更なし） ----
		bool sprint = (GetAsyncKeyState('Q') & 0x8000) != 0;
		float speed_mult = sprint ? sprint_mult_ : 1.0f;

		dx::XMVECTOR move_delta = dx::XMVectorZero();

		if (is_moving)
		{
			// カメラ基準の方向をワールド座標に変換（入力はそのまま）
			dx::XMVECTOR world_move = dx::XMVectorZero();
			world_move = dx::XMVectorAdd(world_move, dx::XMVectorScale(cam_forward, forward_input));
			world_move = dx::XMVectorAdd(world_move, dx::XMVectorScale(cam_right, right_input));

			float len = Length(world_move);
			if (len > 0.001f)
			{
				world_move = dx::XMVectorScale(world_move, 1.0f / len);
				move_delta = dx::XMVectorScale(world_move, speed_ * speed_mult * _elapsed_time);
			}
		}

		if (vertical_input != 0.0f)
		{
			dx::XMVECTOR vertical_delta = dx::XMVectorScale(
				dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
				vertical_input * vertical_speed_ * _elapsed_time
			);
			move_delta = dx::XMVectorAdd(move_delta, vertical_delta);
		}

		// ---- コリジョン ----
		dx::XMVECTOR corrected_delta = move_delta;
		if (enable_collision_ && _collision_triangles && !_collision_triangles->empty() && Length(move_delta) > 0.001f)
		{
			CollisionSphere sphere{ position_, sphere_radius_ };
			SphereCastTriangles(sphere, move_delta, *_collision_triangles, corrected_delta);
		}

		position_ = dx::XMVectorAdd(position_, corrected_delta);

		// ============================================================
		//  姿勢更新（修正版）
		// ============================================================

		// ---- 1. ヨー（Yaw）：カメラのYawに追従 ----
		float target_yaw_rad = yaw_rad;
		float current_yaw = atan2f(
			dx::XMVectorGetX(forward_vector_),
			dx::XMVectorGetZ(forward_vector_)
		);

		float yaw_diff = target_yaw_rad - current_yaw;
		while (yaw_diff > dx::XM_PI) yaw_diff -= dx::XM_2PI;
		while (yaw_diff < -dx::XM_PI) yaw_diff += dx::XM_2PI;

		float follow_factor = 1.0f - expf(-yaw_follow_speed_ * _elapsed_time);
		float new_yaw = current_yaw + yaw_diff * follow_factor;

		dx::XMVECTOR new_forward = dx::XMVectorSet(sinf(new_yaw), 0.0f, cosf(new_yaw), 0.0f);
		forward_vector_ = dx::XMVector3Normalize(new_forward);

		// ---- 2. 傾き（Tilt）：移動方向に応じて機体を傾ける（修正版） ----
		float target_roll = 0.0f;
		float target_pitch_tilt = 0.0f;

		if (is_moving && Length(move_delta) > 0.001f)
		{
			// ★ 修正: 直感的な傾き方向に変更
			// 前進で前傾（ピッチ）、右移動で右にロール
			float raw_pitch_tilt = -forward_input * max_tilt_rad;  // 前に進むと前傾
			float raw_roll = right_input * max_tilt_rad;          // 右に進むと右にロール

			// モデルの前方軸に応じて傾き方向を変換
			target_pitch_tilt = raw_pitch_tilt;
			target_roll = raw_roll;
			ConvertTiltByAxis_(target_pitch_tilt, target_roll);
		}

		// スムーズ補間
		float tilt_factor = 1.0f - expf(-tilt_speed_ * _elapsed_time);
		current_roll_ += (target_roll - current_roll_) * tilt_factor;
		current_pitch_tilt_ += (target_pitch_tilt - current_pitch_tilt_) * tilt_factor;

		// ---- 3. 姿勢行列の構築 ----
		dx::XMVECTOR world_up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		dx::XMVECTOR right = dx::XMVector3Cross(world_up, forward_vector_);
		float r_len = Length(right);
		if (r_len > 0.001f)
		{
			right = dx::XMVectorScale(right, 1.0f / r_len);
		}
		else
		{
			right = dx::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		}

		// ロール回転（前方ベクトルを軸に回転）
		dx::XMMATRIX roll_rot = dx::XMMatrixRotationAxis(forward_vector_, current_roll_);
		dx::XMVECTOR tilted_up = dx::XMVector3TransformNormal(world_up, roll_rot);

		// ピッチ傾き（右ベクトルを軸に回転）
		dx::XMMATRIX pitch_rot = dx::XMMatrixRotationAxis(right, current_pitch_tilt_);
		dx::XMVECTOR final_up = dx::XMVector3TransformNormal(tilted_up, pitch_rot);

		up_vector_ = dx::XMVector3Normalize(final_up);

		// ---- 4. 直交化 ----
		right_vector_ = dx::XMVector3Cross(up_vector_, forward_vector_);
		forward_vector_ = dx::XMVector3Cross(right_vector_, up_vector_);
		up_vector_ = dx::XMVector3Normalize(up_vector_);
		forward_vector_ = dx::XMVector3Normalize(forward_vector_);
	}

	// ======================================================
	//  メンバ変数
	// ======================================================

	HWND             hwnd_ = nullptr;

	dx::XMVECTOR     position_ = dx::XMVectorSet(0.0f, 5.0f, 0.0f, 1.0f);
	float            yaw_ = 0.0f;
	float            pitch_ = 0.0f;

	dx::XMVECTOR     up_vector_ = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	dx::XMVECTOR     forward_vector_ = dx::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	dx::XMVECTOR     right_vector_ = dx::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

	dx::XMVECTOR     scale_ = dx::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f); // ★ スケール追加

	float            current_roll_ = 0.0f;
	float            current_pitch_tilt_ = 0.0f;
	float            target_roll_ = 0.0f;

	int              animation_index_ = 0;
	float            animation_speed_ = 1.0f;
	bool             is_dead_ = false;

	// モデル設定
	std::string      model_name_ = "Spider";
	int              model_forward_axis_ = 1; // 0:+Z, 1:+X, 2:-Z, 3:-X

	// 移動パラメータ
	float            speed_ = 3.0f;
	float            vertical_speed_ = 5.0f;
	float            sprint_mult_ = 2.0f;
	float            yaw_follow_speed_ = 8.0f;
	float            tilt_speed_ = 8.0f;
	float            max_tilt_angle_ = 25.0f;
	float            eye_height_ = 0.0f;

	float            sphere_radius_ = 0.5f;
	bool             enable_collision_ = true;

	int              spotlight_index_ = -1;
	float            light_radius_ = 20.0f;
	float            light_intensity_ = 12.0f;
	float            light_inner_angle_ = 0.10f;
	float            light_outer_angle_ = 0.30f;
	float            light_height_offset_ = 0.8f;
	dx::XMFLOAT4     light_color_ = { 1.0f, 0.9f, 0.7f, 1.0f };
};