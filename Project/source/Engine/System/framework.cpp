#include "framework.h"
#include"graphics_core.h"
#include "Engine/system/manager/resource_manager.h"
#include "Engine/Utilities/misc.h"
#include "Engine/Graphics/UI/DebugMenu/Dashboard.h"
#include "Engine/Extentions/Tracy/tracy/Tracy.hpp"
Frame_Work::Frame_Work(HWND hwnd) : hwnd(hwnd)
{}

bool Frame_Work::initialize()

{
	Graphics_Core::instance().initialize(hwnd);
	Graphics_Core::instance().set_borderless_window();

	//static ImGui_Util::ThemeType theme = ImGui_Util::ThemeType::seeing;
	//ImGui_Util::set_theme(theme);

	Scene_Manager::instance().change_scene(new Scene_Splash());
	Text::initialize();
	Dashboard::Instance().InitializeUI();
	AudioSystem::instance().Initialize(
		"data/Audio/audio_data.json",   // JSON ファイルパス
		"data/Audio/",            // 音声ファイルのベースディレクトリ
		64                          // 最大同時再生ボイス数
	);
	AudioSystem::instance().SetListenerPosition(0.0f, 0.0f, 0.0f);
	AudioSystem::instance().SetListenerOrientation(0.0f, 0.0f, 1.0f,   // front
		0.0f, 1.0f, 0.0f);  // up
	return true;
}

void Frame_Work::update(float elapsed_time/*Elapsed seconds from last frame*/)
{
	ZoneScoped;

#ifdef USE_IMGUI
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif

	Graphics_Core::instance().post_procss.update(elapsed_time);
	Scene_Manager::instance().update(elapsed_time);
	AudioSystem::instance().Update();
	//AudioSystem::instance().UpdateInput();

#ifdef USE_IMGUI



#endif
}
void Frame_Work::render(float elapsed_time) {
	ZoneScoped;
	using namespace Color_Utils;

	// --- ゲームのメイン描画 ---
	Graphics_Core::instance().clear(Colors::gray(0.5f));
	Graphics_Core::instance().set_render_targets();
	Render_State::instance().set_sampler_state(Graphics_Core::instance().get_device_context());
	Scene_Manager::instance().render(elapsed_time);


	// --- キー入力処理 (毎フレームチェック) ---
	// 0x60 は '~' キー (半角/全角キー) の仮想キーコードです
	static bool prev_key_state = false;
	bool current_key_state = (GetAsyncKeyState(VK_OEM_3) & 0x8000) != 0;

	// キーが押された瞬間だけ切り替える (チャタリング防止)
	if (current_key_state && !prev_key_state) {
		_hide_imgui = !_hide_imgui;
	}
	prev_key_state = current_key_state;

	// --- ImGuiの描画 ---
#ifdef USE_IMGUI
	if (!_hide_imgui) {
		Dashboard::Instance().data.UpdateFPS(1.0f / elapsed_time);
		Dashboard::Instance().Render();
	}

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		// 別ウィンドウの描画
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		// --- 修正の鍵 ---
		// 別ウィンドウの描画完了後に、デバイスコンテキストをメインに戻す
		// これをしないと、次のフレームの描画が別ウィンドウ側の設定に引きずられます
		Graphics_Core::instance().set_render_targets();
	}
#endif

	UINT sync_interval{ 0 }; // ちらつき防止のため 1 (垂直同期) を推奨
	FrameMark;
	Graphics_Core::instance().present(sync_interval);
}

bool Frame_Work::uninitialize()
{
	AudioSystem::instance().Shutdown();



	return true;
}

Frame_Work::~Frame_Work()
{

}

