#include "CustomWidgets.h"
#include <unordered_map>
#include <cmath>

// =========================================================================
// Internal helpers
// =========================================================================
namespace
{
	float& GetAnim(ImGuiID id)
	{
		static std::unordered_map<ImGuiID, float> s_map;
		return s_map[id];
	}
}

// =========================================================================
// 装飾系コントロール
// =========================================================================
void CustomUI::SectionLabel(const char* label) {
	ImGui::Spacing();
	ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f), "%s", label);
	ImGui::Separator();
	ImGui::Spacing();
}

void CustomUI::Separator() {
	ImGui::Separator();
	ImGui::Spacing();
}

void CustomUI::GradientHeader(const char* label, bool* is_open) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	ImDrawList* draw = window->DrawList;
	const ImVec2 pos = window->DC.CursorPos;

	const float width = ImGui::GetContentRegionAvail().x;
	const float height = ImGui::CalcTextSize(label).y + style.FramePadding.y * 2.0f;
	const ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));

	ImGui::ItemSize(bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(bb, id)) return;

	bool hovered, held;
	if (ImGui::ButtonBehavior(bb, id, &hovered, &held)) {
		if (is_open) *is_open = !(*is_open);
	}

	// 左から右へのグラデーション背景描画
	ImU32 col_base = ImGui::GetColorU32(ImGuiCol_FrameBg);
	ImU32 col_fade = col_base & 0x00FFFFFF; // アルファを0にする
	draw->AddRectFilledMultiColor(bb.Min, bb.Max, col_base, col_fade, col_fade, col_base);

	// 開閉を表す三角矢印の描画
	float arrow_size = g.FontSize * 0.75f;
	ImGuiDir dir = (is_open && *is_open) ? ImGuiDir_Down : ImGuiDir_Right;
	ImGui::RenderArrow(draw, ImVec2(bb.Min.x + style.FramePadding.x, bb.Min.y + style.FramePadding.y + 2.0f), ImGui::GetColorU32(ImGuiCol_Text), dir, arrow_size);

	// テキストの描画
	draw->AddText(ImVec2(bb.Min.x + style.FramePadding.x * 2.0f + arrow_size, bb.Min.y + style.FramePadding.y), ImGui::GetColorU32(ImGuiCol_Text), label);
}

// =========================================================================
// 基本コントロール
// =========================================================================
bool CustomUI::Button(const char* label, const ImVec2& size_arg) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

	ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);
	const ImVec2 pos = window->DC.CursorPos;
	const ImRect bb(pos.x, pos.y, pos.x + size.x, pos.y + size.y);

	ImGui::ItemSize(size, style.FramePadding.y);
	if (!ImGui::ItemAdd(bb, id)) return false;

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

	const ImU32 col = ImGui::GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
	ImGui::RenderNavHighlight(bb, id);
	ImGui::RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);

	const ImVec2 text_pos = ImVec2(bb.Min.x + (size.x - label_size.x) * 0.5f, bb.Min.y + (size.y - label_size.y) * 0.5f);
	window->DrawList->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), label);

	return pressed;
}

bool CustomUI::Checkbox(const char* label, bool* v) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return false;

	ImDrawList* draw = window->DrawList;
	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

	const float w = ImGui::GetContentRegionAvail().x;
	const ImVec2 sz = { 30.f, 17.f };
	const ImVec2 pos = window->DC.CursorPos;

	const ImRect frame_bb(pos.x + w - sz.x, pos.y, pos.x + w, pos.y + sz.y);
	const ImRect total_bb(pos.x, pos.y, pos.x + w, pos.y + ImMax(sz.y, label_size.y));

	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, id)) return false;

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(frame_bb, id, &hovered, &held);
	if (pressed) {
		*v = !(*v);
		ImGui::MarkItemEdited(id);
	}

	float& anim = GetAnim(id);
	anim = ImLerp(anim, *v ? 1.0f : 0.0f, 0.08f);

	const ImU32 text_col = ImGui::GetColorU32(*v ? ImGuiCol_Text : ImGuiCol_TextDisabled);
	draw->AddText(ImVec2(total_bb.Min.x, frame_bb.Min.y + (sz.y - label_size.y) * 0.5f), text_col, label);

	const ImU32 col_bg = ImGui::GetColorU32(ImLerp(ImVec4(0.2f, 0.2f, 0.2f, 1.0f), ImVec4(0.0f, 0.5f, 1.0f, 1.0f), anim));
	draw->AddRectFilled(frame_bb.Min, frame_bb.Max, col_bg, sz.y * 0.5f);
	draw->AddRect(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), sz.y * 0.5f);

	const float knob_radius = (sz.y - 4.0f) * 0.5f;
	const ImVec2 knob_pos = ImVec2(frame_bb.Min.x + knob_radius + 2.0f + (sz.x - knob_radius * 2.0f - 4.0f) * anim, frame_bb.GetCenter().y);
	draw->AddCircleFilled(knob_pos, knob_radius, ImGui::GetColorU32(ImGuiCol_Text), 20);

	return pressed;
}

// =========================================================================
// 数値スライダー系
// =========================================================================
bool CustomUI::SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	const ImVec2 pos = window->DC.CursorPos;
	const float w = ImGui::GetContentRegionAvail().x;

	const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
	const float widget_height = 20.0f;
	const float track_height = 4.0f;
	const float slider_right_offset = 60.0f;
	const float start_y = pos.y + label_size.y + 4.0f;
	const float slider_y_center = start_y + (widget_height * 0.5f);

	const ImRect frame_bb(pos.x, slider_y_center - (track_height * 0.5f), pos.x + w - slider_right_offset, slider_y_center + (track_height * 0.5f));
	const ImRect number_bb(frame_bb.Max.x + 8.0f, start_y, pos.x + w, start_y + widget_height);
	const ImRect total_bb(pos.x, pos.y, pos.x + w, start_y + widget_height);

	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, id, &total_bb)) return false;

	ImGuiStorage* storage = ImGui::GetStateStorage();
	bool is_editing = (storage->GetInt(id, 0) != 0);

	if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(number_bb.Min, number_bb.Max)) {
		storage->SetInt(id, 1);
		is_editing = true;
	}

	bool value_changed = false;

	if (is_editing) {
		ImGui::SetCursorScreenPos(number_bb.Min);
		ImGui::PushItemWidth(number_bb.GetWidth());
		char buf[64];
		ImGui::DataTypeFormatString(buf, 64, data_type, p_data, format);

		if (ImGui::InputText("##edit", buf, 64, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsDecimal)) {
			if (data_type == ImGuiDataType_Float) *(float*)p_data = (float)atof(buf);
			if (data_type == ImGuiDataType_S32)   *(int*)p_data = atoi(buf);
			storage->SetInt(id, 0);
			value_changed = true;
		}
		if (ImGui::IsItemDeactivatedAfterEdit()) storage->SetInt(id, 0);
		ImGui::PopItemWidth();
	}
	else {
		bool hovered, held;
		ImGui::ButtonBehavior(total_bb, id, &hovered, &held);

		ImRect grab_bb;
		if (ImGui::SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, format, flags, &grab_bb)) {
			ImGui::MarkItemEdited(id);
			value_changed = true;
		}

		window->DrawList->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), label);
		window->DrawList->AddRectFilled(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), track_height * 0.5f);
		float& anim = GetAnim(id);
		anim = ImLerp(anim, grab_bb.GetCenter().x, 0.2f);
		window->DrawList->AddRectFilled(frame_bb.Min, ImVec2(anim, frame_bb.Max.y), ImGui::GetColorU32(held ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), track_height * 0.5f);
		window->DrawList->AddCircleFilled(ImVec2(anim, slider_y_center), 6.0f, ImGui::GetColorU32(ImGuiCol_SliderGrab));

		char val_buf[64];
		ImGui::DataTypeFormatString(val_buf, 64, data_type, p_data, format);
		window->DrawList->AddRectFilled(number_bb.Min, number_bb.Max, ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), 4.0f);
		window->DrawList->AddRect(number_bb.Min, number_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), 4.0f);
		window->DrawList->AddText(ImVec2(number_bb.Min.x + 4, number_bb.Min.y + 2), ImGui::GetColorU32(ImGuiCol_Text), val_buf);
	}
	return value_changed;
}

bool CustomUI::SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
	return SliderScalar(label, ImGuiDataType_Float, v, &v_min, &v_max, format, flags);
}

bool CustomUI::SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
	return SliderScalar(label, ImGuiDataType_S32, v, &v_min, &v_max, format, flags);
}

bool CustomUI::DragScalar(const char* label, ImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	ImDrawList* draw = window->DrawList;
	const ImVec2 pos = window->DC.CursorPos;
	const float w = ImGui::GetContentRegionAvail().x;
	const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

	const float frame_height = 20.0f;
	const float frame_y_offset = label_size.y + 4.0f;

	const ImRect frame_bb(pos.x, pos.y + frame_y_offset, pos.x + w, pos.y + frame_y_offset + frame_height);
	const ImRect total_bb(pos.x, pos.y, pos.x + w, frame_bb.Max.y + style.FramePadding.y);

	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, id, &frame_bb)) return false;

	bool hovered, held;
	ImGui::ButtonBehavior(frame_bb, id, &hovered, &held);

	if (format == nullptr) format = ImGui::DataTypeGetInfo(data_type)->PrintFmt;

	const bool value_changed = ImGui::DragBehavior(id, data_type, p_data, v_speed, p_min, p_max, format, flags);
	if (value_changed) ImGui::MarkItemEdited(id);

	draw->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), label);
	const ImU32 frame_col = ImGui::GetColorU32(held ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
	draw->AddRectFilled(frame_bb.Min, frame_bb.Max, frame_col, style.FrameRounding);
	draw->AddRect(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding);

	char value_buf[64];
	ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);
	const ImVec2 text_size = ImGui::CalcTextSize(value_buf);
	const ImVec2 text_pos = ImVec2(frame_bb.Min.x + (frame_bb.GetWidth() - text_size.x) * 0.5f, frame_bb.Min.y + (frame_bb.GetHeight() - text_size.y) * 0.5f);
	draw->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), value_buf);

	return value_changed;
}

bool CustomUI::DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
	return DragScalar(label, ImGuiDataType_Float, v, v_speed, &v_min, &v_max, format, flags);
}

// =========================================================================
// コンボボックス系
// =========================================================================
bool CustomUI::BeginCombo(const char* label, const char* preview_value, ImGuiComboFlags flags) {
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return false;

	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	ImDrawList* draw = window->DrawList;
	const ImVec2 pos = window->DC.CursorPos;

	const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
	const float w = ImGui::GetContentRegionAvail().x;
	const float combo_width = 130.0f;
	const float combo_height = label_size.y + style.FramePadding.y * 2.0f;

	const ImRect bb(pos.x + w - combo_width, pos.y, pos.x + w, pos.y + combo_height);
	const ImRect total_bb(pos.x, pos.y, pos.x + w, pos.y + combo_height);

	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, id, &bb)) return false;

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

	const ImGuiID popup_id = ImHashStr("##ComboPopup", 0, id);
	if (pressed) ImGui::OpenPopupEx(popup_id, ImGuiPopupFlags_None);
	const bool popup_open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);

	float& anim = GetAnim(id);
	anim = ImLerp(anim, hovered ? 0.5f : 0.0f, 0.05f);

	draw->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
	draw->AddRectFilled(bb.Min, bb.Max, ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, anim)), style.FrameRounding);

	if (preview_value && !(flags & ImGuiComboFlags_NoPreview)) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
		ImVec2 text_min = ImVec2(bb.Min.x + style.FramePadding.x, bb.Min.y + style.FramePadding.y);
		ImVec2 text_max = ImVec2(bb.Max.x - 5.0f, bb.Max.y - 0.0f);
		ImGui::RenderTextClipped(text_min, text_max, preview_value, nullptr, nullptr);
		ImGui::PopStyleColor();
	}

	draw->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding);
	ImGui::RenderArrow(window->DrawList, ImVec2(bb.Max.x - 15.0f, bb.Min.y + style.FramePadding.y), ImGui::GetColorU32(ImGuiCol_Text), ImGuiDir_Down);
	draw->AddText(ImVec2(total_bb.Min.x, bb.Min.y + style.FramePadding.y), ImGui::GetColorU32(ImGuiCol_Text), label);

	if (!popup_open) return false;
	return ImGui::BeginComboPopup(popup_id, bb, flags);
}

void CustomUI::EndCombo() { ImGui::EndCombo(); }

bool CustomUI::Combo(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items) {
	ImGuiContext& g = *GImGui;
	const char* preview = nullptr;
	if (*current_item >= 0 && *current_item < items_count) preview = items[*current_item];

	if (height_in_items != -1 && g.NextWindowData.SizeCond == ImGuiCond_None) {
		float text_height = ImGui::CalcTextSize("A").y;
		ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, text_height * (float)height_in_items * 1.4f));
	}

	if (!BeginCombo(label, preview, ImGuiComboFlags_None)) return false;

	bool changed = false;
	for (int i = 0; i < items_count; i++) {
		ImGui::PushID(i);
		const bool selected = (i == *current_item);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));

		if (ImGui::Selectable(items[i], selected, ImGuiSelectableFlags_None, ImVec2(0.0f, 25.0f))) {
			changed = true;
			*current_item = i;
		}
		if (selected) ImGui::SetItemDefaultFocus();

		ImGui::PopStyleVar();
		ImGui::PopID();
	}
	ImGui::EndCombo();
	if (changed) ImGui::MarkItemEdited(g.LastItemData.ID);
	return changed;
}

bool CustomUI::Combo(const char* label, int* current_item, const char* items_separated_by_zeros, int height_in_items) {
	int count = 0;
	const char* p = items_separated_by_zeros;
	while (*p) { p += strlen(p) + 1; count++; }
	const char** ptrs = new const char* [count];
	p = items_separated_by_zeros;
	for (int i = 0; i < count; i++) { ptrs[i] = p; p += strlen(p) + 1; }
	bool changed = Combo(label, current_item, ptrs, count, height_in_items);
	delete[] ptrs;
	return changed;
}

// =========================================================================
// テキスト・カラー
// =========================================================================
bool CustomUI::InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback cb, void* user_data) {
	IM_ASSERT(!(flags & ImGuiInputTextFlags_Multiline));
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImVec2 pos = window->DC.CursorPos;
	const float w = ImGui::GetContentRegionAvail().x;
	const float frame_height = ImGui::CalcTextSize("A", nullptr, true).y + style.FramePadding.y * 2.0f;
	const float frame_width = 130.0f;

	const ImRect frame_bb(pos.x + w - frame_width, pos.y, pos.x + w, pos.y + frame_height);
	const ImRect total_bb(pos.x, pos.y, pos.x + w, pos.y + frame_height);
	ImGuiID id = window->GetID(label);

	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
	bool value_changed = ImGui::InputTextEx(label, nullptr, buf, (int)buf_size, ImVec2(frame_width, frame_height), flags, cb, user_data);
	ImGui::PopStyleColor(3);

	float& anim = GetAnim(id);
	anim = ImLerp(anim, ImGui::IsItemActive() ? 0.5f : (ImGui::IsItemHovered() ? 0.25f : 0.0f), 0.05f);

	ImGui::RenderNavHighlight(frame_bb, id);
	ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), false, 3.0f);
	window->DrawList->AddRectFilled(frame_bb.Min, frame_bb.Max, ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, anim)), 3.0f);
	window->DrawList->AddRect(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), 3.0f);
	window->DrawList->AddText(ImVec2(total_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), ImGui::GetColorU32(ImGuiCol_TextDisabled), label);

	if (value_changed) ImGui::MarkItemEdited(id);
	return value_changed;
}

bool CustomUI::ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags) {
	return ColorEdit4(label, col, flags | ImGuiColorEditFlags_NoAlpha);
}

bool CustomUI::ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags) {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(13.0f, 13.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 12.0f));
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::GetColorU32(ImGuiCol_FrameBg));

	bool changed = ImGui::ColorEdit4(label, col, flags);
	ImGui::PopStyleVar(4);
	ImGui::PopStyleColor();
	return changed;
}

// =========================================================================
// 空間・データ可視化ウィジェット (2D Pad, Graph, Dial, Curve)
// =========================================================================
bool CustomUI::RotationDial(const char* label, float* p_angle_rad, float radius, float min_rad, float max_rad) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	ImDrawList* draw = window->DrawList;
	const ImVec2 pos = window->DC.CursorPos;

	const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
	const float widget_height = radius * 2.0f;
	const float dial_y_offset = label_size.y + 4.0f;
	const ImVec2 center = ImVec2(pos.x + radius + style.FramePadding.x, pos.y + dial_y_offset + radius);

	char val_buf[64];
	snprintf(val_buf, sizeof(val_buf), "%.1f deg", *p_angle_rad * (180.0f / 3.14159265f));
	const ImVec2 text_size = ImGui::CalcTextSize(val_buf);

	const float text_padding = 15.0f;
	const float widget_width = (radius * 2.0f) + style.FramePadding.x * 2.0f + text_padding + text_size.x + 10.0f;

	const ImRect dial_bb(pos.x, pos.y + dial_y_offset, pos.x + radius * 2.0f + style.FramePadding.x * 2.0f, pos.y + dial_y_offset + widget_height);
	const ImRect total_bb(pos.x, pos.y, pos.x + widget_width, dial_bb.Max.y + style.FramePadding.y);

	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, id, &dial_bb)) return false;

	bool hovered, held;
	ImGui::ButtonBehavior(dial_bb, id, &hovered, &held);
	bool value_changed = false;

	if (g.ActiveId == id) {
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			ImVec2 mouse_pos = ImGui::GetIO().MousePos;
			float target_angle = atan2f(mouse_pos.y - center.y, mouse_pos.x - center.x) + 1.57079632f;
			while (target_angle > 3.14159265f) target_angle -= 6.2831853f;
			while (target_angle < -3.14159265f) target_angle += 6.2831853f;
			target_angle = ImClamp(target_angle, min_rad, max_rad);
			*p_angle_rad = target_angle;
			value_changed = true;
			ImGui::MarkItemEdited(id);
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) ImGui::ClearActiveID();
	}

	float& anim = GetAnim(id);
	anim = ImLerp(anim, (g.ActiveId == id) ? 0.4f : hovered ? 0.2f : 0.0f, 0.1f);

	draw->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), label);
	draw->AddCircleFilled(center, radius, ImGui::GetColorU32(ImGuiCol_FrameBg), 36);
	if (anim > 0.0f) draw->AddCircleFilled(center, radius, ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, anim)), 36);
	draw->AddCircle(center, radius, ImGui::GetColorU32(ImGuiCol_Border), 36, 1.0f);

	float display_angle = *p_angle_rad - 1.57079632f;
	ImVec2 direction(cosf(display_angle), sinf(display_angle));
	ImVec2 needle_end(center.x + direction.x * radius, center.y + direction.y * radius);
	const ImU32 needle_col = ImGui::GetColorU32((g.ActiveId == id) ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);

	draw->AddLine(center, needle_end, needle_col, 2.5f);
	draw->AddCircleFilled(needle_end, 3.5f, needle_col, 12);
	draw->AddText(ImVec2(dial_bb.Max.x + text_padding, center.y - text_size.y * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), val_buf);

	return value_changed;
}

bool CustomUI::Vec2PositionPad(const char* label, float v[2], float min_val, float max_val, const ImVec2& size_arg) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	ImDrawList* draw = window->DrawList;
	const ImVec2 pos = window->DC.CursorPos;

	const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
	ImVec2 pad_size = ImGui::CalcItemSize(size_arg, 100.0f, 100.0f);
	const float pad_y_offset = label_size.y + 4.0f;
	const ImRect pad_bb(pos.x, pos.y + pad_y_offset, pos.x + pad_size.x, pos.y + pad_y_offset + pad_size.y);

	char val_buf[64];
	snprintf(val_buf, sizeof(val_buf), "X:%.2f\nY:%.2f", v[0], v[1]);
	const ImVec2 text_size = ImGui::CalcTextSize(val_buf);
	const float total_width = pad_size.x + 12.0f + text_size.x + 8.0f;
	const ImRect total_bb(pos.x, pos.y, pos.x + total_width, pad_bb.Max.y + style.FramePadding.y);

	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, id, &pad_bb)) return false;

	bool hovered, held;
	ImGui::ButtonBehavior(pad_bb, id, &hovered, &held);
	bool value_changed = false;

	float nx = ImClamp((v[0] - min_val) / (max_val - min_val), 0.0f, 1.0f);
	float ny = 1.0f - ImClamp((v[1] - min_val) / (max_val - min_val), 0.0f, 1.0f);

	if (held) {
		nx = ImClamp((g.IO.MousePos.x - pad_bb.Min.x) / pad_size.x, 0.0f, 1.0f);
		ny = ImClamp((g.IO.MousePos.y - pad_bb.Min.y) / pad_size.y, 0.0f, 1.0f);
		v[0] = min_val + nx * (max_val - min_val);
		v[1] = min_val + (1.0f - ny) * (max_val - min_val);
		value_changed = true;
		ImGui::MarkItemEdited(id);
	}

	float& anim = GetAnim(id);
	anim = ImLerp(anim, held ? 0.4f : hovered ? 0.15f : 0.0f, 0.1f);

	draw->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), label);
	draw->AddRectFilled(pad_bb.Min, pad_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
	if (anim > 0.0f) draw->AddRectFilled(pad_bb.Min, pad_bb.Max, ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, anim)), style.FrameRounding);
	draw->AddRect(pad_bb.Min, pad_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding);

	ImVec2 center(pad_bb.Min.x + pad_size.x * 0.5f, pad_bb.Min.y + pad_size.y * 0.5f);
	draw->AddLine(ImVec2(pad_bb.Min.x, center.y), ImVec2(pad_bb.Max.x, center.y), ImGui::GetColorU32(ImGuiCol_Border, 0.4f));
	draw->AddLine(ImVec2(center.x, pad_bb.Min.y), ImVec2(center.x, pad_bb.Max.y), ImGui::GetColorU32(ImGuiCol_Border, 0.4f));

	ImVec2 knob_pos(pad_bb.Min.x + nx * pad_size.x, pad_bb.Min.y + ny * pad_size.y);
	const ImU32 grab_col = ImGui::GetColorU32(held ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);
	draw->AddLine(center, knob_pos, grab_col, 1.5f);
	draw->AddCircleFilled(knob_pos, 5.0f, grab_col, 16);
	draw->AddCircle(knob_pos, 5.0f, ImGui::GetColorU32(ImGuiCol_Border), 16, 1.0f);
	draw->AddText(ImVec2(pad_bb.Max.x + 12.0f, center.y - text_size.y * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), val_buf);

	return value_changed;
}

void CustomUI::MiniPerformanceGraph(const char* label, const float* values, int values_count, float scale_min, float scale_max, const ImVec2& size_arg) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems || values_count < 2) return;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	ImDrawList* draw = window->DrawList;
	const ImVec2 pos = window->DC.CursorPos;

	ImVec2 graph_size = ImGui::CalcItemSize(size_arg, ImGui::GetContentRegionAvail().x, 40.0f);
	const ImRect graph_bb(pos.x, pos.y + ImGui::CalcTextSize(label).y + 4.0f, pos.x + graph_size.x, pos.y + ImGui::CalcTextSize(label).y + 4.0f + graph_size.y);
	const ImRect total_bb(pos.x, pos.y, pos.x + graph_size.x, graph_bb.Max.y + style.FramePadding.y);

	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, 0, &graph_bb)) return;

	draw->AddRectFilled(graph_bb.Min, graph_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
	draw->AddRect(graph_bb.Min, graph_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding);

	if (scale_min == FLT_MAX || scale_max == FLT_MAX) {
		float v_min = FLT_MAX, v_max = -FLT_MAX;
		for (int i = 0; i < values_count; i++) {
			if (values[i] < v_min) v_min = values[i];
			if (values[i] > v_max) v_max = values[i];
		}
		if (scale_min == FLT_MAX) scale_min = v_min;
		if (scale_max == FLT_MAX) scale_max = v_max;
	}
	float range = (scale_max - scale_min <= 0.0f) ? 1.0f : scale_max - scale_min;

	ImVector<ImVec2> points;
	points.reserve(values_count + 2);
	float x_step = graph_size.x / (float)(values_count - 1);

	for (int i = 0; i < values_count; i++) {
		float ratio_y = ImClamp((values[i] - scale_min) / range, 0.0f, 1.0f);
		points.push_back(ImVec2(graph_bb.Min.x + i * x_step, graph_bb.Max.y - ratio_y * graph_size.y));
	}

	ImVector<ImVec2> filled = points;
	filled.push_back(ImVec2(graph_bb.Max.x, graph_bb.Max.y));
	filled.push_back(ImVec2(graph_bb.Min.x, graph_bb.Max.y));

	ImU32 col_accent = ImGui::GetColorU32(ImGuiCol_SliderGrab);
	draw->AddConvexPolyFilled(filled.Data, filled.Size, (col_accent & 0x00FFFFFF) | 0x33000000);
	draw->AddPolyline(points.Data, points.Size, col_accent, 0, 1.5f);

	draw->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), label);
	char cur_buf[32]; snprintf(cur_buf, sizeof(cur_buf), "%.1f", values[values_count - 1]);
	draw->AddText(ImVec2(graph_bb.Max.x - ImGui::CalcTextSize(cur_buf).x - 4.0f, pos.y), ImGui::GetColorU32(ImGuiCol_TextDisabled), cur_buf);
}

bool CustomUI::CurveEasingEditor(const char* label, ImVec2& cp1, ImVec2& cp2, const ImVec2& size_arg) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	ImDrawList* draw = window->DrawList;
	const ImVec2 pos = window->DC.CursorPos;

	ImVec2 box_size = ImGui::CalcItemSize(size_arg, 120.0f, 120.0f);
	const ImRect box_bb(pos.x, pos.y + ImGui::CalcTextSize(label).y + 4.0f, pos.x + box_size.x, pos.y + ImGui::CalcTextSize(label).y + 4.0f + box_size.y);
	const ImRect total_bb(pos.x, pos.y, pos.x + box_size.x, box_bb.Max.y + style.FramePadding.y);

	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, id, &box_bb)) return false;

	bool hovered, held;
	ImGui::ButtonBehavior(box_bb, id, &hovered, &held);

	// イージング空間の始点と終点 (0,1) 空間
	ImVec2 start_pt(box_bb.Min.x, box_bb.Max.y);
	ImVec2 end_pt(box_bb.Max.x, box_bb.Min.y);

	// 制御点の画面ピクセル座標への変換
	ImVec2 p1 = ImVec2(box_bb.Min.x + cp1.x * box_size.x, box_bb.Max.y - cp1.y * box_size.y);
	ImVec2 p2 = ImVec2(box_bb.Min.x + cp2.x * box_size.x, box_bb.Max.y - cp2.y * box_size.y);

	ImGuiID id_p1 = window->GetID("##p1");
	ImGuiID id_p2 = window->GetID("##p2");
	bool value_changed = false;

	// 制御点1のドラッグ判定 (手動計算に修正)
	ImGui::SetCursorScreenPos(ImVec2(p1.x - 6.0f, p1.y - 6.0f));
	ImGui::InvisibleButton("##p1_btn", ImVec2(12.0f, 12.0f));
	if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
		cp1.x = ImClamp((g.IO.MousePos.x - box_bb.Min.x) / box_size.x, 0.0f, 1.0f);
		cp1.y = ImClamp((box_bb.Max.y - g.IO.MousePos.y) / box_size.y, 0.0f, 1.0f);
		value_changed = true;
	}

	// 制御点2のドラッグ判定 (手動計算に修正)
	ImGui::SetCursorScreenPos(ImVec2(p2.x - 6.0f, p2.y - 6.0f));
	ImGui::InvisibleButton("##p2_btn", ImVec2(12.0f, 12.0f));
	if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
		cp2.x = ImClamp((g.IO.MousePos.x - box_bb.Min.x) / box_size.x, 0.0f, 1.0f);
		cp2.y = ImClamp((box_bb.Max.y - g.IO.MousePos.y) / box_size.y, 0.0f, 1.0f);
		value_changed = true;
	}

	// 背景とベジエ曲線の描画
	draw->AddRectFilled(box_bb.Min, box_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
	draw->AddRect(box_bb.Min, box_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding);

	// ハンドル線の描画
	draw->AddLine(start_pt, p1, ImGui::GetColorU32(ImGuiCol_TextDisabled), 1.0f);
	draw->AddLine(end_pt, p2, ImGui::GetColorU32(ImGuiCol_TextDisabled), 1.0f);

	// 三次ベジエ曲線の描画
	draw->AddBezierCubic(start_pt, p1, p2, end_pt, ImGui::GetColorU32(ImGuiCol_SliderGrab), 2.0f);

	// 制御点ノブの描画
	draw->AddCircleFilled(p1, 4.0f, ImGui::GetColorU32(ImGuiCol_Text));
	draw->AddCircleFilled(p2, 4.0f, ImGui::GetColorU32(ImGuiCol_Text));
	draw->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), label);

	return value_changed;
}

// =========================================================================
// 特殊システム (Toast Notifications)
// =========================================================================
void CustomUI::AddToast(const std::string& message, ImGuiCol type_color, float duration) {
	CustomUI::Toast t;
	t.message = message;
	t.color_type = type_color;
	t.duration = duration;
	t.max_duration = duration;

	// toasts の前に CustomUI:: を付与
	CustomUI::toasts.push_back(t);
}

void CustomUI::RenderToasts(float delta_time) {
	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImDrawList* draw = ImGui::GetForegroundDrawList(); // 最前面への描画

	// 画面右下基点 (手動計算に修正)
	ImVec2 edge_pos = ImVec2(
		vp->Pos.x + vp->Size.x - 20.0f,
		vp->Pos.y + vp->Size.y - 20.0f
	);

	for (int i = (int)toasts.size() - 1; i >= 0; i--) {
		Toast& t = toasts[i];
		t.duration -= delta_time;
		if (t.duration <= 0.0f) {
			toasts.erase(toasts.begin() + i);
			continue;
		}

		ImVec2 txt_size = ImGui::CalcTextSize(t.message.c_str());
		ImVec2 box_size(txt_size.x + 30.0f, txt_size.y + 20.0f);

		// 枠のバウンディングボックス (手動計算に修正)
		ImRect box_bb(
			edge_pos.x - box_size.x,
			edge_pos.y - box_size.y,
			edge_pos.x,
			edge_pos.y
		);

		// フェードイン・アウトの簡易アニメーション
		float alpha = ImMin(1.0f, t.duration / 0.3f);
		if (t.max_duration - t.duration < 0.2f) alpha = (t.max_duration - t.duration) / 0.2f;

		ImU32 bg_col = ImGui::ColorConvertFloat4ToU32(ImVec4(0.1f, 0.1f, 0.13f, 0.95f * alpha));
		ImU32 border_col = ImGui::ColorConvertFloat4ToU32(ImVec4(0.3f, 0.3f, 0.35f, alpha));
		ImU32 text_col = ImGui::ColorConvertFloat4ToU32(ImVec4(0.9f, 0.9f, 0.9f, alpha));

		// トースト通知枠の描画
		draw->AddRectFilled(box_bb.Min, box_bb.Max, bg_col, 4.0f);
		draw->AddRect(box_bb.Min, box_bb.Max, border_col, 4.0f);

		// 下部に経過インジケーター（消滅までのバー）を表示 (手動計算に修正)
		float progress = t.duration / t.max_duration;
		draw->AddLine(
			ImVec2(box_bb.Min.x + 4.0f, box_bb.Max.y - 3.0f),
			ImVec2(box_bb.Min.x + 4.0f + (box_size.x - 8.0f) * progress, box_bb.Max.y - 3.0f),
			t.color_type,
			2.0f
		);

		// テキスト位置 (手動計算に修正)
		draw->AddText(
			ImVec2(box_bb.Min.x + 15.0f, box_bb.Min.y + 10.0f),
			text_col,
			t.message.c_str()
		);

		edge_pos.y -= box_size.y + 10.0f; // 次のトーストを上に積み上げる
	}
}

// 引数を void (なし) に変更
void CustomUI::DrawCustomUIWidgetsTestWindow()
{
	// ====== 【ここがポイント】ImGuiの内部管理データから現在のdeltaTimeを自動取得 ======
	float deltaTime = ImGui::GetIO().DeltaTime;
	// =========================================================================


	// =========================================================================
	// 1. 装飾系コントロールのテスト
	// =========================================================================
	CustomUI::SectionLabel("1. Decorations");

	static bool is_header_open = true;
	CustomUI::GradientHeader("Gradient Header Category", &is_header_open);
	if (is_header_open)
	{
		ImGui::Text("This content is inside the gradient header!");
	}

	CustomUI::Separator();

	// =========================================================================
	// 2. 基本コントロールのテスト
	// =========================================================================
	CustomUI::SectionLabel("2. Basic Controls");

	if (CustomUI::Button("Click Me (Test Toast)"))
	{
		CustomUI::AddToast("Button clicked! System normal.", ImGuiCol_PlotLinesHovered, 3.5f);
	}

	ImGui::SameLine();

	if (CustomUI::Button("Trigger Warning Toast"))
	{
		CustomUI::AddToast("Warning: High memory usage detected!", ImGuiCol_NavHighlight, 4.0f);
	}

	static bool checkbox_state = false;
	CustomUI::Checkbox("Toggle System Feature", &checkbox_state);

	// =========================================================================
	// 3. スライダー / ドラッグのテスト
	// =========================================================================
	CustomUI::SectionLabel("3. Sliders & Drags");

	static float test_float = 0.5f;
	CustomUI::SliderFloat("Float Slider", &test_float, 0.0f, 1.0f, "%.2f");

	static int test_int = 50;
	CustomUI::SliderInt("Int Slider", &test_int, 0, 100, "%d%%");

	// =========================================================================
	// 4. コンボボックスのテスト
	// =========================================================================
	CustomUI::SectionLabel("4. Combo Boxes");

	static int current_item = 0;
	const char* items[] = { "Render Mode: Forward", "Render Mode: Deferred", "Render Mode: RayTracing" };
	CustomUI::Combo("Render Pipeline", &current_item, items, IM_ARRAYSIZE(items));

	// =========================================================================
	// 5. テキスト / カラーのテスト
	// =========================================================================
	CustomUI::SectionLabel("5. Input & Color");

	static char text_buffer[128] = "Engine_Node_01";
	CustomUI::InputText("Object Name", text_buffer, IM_ARRAYSIZE(text_buffer));

	static float color_rgb[3] = { 0.31f, 0.47f, 1.0f };
	CustomUI::ColorEdit3("Accent Color RGB", color_rgb);

	// =========================================================================
	// 6. 空間・データ可視化
	// =========================================================================
	CustomUI::SectionLabel("6. Visualization Controls");

	static float angle_rad = 0.0f;
	CustomUI::RotationDial("Object Rotation", &angle_rad, 25.0f, -3.1415f, 3.1415f);

	ImGui::Spacing();

	static float vec2_pos[2] = { 0.0f, 0.0f };
	CustomUI::Vec2PositionPad("Spawn Offset (X/Y)", vec2_pos, -10.0f, 10.0f, ImVec2(100.0f, 100.0f));

	ImGui::Spacing();

	// ミニグラフのアニメーション（内部で解決したdeltaTimeを使用）
	static float fps_values[30] = { 0 };
	static float timer = 0.0f;
	timer += deltaTime;
	if (timer > 0.05f)
	{
		for (int i = 0; i < 29; i++) fps_values[i] = fps_values[i + 1];
		fps_values[29] = 55.0f + (float)(rand() % 100) / 10.0f;
		timer = 0.0f;
	}
	CustomUI::MiniPerformanceGraph("Engine Frame-rate (FPS)", fps_values, IM_ARRAYSIZE(fps_values), 0.0f, 80.0f, ImVec2(0, 50));

	ImGui::Spacing();

	static ImVec2 cp1(0.42f, 0.0f);
	static ImVec2 cp2(0.58f, 1.0f);
	CustomUI::CurveEasingEditor("Camera Move Ease Curve", cp1, cp2, ImVec2(120.0f, 120.0f));


	// =========================================================================
	// 7. トーストシステムのレンダリング
	// =========================================================================
	CustomUI::RenderToasts(deltaTime);
}