#pragma once
#include "Engine/Graphics/UI/ImGui/imgui.h"
#include "Engine/Graphics/UI/ImGui/imgui_internal.h"
#include <string>
#include <map>

inline namespace CustomUI {
	inline static std::map<ImGuiID, float> anim_map;

	struct Theme {
		ImU32 bg = IM_COL32(20, 20, 25, 255);
		ImU32 accent = IM_COL32(80, 120, 255, 255);
		ImU32 text = IM_COL32(200, 200, 200, 255);
		float rounding = 4.0f;
	};
	inline static Theme theme;

	// --- トグル ---
	inline bool Toggle(const char* label, bool* v) {
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = g.CurrentWindow;
		ImGuiID id = window->GetID(label);
		float height = ImGui::GetFrameHeight();
		float width = height * 1.6f;
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));
		ImGui::ItemSize(bb, g.Style.FramePadding.y);
		if (!ImGui::ItemAdd(bb, id)) return false;

		bool pressed = ImGui::ButtonBehavior(bb, id, nullptr, nullptr);
		if (pressed) *v = !*v;
		float& anim = anim_map[id];
		anim = ImLerp(anim, *v ? 1.0f : 0.0f, 0.15f);

		window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(ImLerp(ImVec4(0.2f, 0.2f, 0.22f, 1.0f), ImVec4(0.3f, 0.5f, 1.0f, 1.0f), anim)), theme.rounding);
		window->DrawList->AddCircleFilled(ImVec2(ImLerp(bb.Min.x + height * 0.4f, bb.Max.x - height * 0.4f, anim), bb.Min.y + height * 0.5f), height * 0.3f, IM_COL32(255, 255, 255, 255));
		ImGui::SameLine(0, 10); ImGui::Text("%s", label);
		return pressed;
	}

	// --- スライダー ---
	inline bool Slider(const char* label, float* v, float min, float max) {
		ImGui::Text("%s", label);
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
		ImGui::PushID(label);
		bool changed = ImGui::SliderFloat("##", v, min, max, "%.0f%%");
		ImGui::PopID();
		return changed;
	}

	// --- コンボボックス ---
	inline bool Combo(const char* label, int* current, const char* items[], int count) {
		ImGui::Text("%s", label);
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
		ImGui::PushID(label);
		bool changed = ImGui::Combo("##", current, items, count);
		ImGui::PopID();
		return changed;
	}

	// --- TreeNode（折りたたみグループ） ---
	inline bool TreeNode(const char* label, bool* open) {
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = g.CurrentWindow;
		ImGuiID id = window->GetID(label);
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, 30.0f);
		ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
		ImGui::ItemSize(bb);
		if (!ImGui::ItemAdd(bb, id)) return false;

		if (ImGui::ButtonBehavior(bb, id, nullptr, nullptr)) *open = !*open;
		float& anim = anim_map[id];
		anim = ImLerp(anim, *open ? 1.0f : 0.0f, 0.15f);

		window->DrawList->AddRectFilled(bb.Min, bb.Max, IM_COL32(35, 35, 40, 255), theme.rounding);
		window->DrawList->AddText(ImVec2(bb.Min.x + 10, bb.Min.y + 7), theme.text, label);
		float angle = ImLerp(0.0f, 1.57f, anim);
		ImVec2 center = ImVec2(bb.Max.x - 15, bb.Min.y + 15.0f);
		window->DrawList->AddTriangleFilled(ImVec2(center.x - 4, center.y - 3), ImVec2(center.x + 4, center.y), ImVec2(center.x - 4, center.y + 3), IM_COL32(200, 200, 200, 255));
		return *open;
	}

	// --- 画像表示 ---
	inline void Image(ImTextureID texture_id, ImVec2 size) {
		ImGui::Image(texture_id, size);
	}

	// --- テキスト入力 ---
	inline bool InputText(const char* label, char* buf, size_t size) {
		ImGui::Text("%s", label);
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
		ImGui::PushID(label);
		bool changed = ImGui::InputText("##", buf, size, ImGuiInputTextFlags_CharsNoBlank);
		ImGui::PopID();
		return changed;
	}
	// セクションタイトル（見出し）
	inline void SectionLabel(const char* label) {
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f), "%s", label);
		ImGui::Separator();
		ImGui::Spacing();
	}

	// テーマカラーに合わせたセパレーター
	inline void Separator() {
		ImGui::Separator();
		ImGui::Spacing();
	}

}