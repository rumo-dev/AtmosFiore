#include "CustomWidgets.h"
#include <unordered_map>
#include <cmath>
#include <algorithm>
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

	// --- 1. 背景グラデーションの修正 ---
	ImU32 col_base = ImGui::GetColorU32(held ? ImGuiCol_HeaderActive : (hovered ? ImGuiCol_HeaderHovered : ImGuiCol_FrameBg));

	// IM_COL32マクロを展開して安全にアルファチャネルだけを0(完全に透明)にする処理
	// これにより環境に依存せず、確実に左から右へ消えるグラデーションになります
	ImU32 col_fade = (col_base & 0x00FFFFFF); // 最上位バイト（アルファ）を確実にゼロクリア

	// 時計回りの順序: 左上(base), 右上(fade), 右下(fade), 左下(base)
	draw->AddRectFilledMultiColor(bb.Min, bb.Max, col_base, col_fade, col_fade, col_base);

	// --- 2. 三角矢印（手動描画への変更によるバグ回避） ---
	float text_height = ImGui::CalcTextSize(label).y;
	float arrow_size = text_height * 0.4f; // テキストの高さに綺麗に収まるサイズに調整
	ImVec2 arrow_center(bb.Min.x + style.FramePadding.x + arrow_size, bb.Min.y + height * 0.5f);
	ImU32 arrow_col = ImGui::GetColorU32(ImGuiCol_Text);

	if (is_open && *is_open) {
		// 下向き三角形 (開いている状態)
		ImVec2 p1(arrow_center.x - arrow_size, arrow_center.y - arrow_size * 0.5f);
		ImVec2 p2(arrow_center.x + arrow_size, arrow_center.y - arrow_size * 0.5f);
		ImVec2 p3(arrow_center.x, arrow_center.y + arrow_size * 0.5f);
		draw->AddTriangleFilled(p1, p2, p3, arrow_col);
	}
	else {
		// 右向き三角形 (閉じている状態)
		ImVec2 p1(arrow_center.x - arrow_size * 0.5f, arrow_center.y - arrow_size);
		ImVec2 p2(arrow_center.x + arrow_size * 0.5f, arrow_center.y);
		ImVec2 p3(arrow_center.x - arrow_size * 0.5f, arrow_center.y + arrow_size);
		draw->AddTriangleFilled(p1, p2, p3, arrow_col);
	}

	// --- 3. テキストの位置調整 ---
	// 三角形のサイズに合わせてパディングを動的に計算
	float text_x = bb.Min.x + style.FramePadding.x * 2.0f + (arrow_size * 2.0f);
	float text_y = bb.Min.y + style.FramePadding.y;
	draw->AddText(ImVec2(text_x, text_y), ImGui::GetColorU32(ImGuiCol_Text), label);
}
void CustomUI::LoadingBar(const char* label, float fraction, const ImVec2& size_arg) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label);
	ImDrawList* draw = window->DrawList;
	const ImVec2 pos = window->DC.CursorPos;

	// サイズの決定（デフォルトは横幅一杯、高さ12px）
	float default_width = ImGui::GetContentRegionAvail().x;
	float default_height = 12.0f;
	ImVec2 size = ImGui::CalcItemSize(size_arg, default_width, default_height);

	// テキスト（ラベル）がある場合は、テキストの高さ分だけ配置領域（bb）を広げる
	bool has_text = (label && label[0] != '\0' && !ImGui::FindRenderedTextEnd(label));
	float text_height = has_text ? (ImGui::CalcTextSize(label).y + 4.0f) : 0.0f;

	const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + text_height + size.y));

	// ImGuiに領域を登録してカーソルを進める
	ImGui::ItemSize(bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(bb, id)) return;

	// 1. ラベルテキストがある場合は描画
	if (has_text) {
		draw->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), label);
	}

	// ローディングバー本体の描画基準座標
	ImVec2 bar_min(bb.Min.x, bb.Min.y + text_height);
	ImVec2 bar_max(bb.Max.x, bb.Max.y);

	// 2. 滑らかな進捗アニメーション (Lerp)
	float& anim_fraction = GetAnim(id);
	anim_fraction = ImLerp(anim_fraction, ImClamp(fraction, 0.0f, 1.0f), g.IO.DeltaTime * 14.0f);

	// 3. 背景（トラック）の描画
	ImU32 col_bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
	float rounding = size.y * 0.5f; // 完全な丸角（カプセル型）にする
	draw->AddRectFilled(bar_min, bar_max, col_bg, rounding);

	// 4. 進捗バー（アクティブ部分）と動く斜線パターンの描画
	if (anim_fraction > 0.001f) {
		float progress_w = size.x * anim_fraction;
		ImVec2 progress_max(bar_min.x + progress_w, bar_max.y);
		ImU32 col_accent = ImGui::GetColorU32(ImGuiCol_SliderGrab); // テーマのメイン色

		// 進捗バーのベース（土台）を丸角で描画
		draw->AddRectFilled(bar_min, progress_max, col_accent, rounding);

		// ★ 修正点: 存在しない PathClip の代わりに標準の四角形クリッピングを適用
		// これで左右の限界を安全に制限し、ループ内の計算をシンプルにします
		draw->PushClipRect(bar_min, progress_max, true);

		// アニメーション用の時間オフセット
		float time = (float)g.Time;
		float speed = 40.0f;
		float stripe_spacing = 16.0f; // 斜線の間隔
		float stripe_width = 8.0f;    // 斜線の太さ

		float offset = ImFmod(time * speed, stripe_spacing);
		ImU32 col_stripe = IM_COL32(255, 255, 255, 35); // 白の不透明度を少し調整（約14%）

		// 斜線を描画
		float start_x = bar_min.x - size.y;
		float end_x = progress_max.x + size.y;

		for (float x = start_x + offset; x < end_x; x += stripe_spacing) {
			ImVec2 p_tl(x, bar_min.y);
			ImVec2 p_tr(x + stripe_width, bar_min.y);
			ImVec2 p_br(x + stripe_width - size.y, bar_max.y);
			ImVec2 p_bl(x - size.y, bar_max.y);

			draw->AddQuadFilled(p_tl, p_tr, p_br, p_bl, col_stripe);
		}

		// 四角形クリッピングを解除
		draw->PopClipRect();

		// ★ 修正点（カプセル型マスク処理）: 
		// 四角形クリッピングだけだと「両端の丸角部分」から斜線がハミ出て不自然になるため、
		// バーの一番外側の丸角を補正します。
		// 進捗バーが完全に満タン（1.0）でない場合、右端の丸みを綺麗にマスクするために
		// トラックの背景色と同じ色で外側を再レンダリングするか、
		// アンチエイリアスが崩れないように内側にだけ斜線を収める処理を自動で行います。
		if (anim_fraction < 0.99f && progress_w > rounding) {
			// 進捗中の右端部分の丸みを綺麗に補正するため、
			// アクセントカラーの「右側半分だけの丸角」を上から再描画して斜線のはみ出しを綺麗に上書き隠蔽します。
			draw->PushClipRect(ImVec2(progress_max.x - rounding, bar_min.y), progress_max, true);
			draw->AddRectFilled(bar_min, progress_max, col_accent, rounding);
			draw->PopClipRect();
		}

		// 左端の丸みからはみ出た斜線を隠すマスク
		if (progress_w > rounding) {
			draw->PushClipRect(bar_min, ImVec2(bar_min.x + rounding, bar_max.y), true);
			draw->AddRectFilled(bar_min, progress_max, col_accent, rounding);
			draw->PopClipRect();
		}
	}
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
		ImVec2 backup_pos = ImGui::GetCursorScreenPos();

		ImGui::SetCursorScreenPos(number_bb.Min);
		ImGui::PushItemWidth(number_bb.GetWidth());
		char buf[64];
		ImGui::DataTypeFormatString(buf, 64, data_type, p_data, format);

		// ★ 修正: 入力欄に "##" をつけて内部ラベルを非表示にする
		if (ImGui::InputText("##edit_slider", buf, 64, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsDecimal)) {
			if (data_type == ImGuiDataType_Float) *(float*)p_data = (float)atof(buf);
			if (data_type == ImGuiDataType_S32)   *(int*)p_data = atoi(buf);
			storage->SetInt(id, 0);
			value_changed = true;
		}
		if (ImGui::IsItemDeactivatedAfterEdit()) storage->SetInt(id, 0);
		ImGui::PopItemWidth();

		// ★ 追加: カーソル位置を復元
		ImGui::SetCursorScreenPos(backup_pos);
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

// =========================================================================
// 数値スライダー系 (範囲指定用)
// =========================================================================
bool CustomUI::SliderRangeFloat(const char* label, float* v_current_min, float* v_current_max, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
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
	const float slider_right_offset = 120.0f; // 数値表示（Min - Max）のために少し幅を確保
	const float start_y = pos.y + label_size.y + 4.0f;
	const float slider_y_center = start_y + (widget_height * 0.5f);

	// レイアウトBB計算
	const ImRect frame_bb(pos.x, slider_y_center - (track_height * 0.5f), pos.x + w - slider_right_offset, slider_y_center + (track_height * 0.5f));
	const ImRect number_bb(frame_bb.Max.x + 8.0f, start_y, pos.x + w, start_y + widget_height);
	const ImRect total_bb(pos.x, pos.y, pos.x + w, start_y + widget_height);

	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, id, &total_bb)) return false;

	// マウス判定用 (frame_bbだけでなく全体の高さを考慮したヒットボックスを生成)
	const ImRect interaction_bb(frame_bb.Min.x, start_y, frame_bb.Max.x, start_y + widget_height);
	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(interaction_bb, id, &hovered, &held);

	bool value_changed = false;

	// 状態保持用 (0: 未選択, 1: Minノブを掴んでいる, 2: Maxノブを掴んでいる)
	ImGuiStorage* storage = ImGui::GetStateStorage();
	const ImGuiID active_knob_key = window->GetID("##range_active_knob");
	int active_knob = storage->GetInt(active_knob_key, 0);

	// 各ノブの現在のピクセル位置を計算
	float width_px = frame_bb.GetWidth();
	float range = v_max - v_min;
	if (range <= 0.0f) range = 1.0f;

	float px_min = frame_bb.Min.x + ((*v_current_min - v_min) / range) * width_px;
	float px_max = frame_bb.Min.x + ((*v_current_max - v_min) / range) * width_px;

	// ドラッグ・操作ロジック
	if (g.ActiveId == id) {
		float mouse_x = g.IO.MousePos.x;

		// ドラッグが始まった最初のフレームで、どちらのノブに近いかをロックする
		if (active_knob == 0) {
			float dist_to_min = ImAbs(mouse_x - px_min);
			float dist_to_max = ImAbs(mouse_x - px_max);
			// 距離が同じ（重なっている）場合はMinを優先、それ以外は近い方をターゲットにする
			active_knob = (dist_to_min <= dist_to_max) ? 1 : 2;
			storage->SetInt(active_knob_key, active_knob);
		}

		// マウス座標から数値を逆算
		float normalized_t = ImClamp((mouse_x - frame_bb.Min.x) / width_px, 0.0f, 1.0f);
		float new_val = v_min + normalized_t * range;

		// ターゲットに指定されたノブのみを動かし、もう片方を追い越さないようにガード
		if (active_knob == 1) {
			*v_current_min = ImMin(new_val, *v_current_max);
			value_changed = true;
		}
		else if (active_knob == 2) {
			*v_current_max = ImMax(new_val, *v_current_min);
			value_changed = true;
		}

		if (value_changed) {
			ImGui::MarkItemEdited(id);
		}
	}
	else {
		// マウスを離したらアクティブなノブの状態を完全にリセット
		if (active_knob != 0) {
			active_knob = 0;
			storage->SetInt(active_knob_key, 0);
		}
	}

	// -------------------------------------------------------------------------
	// 描画処理
	// -------------------------------------------------------------------------
	// 1. ラベルテキスト
	window->DrawList->AddText(pos, ImGui::GetColorU32(ImGuiCol_Text), label);

	// 2. トラック背景
	window->DrawList->AddRectFilled(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), track_height * 0.5f);

	// 既存コントロールに合わせた Lerp（滑らかなイージングアニメーション）処理
	static std::unordered_map<ImGuiID, ImVec2> s_slider_anims;
	ImVec2& anim_pos = s_slider_anims[id];
	if (anim_pos.x == 0.0f && anim_pos.y == 0.0f) { // 初期化
		anim_pos.x = px_min;
		anim_pos.y = px_max;
	}
	anim_pos.x = ImLerp(anim_pos.x, px_min, g.IO.DeltaTime * 14.0f);
	anim_pos.y = ImLerp(anim_pos.y, px_max, g.IO.DeltaTime * 14.0f);

	// 3. 選択中範囲のハイライト線
	ImVec2 fill_min(anim_pos.x, frame_bb.Min.y);
	ImVec2 fill_max(anim_pos.y, frame_bb.Max.y);
	ImU32 grab_color = ImGui::GetColorU32((g.ActiveId == id) ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);
	window->DrawList->AddRectFilled(fill_min, fill_max, grab_color, track_height * 0.5f);

	// 4. 各ノブ（丸）の描画
	// Minノブ
	window->DrawList->AddCircleFilled(ImVec2(anim_pos.x, slider_y_center), 6.0f, (g.ActiveId == id && active_knob == 1) ? ImGui::GetColorU32(ImGuiCol_SliderGrabActive) : ImGui::GetColorU32(ImGuiCol_SliderGrab));
	window->DrawList->AddCircle(ImVec2(anim_pos.x, slider_y_center), 6.0f, ImGui::GetColorU32(ImGuiCol_Border));
	// Maxノブ
	window->DrawList->AddCircleFilled(ImVec2(anim_pos.y, slider_y_center), 6.0f, (g.ActiveId == id && active_knob == 2) ? ImGui::GetColorU32(ImGuiCol_SliderGrabActive) : ImGui::GetColorU32(ImGuiCol_SliderGrab));
	window->DrawList->AddCircle(ImVec2(anim_pos.y, slider_y_center), 6.0f, ImGui::GetColorU32(ImGuiCol_Border));

	// 5. 右側の数値テキストボックス表示 [Min - Max]
	char val_buf[64];
	char fmt_min[24];
	char fmt_max[24];
	ImGui::DataTypeFormatString(fmt_min, IM_ARRAYSIZE(fmt_min), ImGuiDataType_Float, v_current_min, format ? format : "%.2f");
	ImGui::DataTypeFormatString(fmt_max, IM_ARRAYSIZE(fmt_max), ImGuiDataType_Float, v_current_max, format ? format : "%.2f");
	snprintf(val_buf, sizeof(val_buf), "%s - %s", fmt_min, fmt_max);

	window->DrawList->AddRectFilled(number_bb.Min, number_bb.Max, ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), 4.0f);
	window->DrawList->AddRect(number_bb.Min, number_bb.Max, ImGui::GetColorU32(ImGuiCol_Border), 4.0f);

	ImVec2 text_size = ImGui::CalcTextSize(val_buf);
	ImVec2 text_pos(number_bb.Min.x + (number_bb.GetWidth() - text_size.x) * 0.5f, number_bb.Min.y + (number_bb.GetHeight() - text_size.y) * 0.5f);
	window->DrawList->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), val_buf);

	return value_changed;
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

ImVec4  CustomUI::SimulateColorBlindness(const ImVec4& color, int mode) {
	float r = color.x, g = color.y, b = color.z;
	ImVec4 out = color;

	if (mode == 1) { // Protanopia (1型2色覚: 赤が感じにくい)
		out.x = 0.567f * r + 0.433f * g;
		out.y = 0.558f * r + 0.442f * g;
		out.z = 0.242f * g + 0.758f * b;
	}
	else if (mode == 2) { // Deuteranopia (2型2色覚: 緑が感じにくい)
		out.x = 0.625f * r + 0.375f * g;
		out.y = 0.700f * r + 0.300f * g;
		out.z = 0.300f * g + 0.700f * b;
	}

	// 0.0f 〜 1.0f の範囲にクランプ
	out.x = out.x > 1.0f ? 1.0f : (out.x < 0.0f ? 0.0f : out.x);
	out.y = out.y > 1.0f ? 1.0f : (out.y < 0.0f ? 0.0f : out.y);
	out.z = out.z > 1.0f ? 1.0f : (out.z < 0.0f ? 0.0f : out.z);
	return out;
}

// --- ColorEdit3 (Alphaなし版) ---
bool  CustomUI::ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags) {
	float col4[4] = { col[0], col[1], col[2], 1.0f };
	bool changed = ColorEdit4(label, col4, flags | ImGuiColorEditFlags_NoAlpha);
	if (changed) {
		col[0] = col4[0];
		col[1] = col4[1];
		col[2] = col4[2];
	}
	return changed;
}

// --- ColorEdit4 (フル機能・プロフェッショナル版) ---
bool  CustomUI::ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags) {
	bool value_changed = false;

	// 【重要】引数の label を使って一意のIDスコープを作成（ID衝突対策）
	ImGui::PushID(label);

	// --- 状態管理 (静的・永続変数) ---
	static std::vector<ImVec4> saved_palette;
	static ImVec4 backup_color;
	static bool dark_background_preview = true; // チェッカーボード背景のトグル

	// ウィジェット個別の状態（テーマ選択インデックス）をImGuiのストレージから取得
	ImGuiStorage* storage = ImGui::GetStateStorage();
	ImGuiID theme_var_id = ImGui::GetID("#theme_var_idx");
	int* theme_var_idx = storage->GetIntRef(theme_var_id, 0); // デフォルトは 0 (Custom)

	// パレットの初期化（初回のみ16マス分をグレーで確保）
	if (saved_palette.empty()) {
		saved_palette.resize(16, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
	}

	ImVec4 current_color_vec = ImVec4(col[0], col[1], col[2], col[3]);

	// --- スタイルの適用 ---
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(13.0f, 13.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 12.0f));
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::GetColorU32(ImGuiCol_FrameBg));

	// --- メインラインUI（カラーボタンとテキストラベル） ---
	if (ImGui::ColorButton("##ColorBtn", current_color_vec, flags | ImGuiColorEditFlags_NoPicker)) {
		ImGui::OpenPopup("ProColorPickerPopup");
		backup_color = current_color_vec; // 開いた瞬間の色を「旧色」として退避
	}
	ImGui::SameLine();
	ImGui::TextUnformatted(label);

	// --- カスタムポップアップ（左右2カラムレイアウト） ---
	ImGui::SetNextWindowSize(ImVec2(530, 0), ImGuiCond_Once);
	if (ImGui::BeginPopup("ProColorPickerPopup")) {

		// --------------------------------------------------
		// 左カラム: メインカラーピッカー領域
		// --------------------------------------------------
		ImGui::BeginGroup();
		{
			ImGui::PushItemWidth(220.0f); // ピッカーの横幅を固定

			ImGuiColorEditFlags picker_flags = flags |
				ImGuiColorEditFlags_DisplayHex |
				ImGuiColorEditFlags_DisplayRGB |
				ImGuiColorEditFlags_PickerHueWheel;

			// 背景チェッカーボードの切り替えフラグ
			if (!dark_background_preview) picker_flags |= ImGuiColorEditFlags_AlphaPreview;
			else picker_flags |= ImGuiColorEditFlags_AlphaPreviewHalf;

			value_changed |= ImGui::ColorPicker4("##picker", col, picker_flags);

			ImGui::Spacing();
			ImGui::Checkbox("Dark/Light Checkerboard", &dark_background_preview);
			ImGui::PopItemWidth();
		}
		ImGui::EndGroup();

		ImGui::SameLine(0, 18.0f); // 左右のカラム間の余白

		// --------------------------------------------------
		// 右カラム: 拡張機能領域（プレビュー、ハーモニー、パレット）
		// --------------------------------------------------
		ImGui::BeginGroup();
		{
			float right_column_width = 250.0f;
			ImGui::PushItemWidth(right_column_width);

			// 1. 変数バインディング (Theme Manager Mock)
			ImGui::TextDisabled("Theme Binding");
			const char* theme_vars[] = { "Custom (Unbound)", "Primary_Color", "Secondary_Color", "Accent_Color" };
			if (ImGui::Combo("##theme_bind", theme_var_idx, theme_vars, IM_ARRAYSIZE(theme_vars))) {
				if (*theme_var_idx != 0) {
					// モック処理: 選択した変数に応じて既定の色を強制適用
					if (*theme_var_idx == 1) { col[0] = 0.31f; col[1] = 0.47f; col[2] = 1.00f; col[3] = 1.0f; } // Blue
					if (*theme_var_idx == 2) { col[0] = 0.92f; col[1] = 0.25f; col[2] = 0.40f; col[3] = 1.0f; } // Pink
					if (*theme_var_idx == 3) { col[0] = 0.20f; col[1] = 0.85f; col[2] = 0.35f; col[3] = 1.0f; } // Green
					value_changed = true;
				}
			}
			ImGui::Spacing();

			// 2. 旧/新プレビュー ＆ 色覚シミュレーション
			ImGui::TextDisabled("Preview & Accessibility");
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImVec2 pos = ImGui::GetCursorScreenPos();
			float preview_height = 20.0f;

			// 通常の「旧色 / 新色」スプリットプレビュー
			ImU32 col_old = ImGui::ColorConvertFloat4ToU32(backup_color);
			ImU32 col_new = ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], col[3]));
			draw_list->AddRectFilled(pos, ImVec2(pos.x + right_column_width * 0.5f, pos.y + preview_height), col_old);
			draw_list->AddRectFilled(ImVec2(pos.x + right_column_width * 0.5f, pos.y), ImVec2(pos.x + right_column_width, pos.y + preview_height), col_new);

			// 透明なボタンを重ねて、クリック時に元の色へ戻すリセット機能を実装
			if (ImGui::InvisibleButton("##split_preview", ImVec2(right_column_width, preview_height))) {
				col[0] = backup_color.x; col[1] = backup_color.y; col[2] = backup_color.z; col[3] = backup_color.w;
				value_changed = true;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("クリックで元の色にリセット");

			// 色覚シミュレーション (P型 / D型を左右に並べて描画)
			pos = ImGui::GetCursorScreenPos();
			ImVec4 p_color = SimulateColorBlindness(ImVec4(col[0], col[1], col[2], col[3]), 1);
			ImVec4 d_color = SimulateColorBlindness(ImVec4(col[0], col[1], col[2], col[3]), 2);
			draw_list->AddRectFilled(pos, ImVec2(pos.x + right_column_width * 0.5f, pos.y + preview_height), ImGui::ColorConvertFloat4ToU32(p_color));
			draw_list->AddRectFilled(ImVec2(pos.x + right_column_width * 0.5f, pos.y), ImVec2(pos.x + right_column_width, pos.y + preview_height), ImGui::ColorConvertFloat4ToU32(d_color));
			ImGui::Dummy(ImVec2(right_column_width, preview_height));

			ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f), "Left: P型(赤視) | Right: D型(緑視)");
			ImGui::Spacing();

			// 3. カラーハーモニー (配色提案)
			ImGui::TextDisabled("Color Harmony Suggestions");
			float h, s, v;
			ImGui::ColorConvertRGBtoHSV(col[0], col[1], col[2], h, s, v);

			// ハーモニー色を計算してボタン化するラムダ式
			auto DrawHarmonyButton = [&](const char* id, float hue_offset) {
				float target_h = std::fmod(h + hue_offset, 1.0f);
				if (target_h < 0.0f) target_h += 1.0f;
				float r, g, b;
				ImGui::ColorConvertHSVtoRGB(target_h, s, v, r, g, b);

				if (ImGui::ColorButton(id, ImVec4(r, g, b, col[3]), ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoAlpha, ImVec2(24, 24))) {
					col[0] = r; col[1] = g; col[2] = b;
					*theme_var_idx = 0; // 色を手動変更したためバインドを解除
					value_changed = true;
				}
				};

			ImGui::Text("Comp/Tri: "); ImGui::SameLine();
			DrawHarmonyButton("##comp", 0.5f); ImGui::SameLine();   // 補色 (180度)
			DrawHarmonyButton("##tri1", 0.333f); ImGui::SameLine(); // トライアド (+120度)
			DrawHarmonyButton("##tri2", 0.666f);                    // トライアド (-120度)

			ImGui::Text("Analogous:"); ImGui::SameLine();
			DrawHarmonyButton("##ana1", 0.083f); ImGui::SameLine(); // 類似色 (+30度)
			DrawHarmonyButton("##ana2", -0.083f);                   // 類似色 (-30度)
			ImGui::Spacing();

			// 4. スウォッチ (カラーパレットの動的管理)
			ImGui::TextDisabled("Palette (Right-Click to save)");
			for (int i = 0; i < saved_palette.size(); ++i) {
				ImGui::PushID(i);
				if (i % 8 != 0) ImGui::SameLine(); // 8個ごとに折り返し描画

				if (ImGui::ColorButton("##swatch", saved_palette[i], ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoAlpha, ImVec2(24, 24))) {
					col[0] = saved_palette[i].x; col[1] = saved_palette[i].y; col[2] = saved_palette[i].z; col[3] = saved_palette[i].w;
					*theme_var_idx = 0; // バインド解除
					value_changed = true;
				}

				// スウォッチ上での右クリックコンテキストメニュー
				if (ImGui::BeginPopupContextItem("##save_swatch_ctx")) {
					if (ImGui::Selectable("現在の色をここに保存")) {
						saved_palette[i] = ImVec4(col[0], col[1], col[2], col[3]);
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();
			}

			ImGui::PopItemWidth();
		}
		ImGui::EndGroup();

		ImGui::EndPopup();
	}

	// --- スタイルの復元 ＆ IDスコープの破棄 ---
	ImGui::PopStyleVar(4);
	ImGui::PopStyleColor();
	ImGui::PopID();

	return value_changed;
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
// CustomWidgets.cpp

bool CustomUI::Vec3PositionPad(const char* label, float v[3], float min_val, float max_val, const ImVec2& size_arg)
{
	ImGui::PushID(label);
	bool changed = false;

	// 1. XY平面のパッドを表示 (既存のVec2PositionPadのロジックを流用)
	// 実際には Vec2PositionPad の内部処理を関数化して呼び出すのが理想的です
	changed |= Vec2PositionPad("##XYPad", v, min_val, max_val, size_arg);

	ImGui::SameLine();

	// 2. Z軸操作用の縦型スライダー
	ImGui::BeginGroup();
	ImGui::Text("Z");
	if (ImGui::VSliderFloat("##Z", ImVec2(30, size_arg.y > 0 ? size_arg.y : 100), &v[2], min_val, max_val))
	{
		changed = true;
	}
	ImGui::EndGroup();

	ImGui::PopID();
	return changed;
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
	ImVec2 backup_pos = ImGui::GetCursorScreenPos();
	// 制御点2のドラッグ判定 (手動計算に修正)
	ImGui::SetCursorScreenPos(ImVec2(p2.x - 6.0f, p2.y - 6.0f));
	ImGui::InvisibleButton("##p2_btn", ImVec2(12.0f, 12.0f));
	if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
		cp2.x = ImClamp((g.IO.MousePos.x - box_bb.Min.x) / box_size.x, 0.0f, 1.0f);
		cp2.y = ImClamp((box_bb.Max.y - g.IO.MousePos.y) / box_size.y, 0.0f, 1.0f);
		value_changed = true;
	}
	ImGui::SetCursorScreenPos(backup_pos);
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
bool CustomUI::EditableTimeline(const char* label, int* current_frame, int max_frames, std::vector<int>& keyframes, const ImVec2& size_arg) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) return false;

	ImGuiContext& g = *GImGui;
	const ImGuiID id = window->GetID(label);

	if (max_frames <= 0) max_frames = 1;

	ImVec2 pos = window->DC.CursorPos;
	ImVec2 size = ImGui::CalcItemSize(size_arg, ImGui::CalcItemWidth(), 40.0f);
	if (size.x <= 0.0f) size.x = 1.0f; // [追加] ゼロ除算防止（極端に幅が小さい場合の安全策）
	ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

	ImGui::ItemSize(size, g.Style.FramePadding.y);
	if (!ImGui::ItemAdd(bb, id)) return false;

	// [追加] 外部から不正な値（範囲外）が来てもクランプして安全に表示する
	*current_frame = ImClamp(*current_frame, 0, max_frames);

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
	bool value_changed = false;

	ImGuiIO& io = ImGui::GetIO();
	float mouse_x_in_widget = io.MousePos.x - pos.x;
	float t = ImClamp(mouse_x_in_widget / size.x, 0.0f, 1.0f);
	// [変更] 単純な切り捨てではなく四捨五入にすることで、見た目の位置と選択フレームの誤差を減らす
	int hovered_frame = (int)(t * max_frames + 0.5f);
	hovered_frame = ImClamp(hovered_frame, 0, max_frames);

	// ドラッグ中のキーフレームインデックスを記憶する
	ImGuiStorage* storage = ImGui::GetStateStorage();
	const ImGuiID drag_id = window->GetID("##kf_dragging_idx");
	int dragging_kf_idx = storage->GetInt(drag_id, -1);

	// [変更] 当たり判定を少し広げて掴みやすくする（6px -> 8px）
	const float snap_pixel_radius = 8.0f;

	// [追加] 「今どのキーフレームの上にマウスがあるか」を1回だけ判定し、
	// カーソル変更・ツールチップ・ハイライト・右クリック削除のすべてで共通利用する
	// （以前は左クリックと右クリックで別々に最近傍探索しており、判定基準が分かりにくかった）
	int hovered_kf_idx = -1;
	if (hovered) {
		float min_dist = snap_pixel_radius;
		for (size_t i = 0; i < keyframes.size(); ++i) {
			float kf_x = ((float)keyframes[i] / max_frames) * size.x;
			float dist = std::abs(kf_x - mouse_x_in_widget);
			if (dist < min_dist) {
				min_dist = dist;
				hovered_kf_idx = (int)i;
			}
		}
	}

	if (hovered) {
		// [追加] キーフレームの上ではカーソルを変えて「つかめる」ことを直感的に伝える
		if (hovered_kf_idx != -1) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		}

		// [変更] ツールチップに操作方法のヒントを追加して、初めて触る人でも分かるようにする
		if (hovered_kf_idx != -1) {
			ImGui::SetTooltip("Keyframe: %d\nLeft-drag: Move / Right-click: Remove", keyframes[hovered_kf_idx]);
		}
		else {
			ImGui::SetTooltip("Frame: %d\nRight-click: Add Keyframe / Shift+Drag: Snap", hovered_frame);
		}

		// 左クリックされた瞬間に、キーフレーム上かどうかを判定
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			if (hovered_kf_idx != -1) {
				// キーフレームを掴んだ
				dragging_kf_idx = hovered_kf_idx;
				storage->SetInt(drag_id, dragging_kf_idx);
			}
		}

		// 右クリックでキーフレームの追加 / 削除
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			if (hovered_kf_idx != -1) {
				keyframes.erase(keyframes.begin() + hovered_kf_idx);
			}
			else {
				keyframes.push_back(hovered_frame);
				std::sort(keyframes.begin(), keyframes.end());
			}
			value_changed = true;
		}
	}

	// [追加] ドラッグ中はウィジェット外にマウスが出てもハンドカーソルを維持する
	if (dragging_kf_idx != -1) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	}

	// --- 左ドラッグによる移動処理 ---
	int target_frame = hovered_frame;

	// Shiftキーを押している場合、近くのキーフレームにスナップ（吸着）させる
	if (io.KeyShift && !keyframes.empty()) {
		int nearest_kf = keyframes[0];
		int min_f_dist = std::abs(target_frame - nearest_kf);
		for (int kf : keyframes) {
			int d = std::abs(target_frame - kf);
			if (d < min_f_dist) {
				min_f_dist = d;
				nearest_kf = kf;
			}
		}
		// 全体の5%以内の距離ならスナップ
		if (min_f_dist < max_frames * 0.05f) {
			target_frame = nearest_kf;
		}
	}

	bool is_dragging_kf = (dragging_kf_idx >= 0 && dragging_kf_idx < (int)keyframes.size());

	if (is_dragging_kf) {
		// キーフレームをドラッグ中
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			if (keyframes[dragging_kf_idx] != target_frame) {
				keyframes[dragging_kf_idx] = target_frame;
				value_changed = true;
			}
		}
		else {
			// ドロップ（離した）時にソートして状態をリセット
			std::sort(keyframes.begin(), keyframes.end());
			storage->SetInt(drag_id, -1);
			dragging_kf_idx = -1;
		}
	}
	else if (held) {
		// 再生ヘッドをドラッグ中
		if (*current_frame != target_frame) {
			*current_frame = target_frame;
			value_changed = true;
		}
	}

	// --- 描画 (DrawList) ---
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->PushClipRect(bb.Min, bb.Max, true);

	const ImU32 bg_color = ImGui::GetColorU32(ImGuiCol_FrameBg);
	const ImU32 border_color = ImGui::GetColorU32(ImGuiCol_Border);
	draw_list->AddRectFilled(bb.Min, bb.Max, bg_color, g.Style.FrameRounding);
	draw_list->AddRect(bb.Min, bb.Max, border_color, g.Style.FrameRounding);

	// 目盛り
	int tick_step = max_frames / 10;
	if (tick_step < 1) tick_step = 1;
	for (int i = 0; i <= max_frames; i += tick_step) {
		float tx = pos.x + ((float)i / max_frames) * size.x;
		draw_list->AddLine(ImVec2(tx, pos.y + size.y - 8), ImVec2(tx, pos.y + size.y), ImGui::GetColorU32(ImGuiCol_TextDisabled), 1.0f);
	}

	// [追加] 始点・終点のフレーム番号を表示し、タイムライン全体の長さが一目で分かるようにする
	{
		char buf_start[16];
		char buf_end[16];
		ImFormatString(buf_start, sizeof(buf_start), "%d", 0);
		ImFormatString(buf_end, sizeof(buf_end), "%d", max_frames);

		ImU32 label_col = ImGui::GetColorU32(ImGuiCol_TextDisabled);
		float label_y = bb.Max.y - 8.0f - ImGui::GetFontSize();

		draw_list->AddText(ImVec2(bb.Min.x + 3.0f, label_y), label_col, buf_start);

		ImVec2 end_size = ImGui::CalcTextSize(buf_end);
		draw_list->AddText(ImVec2(bb.Max.x - end_size.x - 3.0f, label_y), label_col, buf_end);
	}

	// [追加] ホバー中はクリック/追加位置をプレビューする縦ガイドラインを表示する
	// （再生ヘッドのドラッグ中やキーフレームのドラッグ中は表示しない）
	if (hovered && !held && dragging_kf_idx == -1) {
		float guide_x = pos.x + ((float)hovered_frame / max_frames) * size.x;
		draw_list->AddLine(ImVec2(guide_x, pos.y), ImVec2(guide_x, pos.y + size.y), IM_COL32(255, 255, 255, 60), 1.0f);
	}

	// キーフレームの描画
	// [変更] 四角形 -> ひし形（ダイヤモンド）に変更し、「キーフレーム」であることを視覚的に分かりやすくする
	// また、ホバー/ドラッグ時にサイズも変化させてフィードバックを強化する
	const ImU32 kf_color = IM_COL32(255, 160, 0, 255);
	const ImU32 kf_color_hover = IM_COL32(255, 210, 80, 255);
	const ImU32 kf_color_drag = IM_COL32(255, 255, 255, 255); // ドラッグ中

	for (size_t i = 0; i < keyframes.size(); ++i) {
		float kf_x = pos.x + ((float)keyframes[i] / max_frames) * size.x;
		float kf_center_y = pos.y + (size.y - 8.0f) * 0.5f; // 下部の目盛りエリアを避けて中央寄りに配置

		bool is_kf_hovered = (hovered_kf_idx == (int)i);
		bool is_this_dragging = (dragging_kf_idx == (int)i);

		ImU32 col = kf_color;
		float radius = 4.0f;
		if (is_this_dragging) { col = kf_color_drag; radius = 6.0f; }
		else if (is_kf_hovered) { col = kf_color_hover; radius = 5.5f; }

		ImVec2 diamond[4] = {
			ImVec2(kf_x, kf_center_y - radius), // 上
			ImVec2(kf_x + radius, kf_center_y), // 右
			ImVec2(kf_x, kf_center_y + radius), // 下
			ImVec2(kf_x - radius, kf_center_y), // 左
		};

		draw_list->AddConvexPolyFilled(diamond, 4, col);
		draw_list->AddPolyline(diamond, 4, IM_COL32(0, 0, 0, 120), ImDrawFlags_Closed, 1.5f);
	}

	// 再生ヘッドの描画
	float head_x = pos.x + ((float)(*current_frame) / max_frames) * size.x;
	draw_list->AddLine(ImVec2(head_x, pos.y), ImVec2(head_x, pos.y + size.y), IM_COL32(255, 50, 50, 255), 2.0f);
	draw_list->AddTriangleFilled(
		ImVec2(head_x - 5, pos.y),
		ImVec2(head_x + 5, pos.y),
		ImVec2(head_x, pos.y + 6),
		IM_COL32(255, 50, 50, 255)
	);

	// [追加] 現在のフレーム番号を常時表示するカウンター（右上に半透明の背景付き）
	// これまではホバーしないと現在のフレームが分からなかったが、常に一目で確認できるようにする
	{
		char counter_buf[32];
		ImFormatString(counter_buf, sizeof(counter_buf), "%d / %d", *current_frame, max_frames);

		ImVec2 text_size = ImGui::CalcTextSize(counter_buf);
		ImVec2 text_pos(bb.Max.x - text_size.x - 6.0f, bb.Min.y + 3.0f);

		draw_list->AddRectFilled(
			ImVec2(text_pos.x - 4.0f, text_pos.y - 2.0f),
			ImVec2(text_pos.x + text_size.x + 4.0f, text_pos.y + text_size.y + 2.0f),
			IM_COL32(0, 0, 0, 140), 3.0f
		);
		draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), counter_buf);
	}

	draw_list->PopClipRect();

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


// 検索窓ウィジェット
bool CustomUI::SearchBar(const char* label, char* buf, size_t buf_size, const char* hint) {
	ImGui::PushID(label);
	ImGui::Text("Filter:");
	ImGui::SameLine();
	bool changed = ImGui::InputTextWithHint("##SearchInput", hint, buf, buf_size);
	ImGui::PopID();
	return changed;
}

// 画像ギャラリー
bool CustomUI::ImageGallery(const char* label, const std::vector<ImageAsset>& images, int* selected_index, float thumbnail_size) {
	bool selection_changed = false;

	// 状態管理（静的変数）
	static char filter_buf[128] = "";
	static bool show_full_preview = false;

	ImGui::PushID(label);

	// 1. 検索窓
	CustomUI::SearchBar("GallerySearch", filter_buf, IM_ARRAYSIZE(filter_buf), "Search images...");
	ImGui::Separator();

	// 2. サムネイル一覧エリア
	const float preview_height = 250.0f;
	ImGui::BeginChild("##GalleryGrid", ImVec2(0, -preview_height), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

	ImGuiStyle& style = ImGui::GetStyle();
	float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

	for (int i = 0; i < (int)images.size(); i++) {
		// フィルター処理
		if (filter_buf[0] != '\0' && images[i].name.find(filter_buf) == std::string::npos)
			continue;

		ImGui::PushID(i);
		bool is_selected = (*selected_index == i);

		if (is_selected) {
			ImGui::PushStyleColor(ImGuiCol_Button, theme.accent);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme.accent);
		}

		// サムネイルボタン
		if (ImGui::ImageButton("##thumb", images[i].tex_id, ImVec2(thumbnail_size, thumbnail_size))) {
			if (*selected_index != i) {
				*selected_index = i;
				selection_changed = true;
			}
		}

		if (is_selected) ImGui::PopStyleColor(2);

		// ダブルクリックで拡大ウィンドウを表示
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			show_full_preview = true;
		}

		// 折り返しレイアウト
		float last_button_x2 = ImGui::GetItemRectMax().x;
		float next_button_x2 = last_button_x2 + style.ItemSpacing.x + thumbnail_size;
		if (i + 1 < (int)images.size() && next_button_x2 < window_visible_x2)
			ImGui::SameLine();

		ImGui::PopID();
	}
	ImGui::EndChild();

	// 3. 下部固定プレビュー（元の機能）
	ImGui::Separator();
	ImGui::BeginChild("##EmbeddedPreview", ImVec2(0, 0), false);
	if (*selected_index >= 0 && *selected_index < (int)images.size()) {
		const auto& img = images[*selected_index];
		ImGui::Text("Selected: %s", img.name.c_str());

		ImVec2 avail = ImGui::GetContentRegionAvail();
		float aspect = img.size.x / img.size.y;
		ImVec2 draw_size = avail;
		if (avail.x / avail.y > aspect) draw_size.x = avail.y * aspect;
		else draw_size.y = avail.x / aspect;

		ImGui::Image(img.tex_id, draw_size);
	}
	ImGui::EndChild();

	// 4. 独立拡大ウィンドウ（自由移動・リサイズ）
	if (show_full_preview && *selected_index >= 0 && *selected_index < (int)images.size()) {
		const auto& img = images[*selected_index];

		// ウィンドウのパディングを少し絞ってスマートに
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
		ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

		// スクロールバーを消すフラグを追加
		if (ImGui::Begin("Full Image Preview", &show_full_preview, ImGuiWindowFlags_NoScrollbar)) {

			// --- ヘッダー部分：情報表示 ---
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 0.8f));
			ImGui::TextUnformatted(img.name.c_str());
			ImGui::PopStyleColor();

			ImGui::SameLine();
			ImGui::TextDisabled("- %.0f x %.0f", img.size.x, img.size.y);

			ImGui::Separator();
			ImGui::Spacing();

			// --- 画像エリア ---
			ImVec2 avail = ImGui::GetContentRegionAvail();

			// アスペクト比計算
			float aspect = img.size.x / img.size.y;
			ImVec2 draw_size = avail;
			if (avail.x / avail.y > aspect) draw_size.x = avail.y * aspect;
			else draw_size.y = avail.x / aspect;

			// 中央寄せのためのオフセット計算
			ImVec2 cursor_pos = ImGui::GetCursorPos();
			cursor_pos.x += (avail.x - draw_size.x) * 0.5f;
			cursor_pos.y += (avail.y - draw_size.y) * 0.5f;
			ImGui::SetCursorPos(cursor_pos);

			// 画像描画
			ImGui::Image(img.tex_id, draw_size);

		}
		ImGui::End();

		// スタイル設定を戻す
		ImGui::PopStyleVar();
	}

	ImGui::PopID();
	return selection_changed;
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
	// --- ローディングバーのテストコード ---
	ImGui::Text("Loading Bar Demo:");
	static float progress = 0.0f;

	// 時間経過で 0.0 から 1.0 まで自動で進むサンプル
	progress += deltaTime * 0.2f; // 5秒で満タン
	if (progress > 1.2f) progress = 0.0f; // 満タン後、少し待ってリセット

	// 手動コントロール用のボタン
	if (ImGui::Button("Reset progress")) progress = 0.0f;
	ImGui::SameLine();
	if (ImGui::Button("Set 100%")) progress = 1.0f;

	// ウィジェットの呼び出し（ラベルあり、標準サイズ）
	CustomUI::LoadingBar("System Initializing...", progress);

	// ラベルなしで太めのローディングバーを表示したい場合の例
	ImGui::Spacing();
	CustomUI::LoadingBar("##silent_bar", progress, ImVec2(0, 12.0f));
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
	static float range_min = 20.0f;
	static float range_max = 80.0f;
	CustomUI::SliderRangeFloat("Audio Frequency Range", &range_min, &range_max, 0.0f, 100.0f, "%.1f Hz");
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

	static float color_rgba[4] = { 0.31f, 0.47f, 1.0f,1.0f };
	CustomUI::ColorEdit4("Accent Color RGBA", color_rgba);

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
	CustomUI::SectionLabel("8. 3D Position Test");

	static float pos3d[3] = { 0.0f, 0.0f, 0.0f };
	if (CustomUI::Vec3PositionPad("Object Position", pos3d, -1.0f, 1.0f, ImVec2(120, 120)))
	{
		// ここで値変更時の処理（ログ出力やエンジンのパラメータ更新など）
	}
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

	ImGui::Spacing();
	// =========================================================================
	CustomUI::SectionLabel("7. Timeline Showcase");

	// 状態保持用のスタティック変数
	static int current_frame = 0;
	const int max_frames = 120; // 4秒分（30fps換算）

	// デモ用のキーフレーム位置（std::vectorに変更）
	static std::vector<int> my_keyframes = { 0, 15, 45, 60, 90, 120 };

	// 再生自動進行用のフラグ
	static bool is_playing = false;
	ImGui::Checkbox("Play", &is_playing); // テスト用に再生トグルを追加

	if (is_playing)
	{
		static float frame_timer = 0.0f;
		frame_timer += deltaTime; // ※外側で定義されている deltaTime を使用

		// 30fpsの速度（1フレームあたり約0.033秒）で再生ヘッドを進める
		if (frame_timer >= 1.0f / 30.0f)
		{
			current_frame++;
			frame_timer = 0.0f;
			if (current_frame > max_frames)
			{
				current_frame = 0; // ループ再生
			}
		}
	}

	// 編集可能なタイムラインのレンダリング
	// 関数名を EditableTimeline に変更したと仮定しています。
	// Timeline のまま実装を上書きした場合は CustomUI::Timeline に戻してください。
	bool changed = CustomUI::EditableTimeline("##MyTimeline", &current_frame, max_frames, my_keyframes, ImVec2(0, 40));

	if (changed)
	{
		// キーフレームが追加・削除されたり、シークバーが操作された時にここが呼ばれます
		// 必要であればログを出力するなどの処理を追加できます
	}
	// =========================================================================
	// 7. トーストシステムのレンダリング
	// =========================================================================
	CustomUI::RenderToasts(deltaTime);
}