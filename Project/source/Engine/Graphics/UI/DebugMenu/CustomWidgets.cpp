#include "CustomWidgets.h"


// セクションタイトル（見出し）
void CustomUI::SectionLabel(const char* label) {
	ImGui::Spacing();
	ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f), "%s", label);
	ImGui::Separator();
	ImGui::Spacing();
}

// テーマカラーに合わせたセパレーター
void CustomUI::Separator() {
	ImGui::Separator();
	ImGui::Spacing();
}// step を追加 (例: 0.1f, 1.0f など)
//bool CustomUI::CustomSlider(const char* label, float* value, float min, float max, float step, const char* format) {
//	ImGuiContext& g = *GImGui;
//	ImDrawList* draw_list = ImGui::GetWindowDrawList();
//	ImVec2 pos = ImGui::GetCursorScreenPos();
//	float width = ImGui::CalcItemWidth();
//	float height = g.FontSize;
//
//	ImGui::InvisibleButton(label, ImVec2(width, height));
//	bool is_active = ImGui::IsItemActive();
//
//	// 2. 値の更新処理（丸め処理を追加）
//	if (is_active) {
//		float ratio = (ImGui::GetIO().MousePos.x - pos.x) / width;
//		float raw_value = min + (max - min) * ImClamp(ratio, 0.0f, 1.0f);
//
//		// 指定した step で丸める処理
//		// 値を step で割り、四捨五入してから再度 step を掛ける
//		if (step > 0.0f) {
//			*value = roundf(raw_value / step) * step;
//		}
//		else {
//			*value = raw_value;
//		}
//
//		// 念のため min/max を超えないようにクランプ
//		*value = ImClamp(*value, min, max);
//	}
//
//	// 3. 描画処理 (変更なし)
//	float ratio = (*value - min) / (max - min);
//	float knob_x = pos.x + (ratio * width);
//	draw_list->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(50, 50, 50, 255), 5.0f);
//	draw_list->AddCircleFilled(ImVec2(knob_x, pos.y + height * 0.5f), 8.0f, is_active ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 255));
//
//	// 4. フォーマットされたテキストの描画 (変更なし)
//	char buf[64];
//	snprintf(buf, sizeof(buf), format, *value);
//	draw_list->AddText(ImVec2(pos.x + width + 10, pos.y), IM_COL32(255, 255, 255, 255), buf);
//
//	return is_active;
//}

// Project-specific includes (adjust paths as needed).

#include <unordered_map>

// =========================================================================
// Internal helpers
// =========================================================================
namespace
{
	// Returns/creates a per-widget animation float stored by ImGuiID.
	float& GetAnim(ImGuiID id)
	{
		static std::unordered_map<ImGuiID, float> s_map;
		return s_map[id];
	}
} // anonymous namespace

// =========================================================================
// CustomUI::Button
// =========================================================================
bool CustomUI::Button(const char* label, const ImVec2& size_arg) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

	// サイズ計算
	ImVec2 size = ImGui::CalcItemSize(size_arg,
		label_size.x + style.FramePadding.x * 2.0f,
		label_size.y + style.FramePadding.y * 2.0f);

	const ImVec2 pos = window->DC.CursorPos;
	const ImRect bb(pos.x, pos.y, pos.x + size.x, pos.y + size.y);

	ImGui::ItemSize(size, style.FramePadding.y);
	if (!ImGui::ItemAdd(bb, id))
		return false;

	// ボタンの挙動判定
	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

	// ImGuiの現在のテーマカラーを取得
	const ImU32 col = ImGui::GetColorU32(
		(held && hovered) ? ImGuiCol_ButtonActive :
		hovered ? ImGuiCol_ButtonHovered :
		ImGuiCol_Button);

	// 描画
	ImGui::RenderNavHighlight(bb, id);
	ImGui::RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);

	// テキストカラーの決定
	const ImU32 text_col = ImGui::GetColorU32(ImGuiCol_Text);

	const ImVec2 text_pos = ImVec2(
		bb.Min.x + (size.x - label_size.x) * 0.5f,
		bb.Min.y + (size.y - label_size.y) * 0.5f
	);

	window->DrawList->AddText(text_pos, text_col, label);

	return pressed;
}

// =========================================================================
// CustomUI::Checkbox
// =========================================================================
bool CustomUI::Checkbox(const char* label, bool* v) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImDrawList* draw = window->DrawList;
	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

	const float w = ImGui::GetContentRegionAvail().x; // ウィンドウ幅ではなく、現在の利用可能幅を取得
	const ImVec2 sz = { 30.f, 17.f };
	const ImVec2 pos = window->DC.CursorPos;

	// ImRect加算の修正
	const ImRect frame_bb(pos.x + w - sz.x, pos.y, pos.x + w, pos.y + sz.y);
	const ImRect total_bb(pos.x, pos.y, pos.x + w, pos.y + ImMax(sz.y, label_size.y));

	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, id))
		return false;

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(frame_bb, id, &hovered, &held);
	if (pressed) {
		*v = !(*v);
		ImGui::MarkItemEdited(id);
	}

	// アニメーション (GetAnimは既存の実装を使用)
	float& anim = GetAnim(id);
	anim = ImLerp(anim, *v ? 1.0f : 0.0f, 0.08f);

	// テキストカラーの決定 (ImGuiの現在のスタイルを使用)
	const ImU32 text_col = ImGui::GetColorU32(*v ? ImGuiCol_Text : ImGuiCol_TextDisabled);
	draw->AddText(ImVec2(total_bb.Min.x, frame_bb.Min.y + (sz.y - label_size.y) * 0.5f), text_col, label);

	// トラック描画 (ImGuiのテーマカラーを利用)
	const ImU32 col_bg = ImGui::GetColorU32(ImLerp(ImVec4(0.2f, 0.2f, 0.2f, 1.0f), ImVec4(0.0f, 0.5f, 1.0f, 1.0f), anim));
	draw->AddRectFilled(frame_bb.Min, frame_bb.Max, col_bg, sz.y * 0.5f);
	draw->AddRect(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), sz.y * 0.5f);

	// ノブの描画
	const float knob_radius = (sz.y - 4.0f) * 0.5f;
	const ImVec2 knob_pos = ImVec2(frame_bb.Min.x + knob_radius + 2.0f + (sz.x - knob_radius * 2.0f - 4.0f) * anim, frame_bb.GetCenter().y);
	draw->AddCircleFilled(knob_pos, knob_radius, ImGui::GetColorU32(ImGuiCol_Text), 20);

	return pressed;
}

// =========================================================================
// CustomUI::SliderScalar
// =========================================================================
bool CustomUI::SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	const ImVec2 pos = window->DC.CursorPos;
	const float w = ImGui::GetContentRegionAvail().x;

	// レイアウト設定
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

	// 状態管理 (int で代用することでメモリ管理を単純化)
	ImGuiStorage* storage = ImGui::GetStateStorage();
	bool is_editing = (storage->GetInt(id, 0) != 0);

	// ダブルクリックで編集開始
	if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(number_bb.Min, number_bb.Max)) {
		storage->SetInt(id, 1);
		is_editing = true;
	}

	bool value_changed = false;

	if (is_editing) {
		// 編集モード：制限なしで値を代入
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
		// 通常モード：SliderBehavior (こちらは範囲制限がかかる)
		bool hovered, held;
		ImGui::ButtonBehavior(total_bb, id, &hovered, &held);

		ImRect grab_bb;
		if (ImGui::SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, format, flags, &grab_bb)) {
			ImGui::MarkItemEdited(id);
			value_changed = true;
		}

		// 描画 (従来通り)
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
bool CustomUI::SliderFloat(const char* label, float* v,
	float v_min, float v_max,
	const char* format, ImGuiSliderFlags flags)
{
	return SliderScalar(label, ImGuiDataType_Float, v, &v_min, &v_max, format, flags);
}

bool CustomUI::SliderInt(const char* label, int* v,
	int v_min, int v_max,
	const char* format, ImGuiSliderFlags flags)
{
	return SliderScalar(label, ImGuiDataType_S32, v, &v_min, &v_max, format, flags);
}
bool CustomUI::DragScalar(const char* label, ImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	ImDrawList* draw = window->DrawList;
	const ImVec2 pos = window->DC.CursorPos;

	const float w = ImGui::GetContentRegionAvail().x;
	const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

	// レイアウト設定
	const float frame_height = 20.0f; // ドラッグエリアの高さ
	const float label_padding = 4.0f;
	const float frame_y_offset = label_size.y + label_padding;

	// 座標計算
	const ImRect frame_bb(pos.x, pos.y + frame_y_offset, pos.x + w, pos.y + frame_y_offset + frame_height);
	const ImRect total_bb(pos.x, pos.y, pos.x + w, frame_bb.Max.y + style.FramePadding.y);

	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, id, &frame_bb))
		return false;

	// 入力消費 (ホバー・アクティブ状態の取得)
	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(frame_bb, id, &hovered, &held);

	if (format == nullptr)
		format = ImGui::DataTypeGetInfo(data_type)->PrintFmt;

	// --- ドラッグ動作 ---
	// ここが SliderBehavior からの変更点
	const bool value_changed = ImGui::DragBehavior(id, data_type, p_data, v_speed, p_min, p_max, format, flags);
	if (value_changed)
		ImGui::MarkItemEdited(id);

	// --- 描画 ---
	// ラベル
	draw->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), label);

	// ドラッグエリアの背景
	const ImU32 frame_col = ImGui::GetColorU32(held ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
	draw->AddRectFilled(frame_bb.Min, frame_bb.Max, frame_col, style.FrameRounding);
	draw->AddRect(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding);

	// 数値テキスト
	char value_buf[64];
	ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);

	// テキスト中央揃え描画用の計算
	const ImVec2 text_size = ImGui::CalcTextSize(value_buf);
	const ImVec2 text_pos = ImVec2(
		frame_bb.Min.x + (frame_bb.GetWidth() - text_size.x) * 0.5f,
		frame_bb.Min.y + (frame_bb.GetHeight() - text_size.y) * 0.5f
	);
	draw->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), value_buf);

	return value_changed;
}

// 呼び出し用ラッパー
bool CustomUI::DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
	return DragScalar(label, ImGuiDataType_Float, v, v_speed, &v_min, &v_max, format, flags);
}
// =========================================================================
// CustomUI::BeginCombo / EndCombo
// =========================================================================
bool CustomUI::BeginCombo(const char* label, const char* preview_value, ImGuiComboFlags flags) {
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return false;

	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	ImDrawList* draw = window->DrawList;
	const ImVec2 pos = window->DC.CursorPos;

	// サイズと領域の計算
	const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
	const float w = ImGui::GetContentRegionAvail().x;
	const float combo_width = 130.0f;
	const float combo_height = label_size.y + style.FramePadding.y * 2.0f;

	// ImRectの安全な構築
	const ImRect bb(pos.x + w - combo_width, pos.y, pos.x + w, pos.y + combo_height);
	const ImRect total_bb(pos.x, pos.y, pos.x + w, pos.y + combo_height);

	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, id, &bb)) return false;

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

	const ImGuiID popup_id = ImHashStr("##ComboPopup", 0, id);
	if (pressed) {
		ImGui::OpenPopupEx(popup_id, ImGuiPopupFlags_None);
	}
	const bool popup_open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);

	// ホバーアニメーション
	float& anim = GetAnim(id);
	anim = ImLerp(anim, hovered ? 0.5f : 0.0f, 0.05f);

	// 背景描画 (ImGuiCol_FrameBg等を利用)
	const ImU32 col_bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
	const ImU32 col_hover = ImGui::GetColorU32(ImGuiCol_FrameBgHovered);

	// 背景の重ね合わせ（アニメーションを表現）
	draw->AddRectFilled(bb.Min, bb.Max, col_bg, style.FrameRounding);
	draw->AddRectFilled(bb.Min, bb.Max, ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, anim)), style.FrameRounding);

	// プレビューテキスト
	if (preview_value && !(flags & ImGuiComboFlags_NoPreview)) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
		ImVec2 text_min = ImVec2(bb.Min.x + style.FramePadding.x, bb.Min.y + style.FramePadding.y);
		ImVec2 text_max = ImVec2(bb.Max.x - 5.0f, bb.Max.y - 0.0f);

		ImGui::RenderTextClipped(text_min, text_max, preview_value, nullptr, nullptr);
		ImGui::PopStyleColor();
	}

	// 枠線と矢印
	draw->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding);
	ImGui::RenderArrow(window->DrawList, ImVec2(bb.Max.x - 15.0f, bb.Min.y + style.FramePadding.y), ImGui::GetColorU32(ImGuiCol_Text), ImGuiDir_Down);

	// ラベルの描画
	draw->AddText(ImVec2(total_bb.Min.x, bb.Min.y + style.FramePadding.y), ImGui::GetColorU32(ImGuiCol_Text), label);

	if (!popup_open) return false;

	// ポップアップの開始
	return ImGui::BeginComboPopup(popup_id, bb, flags);
}

void CustomUI::EndCombo()
{
	ImGui::EndCombo();
}

// =========================================================================
// CustomUI::Combo (array overload)
// =========================================================================
bool CustomUI::Combo(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items) {
	ImGuiContext& g = *GImGui;

	// プレビュー値の決定
	const char* preview = nullptr;
	if (*current_item >= 0 && *current_item < items_count)
		preview = items[*current_item];

	// サイズ制約の設定
	if (height_in_items != -1 && g.NextWindowData.SizeCond == ImGuiCond_None) {
		float text_height = ImGui::CalcTextSize("A").y;
		ImGui::SetNextWindowSizeConstraints(
			ImVec2(0.0f, 0.0f),
			ImVec2(FLT_MAX, text_height * (float)height_in_items * 1.4f)
		);
	}

	// 独自実装の BeginCombo を使用
	if (!BeginCombo(label, preview, ImGuiComboFlags_None))
		return false;

	bool changed = false;
	// 各項目の描画
	for (int i = 0; i < items_count; i++) {
		ImGui::PushID(i);
		const bool selected = (i == *current_item);

		// 演算子オーバーロードに頼らないスタイル指定
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));

		if (ImGui::Selectable(items[i], selected, ImGuiSelectableFlags_None, ImVec2(0.0f, 25.0f))) {
			changed = true;
			*current_item = i;
		}

		if (selected)
			ImGui::SetItemDefaultFocus();

		ImGui::PopStyleVar();
		ImGui::PopID();
	}

	// コンボボックスの終了
	ImGui::EndCombo();

	if (changed)
		ImGui::MarkItemEdited(g.LastItemData.ID);

	return changed;
}

// Combo (zero-separated string overload)
bool CustomUI::Combo(const char* label, int* current_item,
	const char* items_separated_by_zeros,
	int height_in_items)
{
	// Count items
	int count = 0;
	const char* p = items_separated_by_zeros;
	while (*p) { p += strlen(p) + 1; count++; }

	// Build temporary pointer array
	// (stack allocation fine for small lists; heap for safety with large ones)
	const char** ptrs = new const char* [count];
	p = items_separated_by_zeros;
	for (int i = 0; i < count; i++) { ptrs[i] = p; p += strlen(p) + 1; }

	bool changed = Combo(label, current_item, ptrs, count, height_in_items);
	delete[] ptrs;
	return changed;
}

// =========================================================================
// CustomUI::InputText
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

	// ImRectの安全な構築 (演算子オーバーロードに依存しない)
	const ImRect frame_bb(pos.x + w - frame_width, pos.y, pos.x + w, pos.y + frame_height);
	const ImRect total_bb(pos.x, pos.y, pos.x + w, pos.y + frame_height);
	const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

	ImGuiID id = window->GetID(label);

	// フレームを透明にして独自描画を行う
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

	bool value_changed = ImGui::InputTextEx(
		label, nullptr, buf, (int)buf_size,
		ImVec2(frame_width, frame_height), flags, cb, user_data);

	ImGui::PopStyleColor(3);

	// アニメーション処理
	float& anim = GetAnim(id);
	anim = ImLerp(anim, ImGui::IsItemActive() ? 0.5f : (ImGui::IsItemHovered() ? 0.25f : 0.0f), 0.05f);

	// 独自フレームの描画
	ImGui::RenderNavHighlight(frame_bb, id);

	// 背景とアニメーション枠の描画 (ImGuiColの活用)
	ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), false, 3.0f);
	window->DrawList->AddRectFilled(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImVec4(1, 1, 1, anim)), 3.0f);
	window->DrawList->AddRect(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), 3.0f);

	// ラベルの描画
	window->DrawList->AddText(
		ImVec2(total_bb.Min.x, frame_bb.Min.y + style.FramePadding.y),
		ImGui::GetColorU32(ImGuiCol_TextDisabled), label);

	if (value_changed)
		ImGui::MarkItemEdited(id);

	return value_changed;
}

// =========================================================================
// CustomUI::ColorEdit3 / ColorEdit4
// =========================================================================
bool CustomUI::ColorEdit3(const char* label, float col[3],
	ImGuiColorEditFlags flags)
{
	return ColorEdit4(label, col, flags | ImGuiColorEditFlags_NoAlpha);
}

bool CustomUI::ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags) {
	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;

	// スタイル設定を標準のImGuiテーマに準拠させる
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(13.0f, 13.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 12.0f));

	// gui.frame_inactive の代わりに ImGuiCol_FrameBg を利用
	// もし特定の背景色にしたい場合は ImGuiCol_PopupBg を使用するのが一般的です
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::GetColorU32(ImGuiCol_FrameBg));

	bool changed = ImGui::ColorEdit4(label, col, flags);

	ImGui::PopStyleVar(4);
	ImGui::PopStyleColor();

	return changed;
}