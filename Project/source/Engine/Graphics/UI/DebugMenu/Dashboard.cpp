#include "Dashboard.h"


void Dashboard::Render() {
	if (ImGui::Begin("Nextgen Dashboard")) {
		RenderSideBar();
		ImGui::SameLine();
		RenderMainContent();
	}
	ImGui::End();
}

void Dashboard::RenderSideBar() {
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 10));
	ImGui::BeginChild("Sidebar", ImVec2(200, 0), false, ImGuiWindowFlags_NoBackground);

	// --- プロジェクト名の表示エリア ---
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10); // 上部に余白

	// 少し大きめのフォントを使用する場合の想定（事前にLoadFontが必要）
	// ImGui::PushFont(font_large); 
	ImGui::Indent(20);
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "AtmosFiore");
	ImGui::Unindent(20);

	// ImGui::PopFont();

	ImGui::Separator(); // ロゴの下に区切り線
	ImGui::Dummy(ImVec2(0, 15)); // 下のメニューとの余白
	ImGui::PopStyleVar();
	// ---------------------------------

	for (size_t i = 0; i < modules.size(); ++i) {
		bool is_selected = (active_mod_idx == i);

		// 項目を描画
		if (ImGui::Selectable(modules[i].name.c_str(), is_selected, ImGuiSelectableFlags_None, ImVec2(0, 30))) {
			active_mod_idx = (int)i;
		}

		// アクティブな左側のラインを描画
		if (is_selected) {
			ImVec2 p0 = ImGui::GetItemRectMin();
			ImVec2 p1 = ImGui::GetItemRectMax();
			ImGui::GetWindowDrawList()->AddRectFilled(p0, ImVec2(p0.x + 3, p1.y), IM_COL32(80, 150, 255, 255));
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();
}
void Dashboard::RenderMainContent() {
	// 1. モジュールが登録されているかチェック
	if (modules.empty()) {
		ImGui::Text("No modules registered.");
		return;
	}

	// インデックスが範囲内か再確認（念のため）
	if (active_mod_idx < 0 || active_mod_idx >= modules.size()) {
		active_mod_idx = 0;
	}

	ImGui::BeginChild("MainContent", ImVec2(0, 0), true);
	auto& current_mod = modules[active_mod_idx];

	// 2. サブタブが空でないかチェック
	if (current_mod.subtabs.empty()) {
		ImGui::Text("No subtabs in this module.");
	}
	else {
		// 安全なインデックス範囲チェック
		if (active_sub_idx < 0 || active_sub_idx >= current_mod.subtabs.size()) {
			active_sub_idx = 0;
		}

		// 上部タブの描画
		for (size_t i = 0; i < current_mod.subtabs.size(); ++i) {
			if (ImGui::Button(current_mod.subtabs[i].name.c_str())) {
				active_sub_idx = (int)i;
			}
			ImGui::SameLine();
		}
		ImGui::Dummy(ImVec2(0, 20));
		ImGui::Separator();

		// 3. 描画関数の呼び出し（念のためnullptrチェックも有効）
		if (current_mod.subtabs[active_sub_idx].render_func) {
			current_mod.subtabs[active_sub_idx].render_func(data);
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
	dash.RegisterModule({ "Camera",   {{"View",   [](SharedMetricsData& d) { Camera_Manager::instance().draw_imgui(); }}} });
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