#include "Dashboard.h"
#include "Icons.h"
#include "Engine/utilities/misc.h"
#include "Engine/utilities/high_resolution_timer.h"
#include "Engine/utilities/color_util.h"
#include "Engine/utilities/imgui_util.h"
#include "Engine/utilities/resource_monitor.h"
#include "Game/Scenes/scene.h"


#include "Engine/Audio/AudioSystem.h"

void Dashboard::Render() {
	if (ImGui::Begin("Dashboard", nullptr, ImGuiWindowFlags_NoTitleBar)) {
		RenderSideBar();
		ImGui::SameLine();
		RenderMainContent();
	}
	ImGui::End();
	_monitor.update();
}
void Dashboard::RenderSideBar() {
	ImGui::BeginChild("Sidebar", ImVec2(180, 0), false, ImGuiWindowFlags_NoBackground);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 sb_min = ImGui::GetWindowPos();
	ImVec2 sb_max = ImVec2(sb_min.x + ImGui::GetWindowWidth(), sb_min.y + ImGui::GetWindowHeight());

	// ① サイドバー全体の縦グラデーション（上部は少し青みのあるグレー、下部は深い黒）
	ImU32 col_sb_top = IM_COL32(28, 28, 33, 255);
	ImU32 col_sb_bot = IM_COL32(12, 12, 13, 255);
	draw_list->AddRectFilledMultiColor(sb_min, sb_max, col_sb_top, col_sb_top, col_sb_bot, col_sb_bot);

	// タイトル
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);
	ImGui::Indent(20);
	if (font_large) ImGui::PushFont(font_large);
	ImGui::TextColored(ImVec4(1, 1, 1, 1), "AtmosFiore");
	if (font_large) ImGui::PopFont();
	ImGui::Unindent(20);
	ImGui::Dummy(ImVec2(0, 20));

	if (active_mod_idx < 0 || active_mod_idx >= (int)modules.size()) {
		active_mod_idx = 0;
	}

	ImVec4 yellow = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
	ImU32 theme_accent = ImGui::ColorConvertFloat4ToU32(yellow);

	for (size_t i = 0; i < modules.size(); ++i) {
		bool is_selected = (active_mod_idx == (int)i);
		ImVec2 cursor = ImGui::GetCursorScreenPos();

		ImGui::InvisibleButton(modules[i].name.c_str(), ImVec2(160, 35));
		bool hovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked()) active_mod_idx = (int)i;

		if (is_selected || hovered) {
			// ② 項目の横フェードアウトグラデーション（右側に向かって透明に）
			ImU32 col_left = is_selected ? IM_COL32(45, 45, 52, 255) : IM_COL32(35, 35, 40, 150);
			ImU32 col_right = is_selected ? IM_COL32(45, 45, 52, 0) : IM_COL32(35, 35, 40, 0);

			draw_list->AddRectFilledMultiColor(
				cursor, ImVec2(cursor.x + 160, cursor.y + 35),
				col_left, col_right, col_right, col_left
			);

			if (is_selected) {
				draw_list->AddRectFilled(cursor, ImVec2(cursor.x + 3, cursor.y + 35), theme_accent, 2.0f);
			}
		}

		std::string display_text = std::string(get_icon(modules[i].name)) + "   " + modules[i].name;
		draw_list->AddText(ImVec2(cursor.x + 20, cursor.y + 10),
			is_selected ? IM_COL32(255, 255, 255, 255) : (hovered ? IM_COL32(200, 200, 200, 255) : IM_COL32(140, 140, 140, 255)),
			display_text.c_str());
	}
	ImGui::EndChild();
}

void Dashboard::RenderGroupCard(const char* label, ImVec2 size, std::function<void()> content) {
	// 背景を一旦透明にして、手動で美しいグラデーションを塗る
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::BeginChild(label, size, true, ImGuiWindowFlags_None);

	ImVec2 card_min = ImGui::GetWindowPos();
	ImVec2 card_max = ImVec2(card_min.x + ImGui::GetWindowWidth(), card_min.y + ImGui::GetWindowHeight());
	ImDrawList* dl = ImGui::GetWindowDrawList();

	// ⑤ カード自体の立体グラデーション（上部をわずかに明るく、下部を暗く）
	// ※角丸(ChildRounding)に対してAddRectFilledMultiColorは厳密には四角く描画されますが、
	// 背景が暗いため、肉眼でははみ出しがほぼ分からず綺麗に馴染みます。
	ImU32 card_top = IM_COL32(34, 34, 38, 255);
	ImU32 card_bot = IM_COL32(23, 23, 25, 255);
	dl->AddRectFilledMultiColor(card_min, card_max, card_top, card_top, card_bot, card_bot);

	// タイトル
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
	ImGui::TextColored(ImVec4(0.88f, 0.88f, 0.88f, 1.0f), "%s", label);

	// タイトル下のセパレータ（綺麗なイエローフェードアウト）
	ImVec2 cursor = ImGui::GetCursorScreenPos();
	float separator_width = ImGui::GetContentRegionAvail().x * 0.7f;

	ImVec4 color_vec = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
	ImU32 col_left = ImGui::ColorConvertFloat4ToU32(color_vec);
	color_vec.w = 0.0f; // RGBを維持したままアルファだけ0に
	ImU32 col_right = ImGui::ColorConvertFloat4ToU32(color_vec);

	float line_thickness = 2.0f;
	dl->AddRectFilledMultiColor(
		ImVec2(cursor.x, cursor.y + 4.0f),
		ImVec2(cursor.x + separator_width, cursor.y + 4.0f + line_thickness),
		col_left, col_right, col_right, col_left
	);

	ImGui::Dummy(ImVec2(0, 10.0f));

	ImGui::BeginGroup();
	content();
	ImGui::EndGroup();

	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void Dashboard::RenderMainContent() {

	if (modules.empty()) return;

	auto& current_mod = modules[active_mod_idx];
	ImGui::BeginChild("MainContent", ImVec2(0, 0), true);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 mc_min = ImGui::GetWindowPos();
	ImVec2 mc_max = ImVec2(mc_min.x + ImGui::GetWindowWidth(), mc_min.y + 42.0f); // ヘッダーの高さ分

	// ③ メインコンテンツ上部のヘッダーグラデーション（空間の分離）
	ImU32 mc_grad_top = IM_COL32(26, 26, 28, 255);
	ImU32 mc_grad_bot = IM_COL32(20, 20, 20, 0); // 下に向かって透明に消える
	dl->AddRectFilledMultiColor(mc_min, mc_max, mc_grad_top, mc_grad_top, mc_grad_bot, mc_grad_bot);

	if (active_sub_idx < 0 || active_sub_idx >= (int)current_mod.subtabs.size()) {
		active_sub_idx = 0;
	}

	const float tab_height = 32.0f;
	const float tab_pad_x = 14.0f;
	const float underline_h = 2.0f;

	ImVec4 yellow = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
	const ImU32 col_active = ImGui::ColorConvertFloat4ToU32(yellow);
	const ImU32 col_hover = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_TabHovered]);
	const ImU32 col_text_on = IM_COL32(255, 255, 255, 255);
	const ImU32 col_text_off = IM_COL32(140, 140, 140, 255);

	for (size_t i = 0; i < current_mod.subtabs.size(); ++i) {
		bool is_active = (active_sub_idx == (int)i);
		const char* label = current_mod.subtabs[i].name.c_str();

		float text_w = ImGui::CalcTextSize(label).x;
		float tab_w = text_w + tab_pad_x * 2.0f;

		ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton(label, ImVec2(tab_w, tab_height));

		bool hovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked()) active_sub_idx = (int)i;

		if (hovered && !current_mod.subtabs[i].tooltip.empty()) {
			ImGui::SetTooltip("%s", current_mod.subtabs[i].tooltip.c_str());
		}

		// ④ アクティブサブタブの「ネオングロウ（発光）」グラデーション
		if (is_active) {
			ImU32 glow_top = ImGui::ColorConvertFloat4ToU32(ImVec4(yellow.x, yellow.y, yellow.z, 0.00f)); // 上は透明
			ImU32 glow_bot = ImGui::ColorConvertFloat4ToU32(ImVec4(yellow.x, yellow.y, yellow.z, 0.12f)); // 下は12%の薄い黄色
			dl->AddRectFilledMultiColor(
				cursor, ImVec2(cursor.x + tab_w, cursor.y + tab_height),
				glow_top, glow_top, glow_bot, glow_bot
			);
		}

		// テキスト描画
		float text_x = cursor.x + tab_pad_x;
		float text_y = cursor.y + (tab_height - ImGui::GetTextLineHeight()) * 0.5f;
		dl->AddText(ImVec2(text_x, text_y), is_active ? col_text_on : col_text_off, label);

		// 下線
		ImVec2 line_p1(cursor.x, cursor.y + tab_height - underline_h);
		ImVec2 line_p2(cursor.x + tab_w, cursor.y + tab_height - underline_h);
		if (is_active) {
			dl->AddLine(line_p1, line_p2, col_active, underline_h);
		}
		else if (hovered) {
			dl->AddLine(line_p1, line_p2, col_hover, underline_h);
		}

		ImGui::SameLine(0, 4.0f);
	}

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

void Dashboard::InitializeUI() {
	auto& dash = Dashboard::Instance();



	dash.RegisterModule({ "Dashboard", {
		{"Monitor", "View system and resource metrics", [&](SharedMetricsData& d) { _monitor.render_ui(); }}
	} });

	dash.RegisterModule({ "Audio", {
		{"Editor", "View system and resource metrics", [&](SharedMetricsData& d) { AudioSystem::instance().DrawEffectGraphEditor(); }}
	} });

	dash.RegisterModule({ "Scene", {
		{"Manager", "Manage scene hierarchy and entities", [](SharedMetricsData& d) { Scene_Manager::instance().draw_gui(); }},
	} });

	dash.RegisterModule({ "DebugViews", {
		{"Debug Views", "Toggle visual debug overlays", [](SharedMetricsData& d) { Graphics_Core::instance().post_procss.drawDebugView(); }},
		{"Debug Logs",  "View application logs", [](SharedMetricsData& d) { draw_log_contents(); }},
	} });

	dash.RegisterModule({ "Rendering", {
		{"Bloom",       "Adjust bloom settings", [](SharedMetricsData& d) { Graphics_Core::instance().post_procss.drawBloomGUI(); }},
		{"Adaptation",  "Adjust eye adaptation parameters", [](SharedMetricsData& d) { Graphics_Core::instance().post_procss.drawAdaptationGUI(); }},
		{"Tone Mapping","Adjust tone mapping curves", [](SharedMetricsData& d) { Graphics_Core::instance().post_procss.drawToneMappingGUI(); }},
		{"Exposure","Adjust tone mapping curves", [](SharedMetricsData& d) { Graphics_Core::instance().post_procss.drawExposureGUI(); }},
		{"LensImperfections","Adjust tone mapping curves", [](SharedMetricsData& d) { Graphics_Core::instance().post_procss.drawLensImperfectionsGUI(); }},
		{"Fog","Adjust tone mapping curves", [](SharedMetricsData& d) { Graphics_Core::instance().post_procss.drawFogGUI(); }}

	} });

	dash.RegisterModule({ "Light", {
		{"Point", "Manage point lights", [](SharedMetricsData& d) { Graphics_Core::instance().get_point_light_manager().draw_imgui(); }},
		{"Spot",  "Manage spot lights", [](SharedMetricsData& d) { Graphics_Core::instance().get_spot_light_manager().draw_imgui(); }},
		{"Area",  "Manage area lights", [](SharedMetricsData& d) { Graphics_Core::instance().get_area_light_manager().draw_imgui(); }},
	} });

	dash.RegisterModule({ "Resource", {
		{"Shaders", "Manage and hot-reload shaders", [](SharedMetricsData& d) { Resource_Manager::instance().shader_manager.draw_imgui(); }},
		{"Models",  "View loaded 3D models", [](SharedMetricsData& d) { Resource_Manager::instance().model_manager.draw_imgui(); }},
	} });

	dash.RegisterModule({ "Camera", {
		{"Setting", "Adjust camera properties", [](SharedMetricsData& d) { Camera_Manager::instance().get_active_camera()->get_camera().DrawCameraSettingsUI(); }},
		{"View",    "View camera transform info", [](SharedMetricsData& d) { Camera_Manager::instance().draw_imgui(); }},
	} });

	dash.RegisterModule({ "UITest", {
		{"Setting", "Test custom UI widgets", [](SharedMetricsData& d) { CustomUI::DrawCustomUIWidgetsTestWindow(); }},
		{"View",    "Test UI Camera view", [](SharedMetricsData& d) { Camera_Manager::instance().draw_imgui(); }},
	} });
}

void Dashboard::SetupStyle() {
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	// --- 余白の拡張 ---
	style.WindowPadding = ImVec2(16.0f, 16.0f);
	style.FramePadding = ImVec2(12.0f, 6.0f);
	style.ItemSpacing = ImVec2(10.0f, 12.0f);
	style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);

	style.WindowRounding = 8.0f;
	style.FrameRounding = 4.0f;
	style.GrabRounding = 4.0f;
	style.ChildRounding = 6.0f;
	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;
	style.PopupBorderSize = 1.0f;
	style.TabRounding = 4.0f;
	style.ScrollbarSize = 12.0f;
	style.ScrollbarRounding = 6.0f;

	colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
	colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

	colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

	ImVec4 yellow = ImVec4(0.95f, 0.75f, 0.10f, 1.00f);
	ImVec4 yellow_hover = ImVec4(0.95f, 0.75f, 0.10f, 0.60f);
	ImVec4 yellow_active = ImVec4(0.95f, 0.75f, 0.10f, 0.90f);

	colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

	colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = yellow_hover;
	colors[ImGuiCol_ButtonActive] = yellow;

	colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
	colors[ImGuiCol_TabHovered] = yellow_hover;
	colors[ImGuiCol_TabActive] = yellow;

	colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = yellow_hover;
	colors[ImGuiCol_HeaderActive] = yellow;

	colors[ImGuiCol_SliderGrab] = yellow_hover;
	colors[ImGuiCol_SliderGrabActive] = yellow;
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = yellow_hover;
	colors[ImGuiCol_ScrollbarGrabActive] = yellow;

	colors[ImGuiCol_CheckMark] = yellow;
	colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
}