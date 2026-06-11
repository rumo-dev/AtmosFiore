#pragma once
#include "Engine/Graphics/UI/ImGui/imgui.h"
#include "Engine/Graphics/UI/ImGui/imgui_internal.h"
#include <string>
#include <map>

//namespace CustomUI {
//	struct Theme {
//		ImU32 bg = IM_COL32(20, 20, 25, 255);
//		ImU32 accent = IM_COL32(80, 120, 255, 255);
//		ImU32 text = IM_COL32(200, 200, 200, 255);
//		float rounding = 4.0f;
//	};
//	static Theme theme;
//
//	// セクションタイトル（見出し）
//	void SectionLabel(const char* label);
//
//	// テーマカラーに合わせたセパレーター
//	void Separator();
//	bool CustomSlider(const char* label, float* value, float min, float max, float step, const char* format);
//}

namespace CustomUI
{
	struct Theme {
		ImU32 bg = IM_COL32(20, 20, 25, 255);
		ImU32 accent = IM_COL32(80, 120, 255, 255);
		ImU32 text = IM_COL32(200, 200, 200, 255);
		float rounding = 4.0f;
	};
	static Theme theme;

	// セクションタイトル（見出し）
	void SectionLabel(const char* label);

	// テーマカラーに合わせたセパレーター
	void Separator();
	// -----------------------------------------------------------------
	// Button
	// Renders a rounded button with hover/active color from gui style.
	// -----------------------------------------------------------------
	bool Button(const char* label, const ImVec2& size_arg = ImVec2(0, 0));

	// -----------------------------------------------------------------
	// Checkbox
	// Toggle switch with animated knob; label on the left.
	// -----------------------------------------------------------------
	bool Checkbox(const char* label, bool* v);

	// -----------------------------------------------------------------
	// SliderScalar / SliderFloat / SliderInt
	// Thin track with accent fill and a value box on the right.
	// -----------------------------------------------------------------
	bool SliderScalar(const char* label,
		ImGuiDataType data_type,
		void* p_data,
		const void* p_min,
		const void* p_max,
		const char* format = nullptr,
		ImGuiSliderFlags flags = 0);

	bool SliderFloat(const char* label,
		float* v,
		float v_min,
		float v_max,
		const char* format = "%.1f",
		ImGuiSliderFlags flags = 0);

	bool SliderInt(const char* label,
		int* v,
		int v_min,
		int v_max,
		const char* format = "%d",
		ImGuiSliderFlags flags = 0);

	bool DragScalar(const char* label, ImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags = 0);
	bool DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags = 0);
	// -----------------------------------------------------------------
	// BeginCombo / EndCombo
	// Drop-down combo aligned to the right side of the window.
	// -----------------------------------------------------------------
	bool BeginCombo(const char* label,
		const char* preview_value,
		ImGuiComboFlags flags = 0);

	void EndCombo();

	// Convenience wrapper identical in signature to ImGui::Combo.
	bool Combo(const char* label,
		int* current_item,
		const char* const items[],
		int items_count,
		int height_in_items = -1);

	bool Combo(const char* label,
		int* current_item,
		const char* items_separated_by_zeros,
		int height_in_items = -1);

	// -----------------------------------------------------------------
	// InputText
	// Single-line input with animated frame highlight.
	// -----------------------------------------------------------------
	bool InputText(const char* label,
		char* buf,
		size_t buf_size,
		ImGuiInputTextFlags flags = 0,
		ImGuiInputTextCallback cb = nullptr,
		void* user_data = nullptr);

	// -----------------------------------------------------------------
	// ColorEdit4 / ColorEdit3
	// Color picker with custom popup styling.
	// -----------------------------------------------------------------
	bool ColorEdit4(const char* label,
		float col[4],
		ImGuiColorEditFlags flags = 0);

	bool ColorEdit3(const char* label,
		float col[3],
		ImGuiColorEditFlags flags = 0);

} // namespace CustomUI