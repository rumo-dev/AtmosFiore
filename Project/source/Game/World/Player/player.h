#pragma once
#include <Windows.h>
#include <DirectXMath.h>
#include <algorithm>
#include <vector>
#include "Engine/utilities/dx_common.h"
#include "Engine/utilities/math_util.h"
#include "Engine/utilities/collision_util.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"

namespace dx = DirectX;

/**
 * @brief プレイヤークラス
 *
 * @details
 * キーボード（WASD / 矢印キー）とマウス（右クリックドラッグ）で操作できる
 * キャラクターの位置・向きを管理するクラス。
 *
 * モデルは持たず、座標と向きのみを提供する。
 * カメラへの座標供給と、プレイヤー前方を照らすスポットライトの
 * 位置・方向の更新を担当する。
 *
 * @note
 * - Y軸（高さ）は固定（重力・コリジョンなし）
 * - カメラYaw（水平回転角）を移動方向の基準にする
 * - スポットライトはシーン側が add_light() で登録し、インデックスを渡す
 */
class Player
{
public:

	// ======================================================
	//  初期化 / 更新
	// ======================================================

	/**
	 * @brief 初期化処理
	 * @param hwnd ウィンドウハンドル（マウス座標変換に使用）
	 * @param start_pos 初期ワールド座標
	 * @param spotlight_index プレイヤーライトのスポットライト配列インデックス（-1 で無効）
	 */
	void initialize(HWND hwnd,
		dx::XMVECTOR start_pos = dx::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
		int spotlight_index = -1)
	{
		_hwnd = hwnd;
		_position = start_pos;
		_spotlight_index = spotlight_index;
	}

	/**
	 * @brief 毎フレーム更新
	 * @param elapsed_time 経過時間（秒）
	 * @param camera_yaw_deg   カメラの水平回転角（度）。移動方向の基準に使用する。
	 * @param camera_pitch_deg カメラの上下回転角（度）。スポットライトの上下方向に使用する。
	 * @param collision_triangles コリジョン用三角形頂点リスト（3頂点で1三角形）。nullptr で無効。
	 */
	void update(float elapsed_time, float camera_yaw_deg, float camera_pitch_deg = 0.0f,
		const std::vector<dx::XMFLOAT3>* collision_triangles = nullptr)
	{
		_yaw = camera_yaw_deg;
		_pitch = camera_pitch_deg;
		_handle_movement(elapsed_time, collision_triangles);
	}

	// ======================================================
	//  ゲッター
	// ======================================================

	/// ワールド座標（w=1）
	dx::XMVECTOR get_position() const { return _position; }

	/// 頭部座標（First Person Camera用, 目の高さ分だけY+）
	dx::XMVECTOR get_head_position() const
	{
		return dx::XMVectorAdd(_position, dx::XMVectorSet(0.0f, _eye_height, 0.0f, 0.0f));
	}

	/// 前方ベクトル（正規化済み・Y成分なし水平）
	dx::XMVECTOR get_forward() const
	{
		float yaw_rad = dx::XMConvertToRadians(_yaw);
		return dx::XMVectorSet(sinf(yaw_rad), 0.0f, cosf(yaw_rad), 0.0f);
	}

	/// スポットライト配列インデックス（-1 で無効）
	int get_spotlight_index() const { return _spotlight_index; }

	// ======================================================
	//  デバッグUI
	// ======================================================

	/**
	 * @brief ImGui デバッグウィンドウ描画
	 */
	void draw_imgui()
	{
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "--- Player Settings ---");

		ImGui::SliderFloat("Move Speed", &_speed, 0.5f, 20.0f, "%.1f m/s");
		ImGui::SliderFloat("Sprint Multiplier", &_sprint_mult, 1.0f, 5.0f, "%.1f x");
		ImGui::SliderFloat("Eye Height", &_eye_height, 0.5f, 3.0f, "%.2f m");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Collision:");
		ImGui::SliderFloat("Capsule Radius", &_capsule_radius, 0.1f, 1.0f, "%.2f m");
		ImGui::SliderFloat("Capsule Height", &_capsule_height, 0.5f, 3.0f, "%.2f m");
		ImGui::Checkbox("Enable Ground Snap", &_enable_ground_snap);
		if (_enable_ground_snap)
			ImGui::SliderFloat("Ground Snap Dist", &_ground_snap_dist, 0.1f, 3.0f, "%.2f m");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Status:");
		dx::XMFLOAT4 p;
		dx::XMStoreFloat4(&p, _position);
		ImGui::BulletText("Position : X:%.2f  Y:%.2f  Z:%.2f", p.x, p.y, p.z);
		ImGui::BulletText("Yaw      : %.1f deg", _yaw);
		ImGui::BulletText("Pitch    : %.1f deg", _pitch);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Spotlight:");
		ImGui::SliderFloat("Light Radius", &_light_radius, 0.5f, 50.0f, "%.1f");
		ImGui::SliderFloat("Light Intensity", &_light_intensity, 0.0f, 20.0f, "%.1f");
		ImGui::SliderFloat("Light Inner Angle", &_light_inner_angle, 0.01f, 1.5f, "%.2f rad");
		ImGui::SliderFloat("Light Outer Angle", &_light_outer_angle, 0.01f, 1.6f, "%.2f rad");
		ImGui::ColorEdit3("Light Color", (float*)&_light_color);
		ImGui::SliderFloat("Light Height Offset", &_light_height_offset, 0.0f, 3.0f, "%.2f m");
	}

	// ======================================================
	//  ライト情報取得（Scene 側で SpotLight を更新するために使用）
	// ======================================================

	/// スポットライト位置（頭部よりやや前）
	dx::XMFLOAT3 get_light_position() const
	{
		dx::XMVECTOR fwd = get_forward();
		dx::XMVECTOR base = dx::XMVectorAdd(_position, dx::XMVectorSet(0.0f, _light_height_offset, 0.0f, 0.0f));
		dx::XMVECTOR shifted = dx::XMVectorAdd(base, dx::XMVectorScale(fwd, 0.3f)); // 少し前にオフセット
		dx::XMFLOAT3 out;
		dx::XMStoreFloat3(&out, shifted);
		return out;
	}

	/// スポットライト方向（カメラが向いている3D方向）
	dx::XMFLOAT3 get_light_direction() const
	{
		float yaw_rad = dx::XMConvertToRadians(_yaw);
		float pitch_rad = dx::XMConvertToRadians(_pitch);

		// Yaw + Pitch から完全な3D向きベクトルを計算
		dx::XMVECTOR dir = dx::XMVectorSet(
			cosf(pitch_rad) * sinf(yaw_rad),   // X
			sinf(pitch_rad),                   // Y（上下山）
			cosf(pitch_rad) * cosf(yaw_rad),   // Z
			0.0f
		);
		dir = dx::XMVector3Normalize(dir);

		dx::XMFLOAT3 out;
		dx::XMStoreFloat3(&out, dir);
		return out;
	}


	float             get_light_radius()       const { return _light_radius; }
	float             get_light_intensity()    const { return _light_intensity; }
	float             get_light_inner_angle()  const { return _light_inner_angle; }
	float             get_light_outer_angle()  const { return _light_outer_angle; }
	dx::XMFLOAT4      get_light_color()        const { return _light_color; }

private:

	// ======================================================
	//  内部処理
	// ======================================================



	/**
	 * @brief WASD / 矢印キーによる水平移動（カプセルコリジョン付き）
	 * @param collision_triangles コリジョン用三角形リスト（nullptr で無効）
	 */
	void _handle_movement(float elapsed_time,
		const std::vector<dx::XMFLOAT3>* collision_triangles = nullptr)
	{
		float yaw_rad = dx::XMConvertToRadians(_yaw);

		// カメラ基準の前方・右方ベクトル（水平）
		dx::XMVECTOR forward = dx::XMVectorSet(sinf(yaw_rad), 0.0f, cosf(yaw_rad), 0.0f);
		dx::XMVECTOR right = dx::XMVectorSet(cosf(yaw_rad), 0.0f, -sinf(yaw_rad), 0.0f);

		dx::XMVECTOR move_dir = dx::XMVectorZero();

		if (GetAsyncKeyState('W') & 0x8000) move_dir = dx::XMVectorAdd(move_dir, forward);
		if (GetAsyncKeyState('S') & 0x8000) move_dir = dx::XMVectorAdd(move_dir, dx::XMVectorNegate(forward));
		if (GetAsyncKeyState('D') & 0x8000) move_dir = dx::XMVectorAdd(move_dir, right);
		if (GetAsyncKeyState('A') & 0x8000) move_dir = dx::XMVectorAdd(move_dir, dx::XMVectorNegate(right));
		if (GetAsyncKeyState(VK_UP) & 0x8000) move_dir = dx::XMVectorAdd(move_dir, forward);
		if (GetAsyncKeyState(VK_DOWN) & 0x8000) move_dir = dx::XMVectorAdd(move_dir, dx::XMVectorNegate(forward));
		if (GetAsyncKeyState(VK_RIGHT) & 0x8000) move_dir = dx::XMVectorAdd(move_dir, right);
		if (GetAsyncKeyState(VK_LEFT) & 0x8000) move_dir = dx::XMVectorAdd(move_dir, dx::XMVectorNegate(right));

		// 長さが 0 でなければ正規化して移動
		float len = Length(move_dir);
		if (len > 0.001f)
		{
			move_dir = dx::XMVector3Normalize(move_dir);

			bool sprint = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			float actual_speed = _speed * (sprint ? _sprint_mult : 1.0f);

			dx::XMVECTOR move_delta = dx::XMVectorScale(move_dir, actual_speed * elapsed_time);

			if (collision_triangles && !collision_triangles->empty())
			{
				// カプセル下端・上端スフィア中心を計算
				// base: 足元の一段上（radius 分浮かせる）
				// tip:  頭部（height - radius）
				dx::XMVECTOR capsule_base = dx::XMVectorAdd(
					_position,
					dx::XMVectorSet(0.0f, _capsule_radius, 0.0f, 0.0f));
				dx::XMVECTOR capsule_tip = dx::XMVectorAdd(
					_position,
					dx::XMVectorSet(0.0f, _capsule_height - _capsule_radius, 0.0f, 0.0f));

				CollisionCapsule capsule{ capsule_base, capsule_tip, _capsule_radius };

				dx::XMVECTOR corrected_delta;
				CapsuleCastTriangles(capsule, move_delta, *collision_triangles, corrected_delta);

				// Y成分はコリジョン補正から除外（水平移動のみ補正）
				corrected_delta = dx::XMVectorSetY(corrected_delta,
					dx::XMVectorGetY(move_delta)); // 元の Y を維持

				_position = dx::XMVectorAdd(_position, corrected_delta);
			}
			else
			{
				_position = dx::XMVectorAdd(_position, move_delta);
			}
		}

		// 床接地（GroundRayCast）
		if (_enable_ground_snap && len > 0.001f && collision_triangles && !collision_triangles->empty())
		{
			// 腰部あたりからレイを下ろす
			dx::XMVECTOR snap_origin = dx::XMVectorAdd(
				_position,
				dx::XMVectorSet(0.0f, _capsule_height * 0.5f, 0.0f, 0.0f));

			float ground_y;
			if (GroundRayCast(snap_origin, *collision_triangles, _ground_snap_dist + _capsule_height * 0.5f, ground_y))
			{
				// ground_y をプレイヤー足元の Y として設定
				float current_y = dx::XMVectorGetY(_position);
				float target_y = ground_y;
				// スムーズに近づける（急降下防止）
				float new_y = current_y + (target_y - current_y) * min(elapsed_time * 10.0f, 1.0f);
				_position = dx::XMVectorSetY(_position, new_y);
			}
		}
	}

	// ======================================================
	//  メンバ変数
	// ======================================================

	HWND             _hwnd = nullptr;

	dx::XMVECTOR     _position = dx::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	float            _yaw = 0.0f;   ///< 水平向き（度）
	float            _pitch = 0.0f;   ///< 上下向き（度）：ライト方向に使用

	// 移動パラメータ
	float            _speed = 5.0f;
	float            _sprint_mult = 2.0f;
	float            _eye_height = 1.7f;   ///< 目の高さ（m）

	// カプセルコリジョン
	float            _capsule_radius = 0.4f;  ///< カプセル半径（m）
	float            _capsule_height = 1.8f;  ///< カプセル全高（m）
	bool             _enable_ground_snap = false; ///< 床スナップ有効フラグ
	float            _ground_snap_dist = 1.0f;  ///< 床スナップ探索距離（m）

	// スポットライト
	int              _spotlight_index = -1;
	float            _light_radius = 15.0f;
	float            _light_intensity = 8.0f;
	float            _light_inner_angle = 0.15f;
	float            _light_outer_angle = 0.40f;
	float            _light_height_offset = 1.5f; ///< ライト発生位置のY+オフセット
	dx::XMFLOAT4     _light_color = { 1.0f, 0.95f, 0.85f, 1.0f }; ///< 暖色系白
};
