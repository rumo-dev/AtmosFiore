#pragma once
#include "Engine/Graphics/UI/DebugMenu/CustomWidgets.h"
class Directional_Light : public Light_Base
{
public:
	dx::XMFLOAT4 direction = { 0,1,0,0 };

private:
	float pitch = -90.0f;
	float yaw = 0.0f;

	// ===== 時間管理 =====
	float time_of_day = 12.0f;     // 0～24
	float day_duration = 60.0f;    // 60秒で1日
	bool auto_rotate = true;

private:

	void update_from_angles()
	{
		float p = dx::XMConvertToRadians(pitch);
		float y = dx::XMConvertToRadians(yaw);

		direction.x = cosf(p) * sinf(y);
		direction.y = sinf(p);
		direction.z = cosf(p) * cosf(y);
		direction.w = 0.0f;

		update();
	}

	void update_from_time()
	{
		float theta =
			dx::XMConvertToRadians(
				(time_of_day / 24.0f) * 360.0f - 90.0f);

		// 太陽位置
		dx::XMFLOAT3 sun_pos =
		{
			cosf(theta),
			sinf(theta),
			-0.3f
		};

		auto v = dx::XMVector3Normalize(
			dx::XMLoadFloat3(&sun_pos));

		dx::XMFLOAT3 s;
		dx::XMStoreFloat3(&s, v);

		// DirectionalLight は光が進む向き
		direction =
		{
			-s.x,
			-s.y,
			-s.z,
			0.0f
		};

		update();
	}

public:

	void tick(float delta_time)
	{
		if (!auto_rotate)
			return;

		time_of_day += delta_time * (24.0f / day_duration);

		if (time_of_day >= 24.0f)
			time_of_day -= 24.0f;

		update_from_time();
	}

	void update() override
	{
		Graphics_Core::instance().set_directional_light(
			ambientColor,
			direction,
			diffuseColor
		);

	}

	void draw_imgui(const char* label = "DirectionalLight")
	{

		CustomUI::SectionLabel(label);

		if (CustomUI::ColorEdit3("Ambient", (float*)&ambientColor))
			update();

		if (CustomUI::ColorEdit3("Diffuse", (float*)&diffuseColor))
			update();

		CustomUI::Checkbox("Auto Day Cycle", &auto_rotate);

		if (CustomUI::SliderFloat("Time", &time_of_day, 0.0f, 24.0f, "%.1f h"))
		{
			update_from_time();
		}

		ImGui::SliderFloat("Day Duration(sec)", &day_duration, 10.0f, 300.0f);

		CustomUI::Separator();

		// 自動回転中は手動操作できないように無効化
		if (auto_rotate)
		{
			ImGui::BeginDisabled();
		}

		CustomUI::SectionLabel("Manual Rotation");

		bool changed = false;

		// =========================================================================
		// ヨーとピッチを横並びにするグループ
		// =========================================================================
		ImGui::BeginGroup();

		// ---- 1. Pitch ダイヤル (UI上は -90～90、0で真上) ----
		float pitch_ui_deg = pitch + 90.0f;
		float pitch_ui_rad = dx::XMConvertToRadians(pitch_ui_deg);

		if (CustomUI::RotationDial("Pitch (上下)", &pitch_ui_rad, 30.0f, -1.570796f, 1.570796f))
		{
			pitch_ui_deg = dx::XMConvertToDegrees(pitch_ui_rad);
			pitch = pitch_ui_deg - 90.0f;
			changed = true; // 変更フラグを立てる
		}

		// 横並びの間隔。ダイヤルのサイズ＋数値テキストの幅を考慮して少し広め（60px～80px程度）に設定
		ImGui::SameLine(0, 70.0f);

		// ---- 2. Yaw ダイヤル (UI・内部ともに -180～180の360度全方位) ----
		float yaw_rad = dx::XMConvertToRadians(yaw);

		if (CustomUI::RotationDial("Yaw (左右)", &yaw_rad, 30.0f, -3.14159265f, 3.14159265f))
		{
			yaw = dx::XMConvertToDegrees(yaw_rad);
			changed = true; // ★ここが漏れているとヨーを回しても光源が更新されません
		}

		ImGui::EndGroup();
		// =========================================================================

		// ピッチ、またはヨーのどちらかが動かされたら、まとめて方向ベクトルを再計算
		if (changed)
		{
			update_from_angles();
		}

		if (auto_rotate)
		{
			ImGui::EndDisabled();
		}
	}
};