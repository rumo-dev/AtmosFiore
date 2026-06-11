class Directional_Light : public Light_Base
{
public:
	dx::XMFLOAT4 direction = { 0.0f, 1.0f, 0.0f, 0.0f }; // w=0

private:
	// 内部管理用の角度（度数法）。初期状態（真上から真下）は Pitch=-90, Yaw=0
	float pitch = -90.0f;
	float yaw = 0.0f;

	// 角度からベクトルを再計算して更新するヘルパー
	void update_from_angles()
	{
		float p_rad = dx::XMConvertToRadians(pitch);
		float y_rad = dx::XMConvertToRadians(yaw);

		// ラジアンから方向ベクトルを計算（Y軸が上方向の座標系を想定）
		direction.x = cosf(p_rad) * sinf(y_rad);
		direction.y = sinf(p_rad);
		direction.z = cosf(p_rad) * cosf(y_rad);
		direction.w = 0.0f;

		update();
	}

public:
	Directional_Light() = default;

	void update() override
	{
		Graphics_Core::instance().set_directional_light(ambientColor, direction, diffuseColor);
	}

	void draw_imgui(const char* label = "DirectionalLight")
	{
		if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
			return;

		if (ImGui::ColorEdit3("Ambient", (float*)&ambientColor)) update();
		if (ImGui::ColorEdit3("Diffuse", (float*)&diffuseColor)) update();

		// --- 角度による直感的な操作 ---
		bool changed = false;
		if (ImGui::SliderFloat("Pitch (上下)", &pitch, -90.0f, 90.0f, "%.1f deg")) changed = true;
		if (ImGui::SliderFloat("Yaw (左右)", &yaw, -180.0f, 180.0f, "%.1f deg")) changed = true;

		if (changed) {
			update_from_angles();
		}

		// リセットボタンもあると便利
		if (ImGui::Button("Reset to Top-Down")) {
			pitch = -90.0f;
			yaw = 0.0f;
			update_from_angles();
		}
	}
};