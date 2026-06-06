#pragma once
#include "Engine/Graphics/UI/ImGui/imgui.h"
#include <string>

namespace ImGui_Util
{
	// -----------------------------
	// テーマ定義
	// -----------------------------
	enum class ThemeType {
		seeing,
		dark,
		light,
		classic,
		cherry,
		dracula,
		monokai,
		material_dark,
		tokyo_night,
		cyberpunk,
		solarized_light,
		solarized_dark
	};

	// -----------------------------
	// 既存テーマ: Seeing
	// -----------------------------
	inline void set_seeing_theme() {
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowPadding = ImVec2(12, 8);
		style.FramePadding = ImVec2(8, 5);
		style.ItemSpacing = ImVec2(10, 8);
		style.ItemInnerSpacing = ImVec2(6, 4);
		style.IndentSpacing = 22.0f;
		style.ScrollbarSize = 16.0f;
		style.GrabMinSize = 10.0f;
		style.WindowRounding = 8.0f;
		style.ChildRounding = 6.0f;
		style.FrameRounding = 6.0f;
		style.PopupRounding = 6.0f;
		style.ScrollbarRounding = 8.0f;
		style.GrabRounding = 6.0f;
		style.TabRounding = 6.0f;
		style.WindowBorderSize = 1.0f;
		style.FrameBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.TabBorderSize = 0.0f;

		style.WindowTitleAlign = ImVec2(0.5f, 0.5f);

		ImVec4* c = style.Colors;
		c[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
		c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
		c[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
		c[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
		c[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.23f, 0.94f);
		c[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.25f, 0.70f);
		c[ImGuiCol_FrameBg] = ImVec4(0.22f, 0.23f, 0.27f, 1.00f);
		c[ImGuiCol_FrameBgHovered] = ImVec4(0.30f, 0.32f, 0.37f, 1.00f);
		c[ImGuiCol_FrameBgActive] = ImVec4(0.36f, 0.39f, 0.46f, 1.00f);
		c[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
		c[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
		c[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
		c[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.80f, 0.38f, 1.00f);
		c[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.50f, 0.94f, 1.00f);
		c[ImGuiCol_Button] = ImVec4(0.24f, 0.50f, 0.94f, 0.70f);
		c[ImGuiCol_ButtonHovered] = ImVec4(0.34f, 0.60f, 0.98f, 1.00f);
		c[ImGuiCol_Header] = ImVec4(0.26f, 0.80f, 0.38f, 0.55f);
		c[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
		c[ImGuiCol_ResizeGripActive] = ImVec4(0.24f, 0.50f, 0.94f, 1.00f);
		c[ImGuiCol_Tab] = ImVec4(0.18f, 0.35f, 0.58f, 0.86f);
	}

	// -----------------------------
	// 他テーマ
	// -----------------------------
	inline void set_dark_theme() { ImGui::StyleColorsDark(); }
	inline void set_light_theme() { ImGui::StyleColorsLight(); }
	inline void set_classic_theme() { ImGui::StyleColorsClassic(); }

	// Cherry
	inline void set_cherry_theme() {
		ImGuiStyle& s = ImGui::GetStyle();
		s.WindowRounding = 5;
		ImVec4* c = s.Colors;
		c[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
		c[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.14f, 0.17f, 1.0f);
		c[ImGuiCol_Button] = ImVec4(0.55f, 0.10f, 0.10f, 1.00f);
		c[ImGuiCol_ButtonHovered] = ImVec4(0.70f, 0.15f, 0.15f, 1.00f);
		c[ImGuiCol_ButtonActive] = ImVec4(0.90f, 0.20f, 0.20f, 1.00f);
	}

	// Dracula
	inline void set_dracula_theme() {
		ImGuiStyle& s = ImGui::GetStyle();
		s.WindowRounding = 6; s.FrameRounding = 4;
		ImVec4* c = s.Colors;

		c[ImGuiCol_Text] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
		c[ImGuiCol_WindowBg] = ImVec4(0.14f, 0.16f, 0.23f, 1.00f);
		c[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.22f, 0.30f, 1.00f);

		c[ImGuiCol_Button] = ImVec4(0.45f, 0.24f, 0.59f, 1.00f);
		c[ImGuiCol_ButtonHovered] = ImVec4(0.55f, 0.34f, 0.69f, 1.00f);

		c[ImGuiCol_Header] = ImVec4(0.45f, 0.24f, 0.59f, 1.00f);
		c[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.23f, 0.35f, 1.00f);
	}

	// Monokai
	inline void set_monokai_theme() {
		ImGuiStyle& s = ImGui::GetStyle();
		s.WindowRounding = 4; s.FrameRounding = 3;
		ImVec4* c = s.Colors;

		c[ImGuiCol_Text] = ImVec4(0.90f, 0.89f, 0.85f, 1.00f);
		c[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);

		c[ImGuiCol_Button] = ImVec4(0.80f, 0.30f, 0.10f, 1.00f);
		c[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 0.40f, 0.15f, 1.00f);

		c[ImGuiCol_Header] = ImVec4(0.60f, 0.40f, 0.10f, 1.00f);
	}

	// Material Dark
	inline void set_material_dark_theme() {
		ImGuiStyle& s = ImGui::GetStyle();
		s.WindowRounding = 6; s.FrameRounding = 4;
		ImVec4* c = s.Colors;

		c[ImGuiCol_Text] = ImVec4(0.90f, 0.91f, 0.92f, 1.00f);
		c[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);

		c[ImGuiCol_Button] = ImVec4(0.24f, 0.56f, 0.90f, 1.00f);
		c[ImGuiCol_ButtonHovered] = ImVec4(0.34f, 0.66f, 0.95f, 1.00f);
	}

	// Tokyo Night
	inline void set_tokyo_night_theme() {
		ImGuiStyle& s = ImGui::GetStyle();
		s.WindowRounding = 6; s.FrameRounding = 5;
		ImVec4* c = s.Colors;

		c[ImGuiCol_Text] = ImVec4(0.80f, 0.85f, 0.88f, 1.00f);
		c[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.13f, 0.18f, 1.00f);

		c[ImGuiCol_Button] = ImVec4(0.26f, 0.45f, 0.70f, 1.00f);
		c[ImGuiCol_ButtonHovered] = ImVec4(0.36f, 0.55f, 0.80f, 1.00f);

		c[ImGuiCol_Header] = ImVec4(0.26f, 0.35f, 0.55f, 1.00f);
	}

	// Cyberpunk
	inline void set_cyberpunk_theme() {
		ImGuiStyle& s = ImGui::GetStyle();
		s.WindowRounding = 3; s.FrameRounding = 2;
		ImVec4* c = s.Colors;

		c[ImGuiCol_Text] = ImVec4(0.90f, 1.00f, 0.90f, 1.00f);
		c[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.02f, 0.05f, 1.00f);

		c[ImGuiCol_Button] = ImVec4(0.95f, 0.85f, 0.10f, 1.00f);
		c[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 0.95f, 0.20f, 1.00f);

		c[ImGuiCol_Header] = ImVec4(0.10f, 0.50f, 0.60f, 1.00f);
	}

	// Solarized Light
	inline void set_solarized_light_theme() {
		ImGuiStyle& s = ImGui::GetStyle();
		s.WindowRounding = 4; s.FrameRounding = 3;
		ImVec4* c = s.Colors;

		c[ImGuiCol_Text] = ImVec4(0.35f, 0.45f, 0.45f, 1.00f);
		c[ImGuiCol_WindowBg] = ImVec4(0.99f, 0.96f, 0.89f, 1.00f);

		c[ImGuiCol_Button] = ImVec4(0.15f, 0.55f, 0.82f, 1.00f);
		c[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.65f, 0.90f, 1.00f);

		c[ImGuiCol_Header] = ImVec4(0.80f, 0.60f, 0.41f, 1.00f);
	}

	// Solarized Dark
	inline void set_solarized_dark_theme() {
		ImGuiStyle& s = ImGui::GetStyle();
		s.WindowRounding = 4; s.FrameRounding = 3;
		ImVec4* c = s.Colors;

		c[ImGuiCol_Text] = ImVec4(0.76f, 0.86f, 0.84f, 1.00f);
		c[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.17f, 0.21f, 1.00f);

		c[ImGuiCol_Button] = ImVec4(0.15f, 0.55f, 0.82f, 1.00f);
		c[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.65f, 0.92f, 1.00f);

		c[ImGuiCol_Header] = ImVec4(0.13f, 0.41f, 0.52f, 1.00f);
	}

	// -----------------------------
	// set_theme
	// -----------------------------
	inline void set_theme(ThemeType t) {
		switch (t) {
		case ThemeType::seeing:            set_seeing_theme(); break;
		case ThemeType::dark:              set_dark_theme(); break;
		case ThemeType::light:             set_light_theme(); break;
		case ThemeType::classic:           set_classic_theme(); break;
		case ThemeType::cherry:            set_cherry_theme(); break;
		case ThemeType::dracula:           set_dracula_theme(); break;
		case ThemeType::monokai:           set_monokai_theme(); break;
		case ThemeType::material_dark:     set_material_dark_theme(); break;
		case ThemeType::tokyo_night:       set_tokyo_night_theme(); break;
		case ThemeType::cyberpunk:         set_cyberpunk_theme(); break;
		case ThemeType::solarized_light:   set_solarized_light_theme(); break;
		case ThemeType::solarized_dark:    set_solarized_dark_theme(); break;
		}
	}

	// -----------------------------
	// テーマ選択 UI
	// -----------------------------
	inline void draw_theme_selector(ThemeType& current_theme) {
		static const char* names[] = {
			"seeing",
			"dark",
			"light",
			"classic",
			"cherry",
			"dracula",
			"monokai",
			"material_dark",
			"tokyo_night",
			"cyberpunk",
			"solarized_light",
			"solarized_dark"
		};

		int cur = (int)current_theme;
		if (ImGui::Combo("Theme", &cur, names, IM_ARRAYSIZE(names))) {
			current_theme = (ThemeType)cur;
			set_theme(current_theme);
		}
	}

	// -----------------------------
	// UI Utility
	// -----------------------------
	inline bool CenteredButton(const char* label, float width = 120.0f) {
		float avail = ImGui::GetContentRegionAvail().x;
		float offset = (avail - width) * 0.5f;
		if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
		return ImGui::Button(label, ImVec2(width, 0));
	}

	inline bool ToggleButton(const char* label, bool* v) {
		ImVec2 size(50, 20);
		ImGui::PushID(label);
		bool clicked = ImGui::Button(*v ? "ON" : "OFF", size);
		if (clicked) *v = !*v;
		ImGui::SameLine();
		ImGui::Text("%s", label);
		ImGui::PopID();
		return clicked;
	}

	inline void BeginSection(const char* title, float pad = 8.0f) {
		ImGui::Separator(); ImGui::Spacing();
		ImGui::Text("%s", title);
		ImGui::Spacing(); ImGui::Dummy(ImVec2(0, pad)); ImGui::Indent();
	}

	inline void EndSection() {
		ImGui::Unindent(); ImGui::Dummy(ImVec2(0, 4)); ImGui::Separator();
	}

	inline void OpenCenteredWindow(const char* name, ImVec2 size) {
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 pos((io.DisplaySize.x - size.x) * 0.5f,
			(io.DisplaySize.y - size.y) * 0.5f);
		ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(size, ImGuiCond_Always);
		ImGui::Begin(name, nullptr,
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse);
	}

	inline void CloseCenteredWindow() { ImGui::End(); }

	inline void PushFontScale(float scale) { ImGui::SetWindowFontScale(scale); }

	inline void Spacer(float height = 8.0f) { ImGui::Dummy(ImVec2(0, height)); }

	inline bool SliderFloatLabeled(const char* label, float* v, float min, float max, const char* fmt = "%.2f") {
		ImGui::Text("%s", label);
		ImGui::SameLine();
		return ImGui::SliderFloat(("##" + std::string(label)).c_str(), v, min, max, fmt);
	}
}
