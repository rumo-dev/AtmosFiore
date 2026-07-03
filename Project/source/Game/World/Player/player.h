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
		dx::XMVECTOR start_pos = dx::XMVectorSet(0.0f, 0.0f, 10.0f, 1.0f),
		int spotlight_index = -1)
	{
		_hwnd = hwnd;
		_position = start_pos;
		_spotlight_index = spotlight_index;
		_up_vector = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		_forward_vector = dx::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		_is_climbing = false;
		_is_dead = false;
		_animation_index = 0;
		_animation_speed = 1.0f;
		_idle_large_timer = 0.0f;
		_trigger_idle_large = false;
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
	//  ゲッター / セッター
	// ======================================================

	/// ワールド座標（w=1）
	dx::XMVECTOR get_position() const { return _position; }

	/// 頭部座標（First Person Camera用, 目の高さ分だけY+）
	dx::XMVECTOR get_head_position() const
	{
		// 蜘蛛の上方向（_up_vector）を考慮した頭部座標
		return dx::XMVectorAdd(_position, dx::XMVectorScale(_up_vector, _eye_height));
	}

	/// 前方ベクトル（正規化済み・Y成分なし水平）
	dx::XMVECTOR get_forward() const
	{
		float yaw_rad = dx::XMConvertToRadians(_yaw);
		return dx::XMVectorSet(sinf(yaw_rad), 0.0f, cosf(yaw_rad), 0.0f);
	}

	/// スポットライト配列インデックス（-1 で無効）
	int get_spotlight_index() const { return _spotlight_index; }

	/// プレイヤーのモデル用ワールド行列（右手系 glTF 用）
	dx::XMFLOAT4X4 get_world_transform() const
	{
		dx::XMVECTOR U = dx::XMVector3Normalize(_up_vector);
		dx::XMVECTOR F = dx::XMVector3Normalize(_forward_vector);
		dx::XMVECTOR R = dx::XMVector3Normalize(dx::XMVector3Cross(U, F));
		F = dx::XMVector3Cross(R, U); // 完全な直交化

		dx::XMMATRIX rot_matrix = dx::XMMatrixIdentity();
		rot_matrix.r[0] = R;
		rot_matrix.r[1] = U;
		rot_matrix.r[2] = F;

		// 座標系変換行列 (RH_Y_UP 用に X 軸反転)
		dx::XMMATRIX C = dx::XMLoadFloat4x4(&COORDINATE_SYSTEM_TABLE[0]);

		// 平行移動
		dx::XMMATRIX T = dx::XMMatrixTranslationFromVector(_position);

		// 合成（C * rot * T）
		dx::XMMATRIX M = C * rot_matrix * T;

		dx::XMFLOAT4X4 out;
		dx::XMStoreFloat4x4(&out, M);
		return out;
	}

	int get_animation_index() const { return _animation_index; }
	float get_animation_speed() const { return _animation_speed; }

	void set_dead(bool dead) { _is_dead = dead; }
	bool is_dead() const { return _is_dead; }
	void trigger_idle_large() { _trigger_idle_large = true; }

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
		ImGui::BulletText("State    : %s", _is_climbing ? "Climbing" : "Falling");
		ImGui::BulletText("Dead     : %s", _is_dead ? "Yes" : "No");

		dx::XMFLOAT3 uv;
		dx::XMStoreFloat3(&uv, _up_vector);
		ImGui::BulletText("UpVector : X:%.2f  Y:%.2f  Z:%.2f", uv.x, uv.y, uv.z);

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
		dx::XMVECTOR fwd = _forward_vector;
		dx::XMVECTOR base = dx::XMVectorAdd(_position, dx::XMVectorScale(_up_vector, _light_height_offset));
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
			sinf(pitch_rad),                   // Y（上下）
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
	 * @brief 蜘蛛としての移動処理（壁這い・重力・アニメーション選択）
	 * @param collision_triangles コリジョン用三角形リスト（nullptr で無効）
	 */
	void _handle_movement(float elapsed_time,
		const std::vector<dx::XMFLOAT3>* collision_triangles = nullptr)
	{
		// 死亡状態なら移動させず、死亡アニメーション再生
		if (_is_dead)
		{
			_animation_index = 3;
			_animation_speed = 1.0f;
			return;
		}

		float yaw_rad = dx::XMConvertToRadians(_yaw);

		// カメラ基準の前方・右方ベクトル（水平）
		dx::XMVECTOR cam_forward = dx::XMVectorSet(sinf(yaw_rad), 0.0f, cosf(yaw_rad), 0.0f);
		dx::XMVECTOR cam_right = dx::XMVectorSet(cosf(yaw_rad), 0.0f, -sinf(yaw_rad), 0.0f);

		dx::XMVECTOR input_move = dx::XMVectorZero();

		if (GetAsyncKeyState('W') & 0x8000) input_move = dx::XMVectorAdd(input_move, cam_forward);
		if (GetAsyncKeyState('S') & 0x8000) input_move = dx::XMVectorAdd(input_move, dx::XMVectorNegate(cam_forward));
		if (GetAsyncKeyState('D') & 0x8000) input_move = dx::XMVectorAdd(input_move, cam_right);
		if (GetAsyncKeyState('A') & 0x8000) input_move = dx::XMVectorAdd(input_move, dx::XMVectorNegate(cam_right));
		if (GetAsyncKeyState(VK_UP) & 0x8000) input_move = dx::XMVectorAdd(input_move, cam_forward);
		if (GetAsyncKeyState(VK_DOWN) & 0x8000) input_move = dx::XMVectorAdd(input_move, dx::XMVectorNegate(cam_forward));
		if (GetAsyncKeyState(VK_RIGHT) & 0x8000) input_move = dx::XMVectorAdd(input_move, cam_right);
		if (GetAsyncKeyState(VK_LEFT) & 0x8000) input_move = dx::XMVectorAdd(input_move, dx::XMVectorNegate(cam_right));

		float len = Length(input_move);
		bool is_moving = (len > 0.001f);

		dx::XMVECTOR move_dir = dx::XMVectorZero();
		if (is_moving)
		{
			input_move = dx::XMVector3Normalize(input_move);
		}

		dx::XMVECTOR target_up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		if (_is_climbing && collision_triangles && !collision_triangles->empty())
		{
			// 接地面上に移動入力を投影
			if (is_moving)
			{
				dx::XMVECTOR proj_move = dx::XMVectorSubtract(input_move,
					dx::XMVectorScale(_up_vector, dx::XMVectorGetX(dx::XMVector3Dot(input_move, _up_vector))));
				float proj_len = Length(proj_move);
				if (proj_len > 0.001f)
				{
					move_dir = dx::XMVector3Normalize(proj_move);
				}
				else
				{
					move_dir = input_move;
				}
			}

			bool sprint = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			float actual_speed = _speed * (sprint ? _sprint_mult : 1.0f);
			dx::XMVECTOR move_delta = dx::XMVectorScale(move_dir, actual_speed * elapsed_time);

			// 壁這い用カプセルの定義 (Upベクトル方向に軸を向ける)
			dx::XMVECTOR capsule_base = dx::XMVectorAdd(_position, dx::XMVectorScale(_up_vector, _capsule_radius));
			dx::XMVECTOR capsule_tip = dx::XMVectorAdd(_position, dx::XMVectorScale(_up_vector, _capsule_height - _capsule_radius));
			CollisionCapsule capsule{ capsule_base, capsule_tip, _capsule_radius };

			dx::XMVECTOR corrected_delta = move_delta;
			if (is_moving)
			{
				// 制限をかけない3次元の壁ずり
				CapsuleCastTriangles(capsule, move_delta, *collision_triangles, corrected_delta);
			}

			dx::XMVECTOR next_position = dx::XMVectorAdd(_position, corrected_delta);

			// 接地スナップ（お腹方向へレイキャスト）
			float offset = 0.5f;
			dx::XMVECTOR ray_origin = dx::XMVectorAdd(next_position, dx::XMVectorScale(_up_vector, offset));
			dx::XMVECTOR ray_dir = dx::XMVectorNegate(_up_vector);
			Ray ray{ ray_origin, ray_dir };

			RayHitResult hit_res;
			bool snap_success = false;

			// 通常接地判定
			if (RayCastTriangles(ray, *collision_triangles, offset + _ground_snap_dist, hit_res))
			{
				_position = dx::XMVectorAdd(hit_res.point, dx::XMVectorScale(hit_res.normal, 0.02f));
				target_up = hit_res.normal;
				snap_success = true;
			}
			// 凸角の回り込み判定（通常の接地が外れた場合）
			else if (is_moving)
			{
				dx::XMVECTOR edge_ray_origin = dx::XMVectorAdd(next_position, dx::XMVectorScale(_up_vector, 0.2f));
				dx::XMVECTOR edge_ray_dir = dx::XMVector3Normalize(dx::XMVectorAdd(dx::XMVectorNegate(_up_vector), dx::XMVectorScale(move_dir, 0.5f)));
				Ray edge_ray{ edge_ray_origin, edge_ray_dir };

				if (RayCastTriangles(edge_ray, *collision_triangles, _ground_snap_dist * 2.0f, hit_res))
				{
					_position = dx::XMVectorAdd(hit_res.point, dx::XMVectorScale(hit_res.normal, 0.02f));
					target_up = hit_res.normal;
					snap_success = true;
				}
				else
				{
					// 壁激突判定（目の前に壁がある場合の吸い付き）
					dx::XMVECTOR forward_ray_origin = dx::XMVectorAdd(_position, dx::XMVectorScale(_up_vector, _capsule_radius));
					Ray forward_ray{ forward_ray_origin, move_dir };
					if (RayCastTriangles(forward_ray, *collision_triangles, _capsule_radius * 2.0f, hit_res))
					{
						_position = dx::XMVectorAdd(hit_res.point, dx::XMVectorScale(hit_res.normal, 0.02f));
						target_up = hit_res.normal;
						snap_success = true;
					}
				}
			}

			if (snap_success)
			{
				_is_climbing = true;
			}
			else
			{
				// 接地消失：落下状態へ移行
				_is_climbing = false;
				_position = next_position;
				target_up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			}
		}
		else
		{
			// 落下中
			_position = dx::XMVectorAdd(_position, dx::XMVectorSet(0.0f, -_gravity * elapsed_time, 0.0f, 0.0f));

			if (is_moving)
			{
				bool sprint = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
				float actual_speed = _speed * (sprint ? _sprint_mult : 1.0f);
				dx::XMVECTOR move_delta = dx::XMVectorScale(input_move, actual_speed * elapsed_time);
				_position = dx::XMVectorAdd(_position, move_delta);
			}

			// 着地判定
			if (collision_triangles && !collision_triangles->empty())
			{
				dx::XMVECTOR ray_origin = dx::XMVectorAdd(_position, dx::XMVectorSet(0.0f, 0.5f, 0.0f, 0.0f));
				Ray ray{ ray_origin, dx::XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f) };
				RayHitResult hit_res;
				if (RayCastTriangles(ray, *collision_triangles, 0.5f + 0.1f, hit_res))
				{
					_position = dx::XMVectorSetY(_position, dx::XMVectorGetY(hit_res.point) + 0.02f);
					target_up = hit_res.normal;
					_is_climbing = true;
				}
			}
		}

		// --- 姿勢の更新 ---
		_up_vector = dx::XMVector3Normalize(dx::XMVectorLerp(_up_vector, target_up, _climb_lerp_speed * elapsed_time));

		if (is_moving && Length(move_dir) > 0.001f)
		{
			// 移動入力がある場合、その方向へスムーズに旋回
			_forward_vector = dx::XMVector3Normalize(dx::XMVectorLerp(_forward_vector, move_dir, 15.0f * elapsed_time));
		}
		else
		{
			// 移動入力がない場合、現在の前方向を接平面に直交化して維持
			dx::XMVECTOR dot_val = dx::XMVector3Dot(_forward_vector, _up_vector);
			dx::XMVECTOR orth_forward = dx::XMVectorSubtract(_forward_vector, dx::XMVectorScale(_up_vector, dx::XMVectorGetX(dot_val)));
			if (Length(orth_forward) > 0.001f)
			{
				_forward_vector = dx::XMVector3Normalize(orth_forward);
			}
			else
			{
				// 特異点回避
				dx::XMVECTOR global_fwd = dx::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
				dx::XMVECTOR dot_val2 = dx::XMVector3Dot(global_fwd, _up_vector);
				orth_forward = dx::XMVectorSubtract(global_fwd, dx::XMVectorScale(_up_vector, dx::XMVectorGetX(dot_val2)));
				if (Length(orth_forward) > 0.001f)
					_forward_vector = dx::XMVector3Normalize(orth_forward);
			}
		}

		// --- アニメーション状態の切り替え ---
		if (is_moving)
		{
			_animation_index = 2; // 歩き
			bool sprint = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			_animation_speed = sprint ? 1.5f : 1.0f;
		}
		else
		{
			// 待機中
			if (_trigger_idle_large)
			{
				_animation_index = 1; // 動きの大きい待機
				_animation_speed = 0.01f;
				_trigger_idle_large = false;
				_idle_large_timer = 2.0f; // アニメーション終了時間（仮）
			}
			else if (_idle_large_timer > 0.0f)
			{
				_idle_large_timer -= elapsed_time;
				if (_idle_large_timer <= 0.0f)
				{
					_animation_index = 0; // 通常待機に戻す
				}
			}
			else
			{
				_animation_index = 0; // 通常待機
				_animation_speed = 1.0f;
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

	// 姿勢ベクトル
	dx::XMVECTOR     _up_vector = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	dx::XMVECTOR     _forward_vector = dx::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	bool             _is_climbing = false;
	float            _climb_lerp_speed = 5.0f;
	float            _gravity = 9.8f;

	// アニメーション制御
	int              _animation_index = 0;
	float            _animation_speed = 1.0f;
	bool             _is_dead = false;
	float            _idle_large_timer = 0.0f;
	bool             _trigger_idle_large = false;

	// 移動パラメータ
	float            _speed = 5.0f;
	float            _sprint_mult = 2.0f;
	float            _eye_height = 0.5f;   ///< 蜘蛛なので目の高さを低めに

	// カプセルコリジョン
	float            _capsule_radius = 0.6f;  ///< 蜘蛛に合わせて半径を大きめにする
	float            _capsule_height = 0.8f;  ///< 蜘蛛に合わせて全高を低めにする
	bool             _enable_ground_snap = true; ///< 床スナップ有効
	float            _ground_snap_dist = 1.5f;

	// スポットライト
	int              _spotlight_index = -1;
	float            _light_radius = 15.0f;
	float            _light_intensity = 8.0f;
	float            _light_inner_angle = 0.15f;
	float            _light_outer_angle = 0.40f;
	float            _light_height_offset = 0.5f; ///< 蜘蛛に合わせてライトオフセットを低めに
	dx::XMFLOAT4     _light_color = { 1.0f, 0.95f, 0.85f, 1.0f };
};
