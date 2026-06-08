#pragma once
#include "Engine/Graphics/UI/ImGui/imgui.h"
#include "Engine/Graphics/UI/ImGui/imgui_internal.h"
#include <string>

namespace CustomWidgets {
	// モダンなトグルスイッチ
	bool Toggle(const char* label, bool* v) {
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems) return false;

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;
		const ImGuiID id = window->GetID(label);
		const float height = ImGui::GetFrameHeight();
		const float width = height * 1.6f;
		const float radius = height * 0.45f;

		const ImVec2 pos = ImGui::GetCursorScreenPos();
		const ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));
		ImGui::ItemSize(bb, style.FramePadding.y);
		if (!ImGui::ItemAdd(bb, id)) return false;

		bool hovered, held;
		bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
		if (pressed) *v = !(*v);

		// 描画
		ImU32 col_bg = *v ? IM_COL32(60, 140, 255, 255) : IM_COL32(50, 50, 55, 255);
		window->DrawList->AddRectFilled(bb.Min, bb.Max, col_bg, height * 0.5f);

		float knob_x = *v ? (bb.Max.x - radius - 2) : (bb.Min.x + radius + 2);
		window->DrawList->AddCircleFilled(ImVec2(knob_x, bb.Min.y + height * 0.5f), radius - 1, IM_COL32(255, 255, 255, 255));

		ImGui::SameLine(0, 10);
		ImGui::Text("%s", label);
		return pressed;
	}

	// モダンなスライダー（Neverlose風）
	bool Slider(const char* label, float* v, float v_min, float v_max) {
		ImGui::Text("%s", label);
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
		return ImGui::SliderFloat(("##" + std::string(label)).c_str(), v, v_min, v_max, "%.1f");
	}
}