#include "DebugMenu.h"
#include "Engine/Graphics/UI/ImGui/imgui_internal.h"

void DebugMenu::Render(SharedDebugData& data) {
	SetupNeverloseStyle();

	// Beginに名前をつけることで、マルチビューポートで「Debug Dashboard」として外に出せる
	// "###DebugMenu" をつけると内部IDが固定され、位置が保存されます
	if (ImGui::Begin("Debug Dashboard###DebugMenu", nullptr, ImGuiWindowFlags_NoCollapse)) {
		RenderDashboard(data);
	}
	ImGui::End();
}

void DebugMenu::RenderDashboard(SharedDebugData& data) {
	std::lock_guard<std::mutex> lock(data.mtx);

	ImGui::Text("FPS: %.1f", data.fps);
	ImGui::Separator();

	// Neverlose風パーツの描画
	ImDrawList* draw = ImGui::GetWindowDrawList();
	ImVec2 pos = ImGui::GetCursorScreenPos();

	// グラフ描画エリアの確保
	ImGui::Dummy(ImVec2(200, 50));
	draw->AddRectFilled(pos, ImVec2(pos.x + 200, pos.y + 50), IM_COL32(30, 30, 35, 255), 4.0f);

	float step = 200.0f / 50.0f;
	for (size_t i = 0; i < data.graph_history.size() - 1; i++) {
		ImVec2 p1 = ImVec2(pos.x + (i * step), pos.y + 50 - (data.graph_history[i] * 50));
		ImVec2 p2 = ImVec2(pos.x + ((i + 1) * step), pos.y + 50 - (data.graph_history[i + 1] * 50));
		draw->AddLine(p1, p2, IM_COL32(80, 160, 255, 255), 2.0f);
	}
}

void DebugMenu::SetupNeverloseStyle() {
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