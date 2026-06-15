#pragma once
#include "Engine/Graphics/UI/ImGui/imgui.h"

#include "Engine/Graphics/UI/ImGui/imgui_internal.h"
#include <string>
#include <vector>

namespace CustomUI
{
	struct Theme {
		ImU32 bg = IM_COL32(20, 20, 25, 255);
		ImU32 accent = IM_COL32(80, 120, 255, 255);
		ImU32 text = IM_COL32(200, 200, 200, 255);
		float rounding = 4.0f;
	};
	static Theme theme;

	struct ImageAsset {
		ImTextureID tex_id;
		ImVec2 size;
		std::string name;
	};
	// トースト通知用の構造体
	struct Toast {
		std::string message;
		ImGuiCol color_type; // ImGuiCol_PlotLinesHovered (Info), ImGuiCol_TextMuted 等で代用
		float duration;      // 表示期間（秒）
		float max_duration;
	};
	static std::vector<Toast> toasts;

	// 1. 基本装飾
	void SectionLabel(const char* label);
	void GradientHeader(const char* label, bool* is_open);
	void Separator();
	void LoadingBar(const char* label, float fraction, const ImVec2& size_arg = ImVec2(0, 0));
	// 2. 基本コントロール
	bool Button(const char* label, const ImVec2& size_arg = ImVec2(0, 0));
	bool Checkbox(const char* label, bool* v);

	// 3. スライダー / ドラッグ
	bool SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format = nullptr, ImGuiSliderFlags flags = 0);
	bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.1f", ImGuiSliderFlags flags = 0);
	bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);
	bool SliderRangeFloat(const char* label, float* v_current_min, float* v_current_max, float v_min, float v_max, const char* format = "%.2f", ImGuiSliderFlags flags = 0);
	bool DragScalar(const char* label, ImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags = 0);
	bool DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags = 0);

	// 4. コンボボックス
	bool BeginCombo(const char* label, const char* preview_value, ImGuiComboFlags flags = 0);
	void EndCombo();
	bool Combo(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items = -1);
	bool Combo(const char* label, int* current_item, const char* items_separated_by_zeros, int height_in_items = -1);

	// 5. テキスト / カラー
	bool InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback cb = nullptr, void* user_data = nullptr);
	bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0);
	bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);

	// 6. 空間・データ可視化（新規追加）
	bool RotationDial(const char* label, float* p_angle_rad, float radius, float min_rad, float max_rad);
	bool Vec2PositionPad(const char* label, float v[2], float min_val = -1.0f, float max_val = 1.0f, const ImVec2& size_arg = ImVec2(0, 0));
	void MiniPerformanceGraph(const char* label, const float* values, int values_count, float scale_min = FLT_MAX, float scale_max = FLT_MAX, const ImVec2& size_arg = ImVec2(0, 0));
	bool CurveEasingEditor(const char* label, ImVec2& cp1, ImVec2& cp2, const ImVec2& size_arg = ImVec2(0, 0));

	// 変更後
	bool EditableTimeline(const char* label, int* current_frame, int max_frames, std::vector<int>& keyframes, const ImVec2& size_arg);
	// 7. 特殊通知
	void AddToast(const std::string& message, ImGuiCol type_color, float duration);
	void RenderToasts(float delta_time);

	void DrawCustomUIWidgetsTestWindow();


	// 検索窓ウィジェット（再利用可能なパーツ）
	bool SearchBar(const char* label, char* buf, size_t buf_size, const char* hint = "Search...");

	// 画像ギャラリー
	bool ImageGallery(const char* label, const std::vector<ImageAsset>& images, int* selected_index, float thumbnail_size);

}
