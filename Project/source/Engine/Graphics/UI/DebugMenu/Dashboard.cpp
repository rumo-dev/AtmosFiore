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
	ImGui::BeginChild("Sidebar", ImVec2(150, 0), true);
	for (size_t i = 0; i < modules.size(); ++i) {
		if (ImGui::Selectable(modules[i].name.c_str(), active_mod_idx == i)) {
			active_mod_idx = (int)i;
			active_sub_idx = 0; // カテゴリ変更時は最初のサブタブへ
		}
	}
	ImGui::EndChild();
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
		{"Manager", [](SharedMetricsData& d) { Scene_Manager::instance().draw_gui(); }}
	} });

	// Rendering
	dash.RegisterModule({ "Rendering", {
		{"PostProcess", [](SharedMetricsData& d) { Graphics_Core::instance().post_procss.renderGUI(); }}
	} });

	// Light (複数のサブタブを登録)
	dash.RegisterModule({ "Light", {
		{"Point", [](SharedMetricsData& d) { Graphics_Core::instance().get_point_light_manager().draw_imgui(); }},
		{"Spot",  [](SharedMetricsData& d) { Graphics_Core::instance().get_spot_light_manager().draw_imgui(); }},
		{"Area",  [](SharedMetricsData& d) { Graphics_Core::instance().get_area_light_manager().draw_imgui(); }}
	} });

	// その他のモジュールも同様に追加...
	dash.RegisterModule({ "Resource", {{"Assets", [](SharedMetricsData& d) { Resource_Manager::instance().draw_imgui(); }}} });
	dash.RegisterModule({ "Camera",   {{"View",   [](SharedMetricsData& d) { Camera_Manager::instance().draw_imgui(); }}} });
}