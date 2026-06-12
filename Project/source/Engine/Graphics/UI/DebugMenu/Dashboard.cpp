#include "Dashboard.h"
#include "Icons.h"


void Dashboard::Render() {
	if (ImGui::Begin("Dashboard", nullptr, ImGuiWindowFlags_NoTitleBar)) {
		RenderSideBar();
		ImGui::SameLine();
		RenderMainContent();
	}
	ImGui::End();
}
void Dashboard::RenderSideBar() {
	ImGui::BeginChild("Sidebar", ImVec2(180, 0), false, ImGuiWindowFlags_NoBackground);

	// タイトル
// --- 修正箇所 ---
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);
	ImGui::Indent(20);

	// font_large を事前に保持していると仮定
	ImGui::PushFont(font_large);
	ImGui::TextColored(ImVec4(1, 1, 1, 1), "AtmosFiore");
	ImGui::PopFont();

	ImGui::Unindent(20);
	// ----------------

	ImGui::Dummy(ImVec2(0, 20));

	ImDrawList* draw_list = ImGui::GetWindowDrawList();


	for (size_t i = 0; i < modules.size(); ++i) {
		bool is_selected = (active_mod_idx == (int)i);
		ImVec2 cursor = ImGui::GetCursorScreenPos();

		ImGui::InvisibleButton(modules[i].name.c_str(), ImVec2(160, 35));
		if (ImGui::IsItemClicked()) active_mod_idx = (int)i;

		if (is_selected) {
			draw_list->AddRectFilled(cursor, ImVec2(cursor.x + 160, cursor.y + 35), IM_COL32(30, 30, 35, 255), 4.0f);
			draw_list->AddRectFilled(cursor, ImVec2(cursor.x + 3, cursor.y + 35), IM_COL32(80, 150, 255, 255), 2.0f);
		}

		// アイコンとテキストを結合して描画
		std::string display_text = std::string(get_icon(modules[i].name)) + "   " + modules[i].name;

		draw_list->AddText(ImVec2(cursor.x + 20, cursor.y + 10),
			is_selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(150, 150, 150, 255),
			display_text.c_str());
	}
	ImGui::EndChild();

}



void Dashboard::RenderGroupCard(const char* label, ImVec2 size, std::function<void()> content) {
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.13f, 1.0f));
	ImGui::BeginChild(label, size, false, ImGuiWindowFlags_None);

	// タイトルと装飾
	ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", label);
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 10));

	content(); // ここにトグルやスライダーを配置

	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void Dashboard::RenderMainContent() {
	if (modules.empty()) return;

	auto& current_mod = modules[active_mod_idx];

	ImGui::BeginChild("MainContent", ImVec2(0, 0), true);

	if (active_sub_idx < 0 || active_sub_idx >= (int)current_mod.subtabs.size()) {
		active_sub_idx = 0;
	}

	// ── サブタブ（アンダーライン型） ──────────────────────────────
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const float tab_height = 32.0f;
	const float tab_pad_x = 14.0f;   // タブ左右の余白
	const float underline_h = 2.0f;    // 下線の太さ
	const ImU32 col_active = IM_COL32(80, 150, 255, 255);   // 選択中：青
	const ImU32 col_hover = IM_COL32(80, 150, 255, 120);   // ホバー：薄青
	const ImU32 col_text_on = IM_COL32(255, 255, 255, 255);
	const ImU32 col_text_off = IM_COL32(150, 150, 150, 255);

	for (size_t i = 0; i < current_mod.subtabs.size(); ++i) {
		bool is_active = (active_sub_idx == (int)i);
		const char* label = current_mod.subtabs[i].name.c_str();

		// テキスト幅からタブ幅を決定
		float text_w = ImGui::CalcTextSize(label).x;
		float tab_w = text_w + tab_pad_x * 2.0f;

		ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton(label, ImVec2(tab_w, tab_height));

		bool hovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked()) active_sub_idx = (int)i;

		// テキスト描画（垂直中央）
		float text_x = cursor.x + tab_pad_x;
		float text_y = cursor.y + (tab_height - ImGui::GetTextLineHeight()) * 0.5f;
		dl->AddText(ImVec2(text_x, text_y),
			is_active ? col_text_on : col_text_off, label);

		// 下線
		ImVec2 line_p1(cursor.x, cursor.y + tab_height - underline_h);
		ImVec2 line_p2(cursor.x + tab_w, cursor.y + tab_height - underline_h);
		if (is_active) {
			dl->AddLine(line_p1, line_p2, col_active, underline_h);
		}
		else if (hovered) {
			dl->AddLine(line_p1, line_p2, col_hover, underline_h);
		}

		ImGui::SameLine(0, 4.0f);   // タブ間隔 4px
	}
	// ─────────────────────────────────────────────────────────────

	ImGui::Dummy(ImVec2(0, 0));     // SameLine の後に改行
	ImGui::Dummy(ImVec2(0, 8.0f));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 10.0f));

	// コンテンツ描画
	if (!current_mod.subtabs.empty()) {
		auto& target_tab = current_mod.subtabs[active_sub_idx];
		if (target_tab.render_func) {
			target_tab.render_func(data);
		}
		else {
			ImGui::Text("Error: Render function is null.");
		}
	}

	ImGui::EndChild();
}
#include "Engine/utilities/misc.h"
#include "Engine/utilities/high_resolution_timer.h"
#include "Engine/utilities/color_util.h"
#include "Engine/utilities/imgui_util.h"
#include "Engine/utilities/resource_monitor.h"

#include "Game/Scenes/scene.h"
void Dashboard::InitializeUI() {
	auto& dash = Dashboard::Instance();
	// Dashboard本体の登録

	/// リソース監視（メモリ・GPUなど）
	_monitor.update(); // 初回更新
	dash.RegisterModule({ "Dashboard", {
		{"Monitor", [&](SharedMetricsData& d) { _monitor.render_ui(); }}
	} });

	// Scene
	dash.RegisterModule({ "Scene", {
		{"Manager", [](SharedMetricsData& d) { Scene_Manager::instance().draw_gui(); }},
	} });

	// Rendering
	dash.RegisterModule({ "DebugViews", {
		{"Debug Views", [](SharedMetricsData& d) { Graphics_Core::instance().post_procss.drawDebugView(); }},
		{"Debug Logs", [](SharedMetricsData& d) {draw_log_contents(); }},
		} });
	dash.RegisterModule({ "Rendering", {
		{"Bloom", [](SharedMetricsData& d) { Graphics_Core::instance().post_procss.drawBloomGUI(); }},
		{"Adaptation", [](SharedMetricsData& d) { Graphics_Core::instance().post_procss.drawAdaptationGUI(); }},
		{"Tone Mapping", [](SharedMetricsData& d) { Graphics_Core::instance().post_procss.drawToneMappingGUI(); }}
	} });

	// Light (複数のサブタブを登録)
	dash.RegisterModule({ "Light", {
		{"Point", [](SharedMetricsData& d) { Graphics_Core::instance().get_point_light_manager().draw_imgui(); }},
		{"Spot",  [](SharedMetricsData& d) { Graphics_Core::instance().get_spot_light_manager().draw_imgui(); }},
		{"Area",  [](SharedMetricsData& d) { Graphics_Core::instance().get_area_light_manager().draw_imgui(); }}
	} });

	// その他のモジュールも同様に追加...
	dash.RegisterModule({ "Resource", {
		{"Shaders", [](SharedMetricsData& d) { Resource_Manager::instance().shader_manager.draw_imgui(); }},
		{"Models",  [](SharedMetricsData& d) { Resource_Manager::instance().model_manager.draw_imgui(); }},

		} });

	dash.RegisterModule({ "Camera",   {

		{"Setting",   [](SharedMetricsData& d) { Camera_Manager::instance().get_active_camera()->get_camera().DrawCameraSettingsUI(); }},
		{"View",   [](SharedMetricsData& d) { Camera_Manager::instance().draw_imgui(); }},

		} });
	dash.RegisterModule({ "UITest",   {

		{"Setting",   [](SharedMetricsData& d) {CustomUI::DrawCustomUIWidgetsTestWindow(); }},
		{"View",   [](SharedMetricsData& d) { Camera_Manager::instance().draw_imgui(); }},

		} });
}


void Dashboard::SetupStyle() {
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	// 基本設定
	style.WindowRounding = 8.0f;
	style.FrameRounding = 4.0f;
	style.GrabRounding = 4.0f;
	style.ChildRounding = 6.0f;
	style.WindowBorderSize = 0.0f;
	style.FrameBorderSize = 0.0f;
	style.PopupBorderSize = 0.0f;
	style.TabRounding = 4.0f;

	// カラーパレット：Neverloseのような暗いダークテーマ
	colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.13f, 1.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);

	// テキスト
	colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

	// フレーム・ボタン系（アクセントカラー：薄い青）
	colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.19f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);

	colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f); // 青みのホバー
	colors[ImGuiCol_ButtonActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);

	// タブ・ヘッダー
	colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.13f, 1.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_TabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

	// ヘッダー（メニュー項目など）
	colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.45f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);


}