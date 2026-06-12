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
	bool auto_rotate = false;

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
		if (!ImGui::CollapsingHeader(label,
			ImGuiTreeNodeFlags_DefaultOpen))
			return;

		if (ImGui::ColorEdit3("Ambient", (float*)&ambientColor))
			update();

		if (ImGui::ColorEdit3("Diffuse", (float*)&diffuseColor))
			update();

		ImGui::Checkbox("Auto Day Cycle", &auto_rotate);

		if (ImGui::SliderFloat(
			"Time",
			&time_of_day,
			0.0f,
			24.0f,
			"%.1f h"))
		{
			update_from_time();
		}

		ImGui::SliderFloat(
			"Day Duration(sec)",
			&day_duration,
			10.0f,
			300.0f);

		bool changed = false;

		if (!auto_rotate)
		{
			if (ImGui::SliderFloat(
				"Pitch",
				&pitch,
				-90,
				90))
				changed = true;

			if (ImGui::SliderFloat(
				"Yaw",
				&yaw,
				-180,
				180))
				changed = true;

			if (changed)
				update_from_angles();
		}
	}
};