#include "framework.h"
#include"graphics_core.h"
#include "Engine/system/manager/resource_manager.h"
#include "Engine/Utilities/misc.h"

Frame_Work::Frame_Work(HWND hwnd) : hwnd(hwnd)
{}

bool Frame_Work::initialize()

{
	Graphics_Core::instance().initialize(hwnd);
	Graphics_Core::instance().set_borderless_window();

	//static ImGui_Util::ThemeType theme = ImGui_Util::ThemeType::seeing;
	//ImGui_Util::set_theme(theme);

	Scene_Manager::instance().change_scene(new Scene_Loading(new Scene_Indoor()));
	Text::initialize();
	return true;
}

void Frame_Work::update(float elapsed_time/*Elapsed seconds from last frame*/)
{
	_monitor.update();


#ifdef USE_IMGUI
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif

	Graphics_Core::instance().post_procss.update(elapsed_time);
	Scene_Manager::instance().update(elapsed_time);

#ifdef USE_IMGUI



#endif
}
void Frame_Work::render(float elapsed_time/*Elapsed seconds from last frame*/)
{
	using namespace Color_Utils;

	Graphics_Core::instance().clear(Colors::gray(0.5f));
	Graphics_Core::instance().set_render_targets();
	Render_State::instance().set_sampler_state(Graphics_Core::instance().get_device_context());
	//Graphics_Core::instance().update_constants();
	//Graphics_Core::instance().get_geometry_buffer()->clear(Graphics_Core::instance().get_device_context());
	//Graphics_Core::instance().get_geometry_buffer()->activate(Graphics_Core::instance().get_device_context());
	//Graphics_Core::instance().get_geometry_buffer()->deactivate(Graphics_Core::instance().get_device_context());
	Scene_Manager::instance().render(elapsed_time);
	//Graphics_Core::instance().post_procss.end();
	//Graphics_Core::instance().post_procss.draw();
	//Graphics_Core::instance().post_procss.render();


#ifdef USE_IMGUI
	SHORT state = GetAsyncKeyState(VK_ADD);
	bool isDown = (state & 0x8000) != 0;

	// 押した瞬間だけを検出（エッジ検出）
	if (isDown && !g_prevHome)
	{
		_hide_imgui = !_hide_imgui;
	}

	g_prevHome = isDown;
	if (!_hide_imgui)
	{
		ImGui::Begin("ImGUI");

		if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_FittingPolicyScroll))
		{
			// Dashboard
			if (ImGui::BeginTabItem("Dashboard"))
			{
				_monitor.render_ui();
				ImGui::EndTabItem();
			}

			// Scene
			if (ImGui::BeginTabItem("Scene"))
			{
				Scene_Manager::instance().draw_gui();
				ImGui::EndTabItem();
			}

			// Rendering
			if (ImGui::BeginTabItem("Rendering"))
			{
				Graphics_Core::instance().post_procss.renderGUI();
				ImGui::EndTabItem();
			}

			// Assets（後から追加する予定）
			if (ImGui::BeginTabItem("Resource"))
			{
				// 今後追加するアセット管理UI
				//ImGui::Text("No asset tools yet.");
				Resource_Manager::instance().draw_imgui();
				ImGui::EndTabItem();
			}

			// Debug
			if (ImGui::BeginTabItem("Debug"))
			{
				// 今後追加するデバッグUI
				draw_log_contents();
				ImGui::EndTabItem();
			}

			// Settings
			if (ImGui::BeginTabItem("Settings"))
			{
				static ImGui_Util::ThemeType theme = ImGui_Util::ThemeType::seeing;
				ImGui_Util::draw_theme_selector(theme);
				ImGui::EndTabItem();
			}
			// Light
			if (ImGui::BeginTabItem("Light"))
			{

				Graphics_Core::instance().get_point_light_manager().draw_imgui();
				Graphics_Core::instance().get_spot_light_manager().draw_imgui();
				Graphics_Core::instance().get_area_light_manager().draw_imgui();

				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Camera"))
			{
				Camera_Manager::instance().draw_imgui();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::End();
	}




	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif

	UINT sync_interval{ 0 };
	Graphics_Core::instance().present(sync_interval);



}

bool Frame_Work::uninitialize()
{




	return true;
}

Frame_Work::~Frame_Work()
{

}

