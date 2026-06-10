#pragma once
#include "light_base.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"
#include "Engine/Graphics/UI/ImGui/ImGuizmo.h"
#include "Game/World/camera/camera_manager.h" // カメラマネージャーを利用
#include "Engine/system/graphics_core.h"
#include <cmath>

class Directional_Light : public Light_Base
{
public:
	dx::XMFLOAT4 direction = { -0.5f, -1.0f, 0.5f, 0.0f }; // w=0

public:
	Directional_Light() = default;

	// 更新（GPU送信）
	void update() override
	{
		Graphics_Core::instance().set_directional_light(ambientColor, direction, diffuseColor);
	}

	void draw_imgui(const char* label = "DirectionalLight")
	{
		if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
			return;

		// --- パラメータ編集 ---
		// 変更があった場合は update() を呼ぶ構成を維持
		if (ImGui::ColorEdit3("Ambient", (float*)&ambientColor)) update();
		if (ImGui::ColorEdit3("Diffuse", (float*)&diffuseColor)) update();

		// ライトの向きをスライダー等で編集可能にする場合（ギズモの代わり）
		if (ImGui::SliderFloat3("Direction", (float*)&direction, -1.0f, 1.0f)) {
			// 正規化して更新
			dx::XMVECTOR dir = dx::XMLoadFloat4(&direction);
			dx::XMStoreFloat4(&direction, dx::XMVector3Normalize(dir));
			update();
		}
	}
};