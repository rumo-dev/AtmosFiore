#include "Dashboard.h"
#include "Engine/Graphics/UI/ImGui/imgui_internal.h"
#include <cmath>

// 全体のスタイル設定（画像の色味と角丸を再現）
void Dashboard::SetupBaseStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ChildRounding = 6.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;

    // カラーパレット
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.13f, 1.00f);

    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);

    // フレーム・ボタン系
    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.21f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.23f, 0.23f, 0.26f, 1.00f);

    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

    // スライダー・グラブ（アクセントカラー：薄い青）
    colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
}

// カテゴリ見出し
void Dashboard::CustomHeader(const char* label) {
    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "%s", label);
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 5));
}

// モダンなトグルスイッチ
bool Dashboard::CustomToggle(const char* label, bool* v) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const float height = ImGui::GetFrameHeight();
    const float width = height * 1.6f;
    const float radius = height * 0.45f;

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));
    ImGui::ItemSize(bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) *v = !(*v);

    // 描画
    ImDrawList* draw = window->DrawList;
    ImU32 col_bg = *v ? IM_COL32(60, 140, 255, 255) : IM_COL32(50, 50, 55, 255);
    draw->AddRectFilled(bb.Min, bb.Max, col_bg, height * 0.5f);

    float knob_x = *v ? (bb.Max.x - radius - 2) : (bb.Min.x + radius + 2);
    draw->AddCircleFilled(ImVec2(knob_x, bb.Min.y + height * 0.5f), radius - 1, IM_COL32(255, 255, 255, 255));

    ImGui::SameLine(0, 10);
    ImGui::Text("%s", label);
    return pressed;
}

// モダンなスライダー
void Dashboard::CustomSlider(const char* label, float* v, float v_min, float v_max) {
    ImGui::Text("%s", label);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
    ImGui::PushItemWidth(100);
    ImGui::SliderFloat(("##" + std::string(label)).c_str(), v, v_min, v_max, "%.1f");
    ImGui::PopItemWidth();
}

// 左側のアイコンメニュー
void Dashboard::RenderSideBar() {
    ImGui::BeginChild("Sidebar", ImVec2(180, 0), true, ImGuiWindowFlags_NoScrollbar);

    // ロゴ（適当なフォントウェイトで）
    ImGui::Dummy(ImVec2(0, 20));
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "  NEVERLOSE");
    ImGui::Dummy(ImVec2(0, 30));

    // アイコン風のラベル（実際はFontAwesomeなどを使うが、ここではText）
    ImGui::TextDisabled("Aimbot");
    ImGui::Selectable("  [R] Ragebot", true);
    ImGui::Selectable("  [A] Anti-aim");
    ImGui::Selectable("  [L] Legitbot");

    ImGui::Dummy(ImVec2(0, 20));
    ImGui::TextDisabled("Visuals");
    ImGui::Selectable("  [P] Players");
    ImGui::Selectable("  [W] World");

    ImGui::Dummy(ImVec2(0, 20));
    ImGui::TextDisabled("Miscellaneous");
    ImGui::Selectable("  [M] Main");
    ImGui::Selectable("  [S] Scripts");

    ImGui::EndChild();
}

// 右側のメインコンテンツ（画像の中央〜右部分）
void Dashboard::RenderMainContent(SharedMetricsData& data) {
    ImGui::BeginChild("MainContent", ImVec2(0, 0), false);

    // 1. TopBar
    ImGui::BeginChild("TopBar", ImVec2(0, 60), true);
    ImGui::Dummy(ImVec2(0, 5));
    ImGui::SetCursorPosX(10);
    ImGui::Button("Save"); ImGui::SameLine();
    ImGui::Button("General"); ImGui::SameLine();
    ImGui::Button("Anti aim"); ImGui::SameLine();
    ImGui::Button("Subtab");
    ImGui::EndChild(); // TopBar終了

    // 2. ScrollableContent
    ImGui::BeginChild("ScrollableContent", ImVec2(0, 0), false);

    // カラム開始
    ImGui::Columns(2, "ContentColumns", false);
    ImGui::SetColumnWidth(0, 320);

    // --- Featherエリア ---
    ImGui::BeginChild("FeatherFrame", ImVec2(0, 280), true);
    CustomHeader("Feather");
    {
        std::lock_guard<std::mutex> lock(data.mtx);
        CustomToggle("Skoro t'ma otstypit", &data.feature_toggles["FeatherEnabled"]);
        CustomSlider("Slider", &data.feature_values["FeatherSlider"], 0.0f, 100.0f);

        // グラフ描画
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        draw->AddRectFilled(pos, ImVec2(pos.x + 300, pos.y + 60), IM_COL32(35, 35, 40, 255), 4.0f);
        float step = 300.0f / 50.0f;
        for (size_t i = 0; i < data.graph_history.size() - 1; i++) {
            ImVec2 p1 = ImVec2(pos.x + (i * step), pos.y + 60 - (data.graph_history[i] * 60));
            ImVec2 p2 = ImVec2(pos.x + ((i + 1) * step), pos.y + 60 - (data.graph_history[i + 1] * 60));
            draw->AddLine(p1, p2, IM_COL32(80, 160, 255, 255), 2.0f);
        }
        ImGui::Dummy(ImVec2(300, 60));
    }
    ImGui::EndChild(); // FeatherFrame終了

    ImGui::NextColumn();

    // --- Crownエリア ---
    ImGui::BeginChild("CrownFrame", ImVec2(0, 320), true);
    CustomHeader("Crown");
    {
        for (int i = 1; i <= 11; ++i) {
            std::string key = "CrownToggle_" + std::to_string(i);
            std::string label = "Toggle " + std::to_string(i);
            std::lock_guard<std::mutex> lock(data.mtx);
            CustomToggle(label.c_str(), &data.feature_toggles[key]);
        }
    }
    ImGui::EndChild(); // CrownFrame終了

    ImGui::Columns(1);
    ImGui::EndChild(); // ScrollableContent終了
    ImGui::EndChild(); // MainContent終了
}

// メインのRender関数
void Dashboard::Render(SharedMetricsData& data) {
    SetupBaseStyle();

    // ###NextgenDashboard というIDをつけることで、マルチビューポート分離時の設定が保存されます
    if (ImGui::Begin("Nextgen Dashboard###NextgenDashboard", nullptr, ImGuiWindowFlags_NoCollapse)) {
        // レイアウト：サイドバーとメインコンテンツをSameLineで並べる
        RenderSideBar();
        ImGui::SameLine();
        RenderMainContent(data);
    }
    ImGui::End();
}