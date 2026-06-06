#pragma once
#include <d3d11.h>

#include <windows.h>
#include <tchar.h>
#include <sstream>
#include <stdio.h>

#include "graphics_core.h"
#include "Engine/utilities/misc.h"
#include "Engine/utilities/high_resolution_timer.h"
#include "Engine/utilities/color_util.h"
#include "Engine/utilities/imgui_util.h"
#include "Engine/utilities/resource_monitor.h"
#include "Engine/Graphics/UI/text/text.h"

#include "Game/Scenes/scene.h"

#ifdef USE_IMGUI
#include "Engine/Graphics/UI/ImGui/imgui.h"
#include "Engine/Graphics/UI/ImGui/imgui_internal.h"
#include "Engine/Graphics/UI/ImGui/imgui_impl_dx11.h"
#include "Engine/Graphics/UI/ImGui/imgui_impl_win32.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern ImWchar glyphRangesJapanese[];
#endif

CONST LPCWSTR APPLICATION_NAME{ L"AtmosFiore" };

/**
 * @brief アプリケーションフレームワーククラス
 *
 * @details
 * Win32 + Direct3D11 ベースのメインループ制御を行うクラス。
 * 以下の責務を持つ：
 *
 * - 初期化 / 終了処理
 * - メインループ管理（更新・描画）
 * - メッセージ処理（WndProc）
 * - フレーム時間・FPS計測
 * - ImGui の初期化と終了処理
 *
 * @note
 * - ゲームループは PeekMessage を用いた非ブロッキング方式
 * - update() / render() はフレームごとに呼ばれる
 * - 時間管理は high_resolution_timer を使用
 *
 * - ImGui は USE_IMGUI 定義時のみ有効
 * - Scene 管理は Scene_Manager に委譲される
 *
 * @warning
 * - initialize() が失敗すると run() は即終了します
 *
 * - Graphics_Core の初期化は initialize() 内で行う前提です
 *
 * - handle_message() は Win32 の WndProc から必ず呼ばれる必要があります
 *
 * - ImGui 使用時：
 *   WndProc で ImGui_ImplWin32_WndProcHandler を先に呼ぶ必要があります
 *
 * @code
 * // WinMain などで
 * Frame_Work app(hwnd);
 *
 * return app.run();
 * @endcode
 */
class Frame_Work
{
public:

	/// ウィンドウハンドル
	CONST HWND hwnd;

	/**
	 * @brief コンストラクタ
	 * @param hwnd ウィンドウハンドル
	 */
	Frame_Work(HWND hwnd);

	/**
	 * @brief デストラクタ
	 */
	~Frame_Work();

	Frame_Work(const Frame_Work&) = delete;
	Frame_Work& operator=(const Frame_Work&) = delete;
	Frame_Work(Frame_Work&&) noexcept = delete;
	Frame_Work& operator=(Frame_Work&&) noexcept = delete;

	/**
	 * @brief メインループ開始
	 * @return 終了コード
	 *
	 * @details
	 * 以下の流れで実行される：
	 * 1. initialize()
	 * 2. メインループ
	 *    - メッセージ処理
	 *    - update()
	 *    - render()
	 * 3. uninitialize()
	 */
	int run()
	{
		MSG msg{};

		if (!initialize())
		{
			return 0;
		}

#ifdef USE_IMGUI
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		const char* font_path = "./data/fonts/imgui.ttf";

		ImGuiIO& io = ImGui::GetIO();

		const ImWchar* ranges = io.Fonts->GetGlyphRangesJapanese();

		io.Fonts->AddFontFromFileTTF(font_path, 16.0f, nullptr, ranges);
		unsigned char* pixels;
		int width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX11_Init(
			Graphics_Core::instance().get_device(),
			Graphics_Core::instance().get_device_context()
		);

		ImGui::StyleColorsDark();
#endif

		while (WM_QUIT != msg.message)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				_tictoc.tick();
				calculate_frame_stats();

				update(_tictoc.time_interval());
				render(_tictoc.time_interval());

				if (Scene_Manager::instance().is_exit()) break;
			}
		}

#ifdef USE_IMGUI
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
#endif

		return uninitialize() ? static_cast<int>(msg.wParam) : 0;
	}

	/**
	 * @brief メッセージ処理（WndProc）
	 *
	 * @param hwnd ウィンドウハンドル
	 * @param msg メッセージ
	 * @param wparam WPARAM
	 * @param lparam LPARAM
	 * @return LRESULT
	 *
	 * @note
	 * - ImGui 使用時は最初に ImGui 側へメッセージを渡す
	 * - ESCキーでアプリ終了
	 */
	LRESULT CALLBACK handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
#ifdef USE_IMGUI
		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
			return true;
		}
#endif
		switch (msg)
		{
		case WM_PAINT:
		{
			PAINTSTRUCT ps{};
			BeginPaint(hwnd, &ps);
			EndPaint(hwnd, &ps);
		}
		break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_KEYDOWN:
			if (wparam == VK_ESCAPE)
			{
				PostMessage(hwnd, WM_CLOSE, 0, 0);
			}
			break;

		case WM_ENTERSIZEMOVE:
			_tictoc.stop();   ///< リサイズ中はタイマー停止
			break;

		case WM_EXITSIZEMOVE:
			_tictoc.start();  ///< リサイズ終了で再開
			break;

		default:
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		return 0;
	}

private:

	/// 初期化
	bool initialize();

	/**
	 * @brief 更新処理
	 * @param elapsed_time 前フレームからの経過時間（秒）
	 */
	void update(float elapsed_time);

	/**
	 * @brief 描画処理
	 * @param elapsed_time 前フレームからの経過時間（秒）
	 */
	void render(float elapsed_time);

	/// 終了処理
	bool uninitialize();

private:

	high_resolution_timer _tictoc; ///< 高精度タイマー

	uint32_t _frames{ 0 };         ///< フレームカウント
	float _elapsed_time{ 0.0f };   ///< 経過時間

	bool _hide_imgui{ false };     ///< ImGui表示フラグ
	bool g_prevHome{ false };      ///< ホットキー状態保持

	/**
	 * @brief FPS計算とウィンドウタイトル更新
	 *
	 * @details
	 * 1秒ごとにFPSとフレーム時間(ms)を計算してタイトルバーに表示する
	 */
	void calculate_frame_stats()
	{
		if (++_frames, (_tictoc.time_stamp() - _elapsed_time) >= 1.0f)
		{
			float fps = static_cast<float>(_frames);

			std::wostringstream outs;
			outs.precision(6);
#ifdef _DEBUG
			outs << APPLICATION_NAME
				<< L" : FPS : " << fps
				<< L" / Frame Time : "
				<< 1000.0f / fps << L" (ms)";
#else
			outs << APPLICATION_NAME;

#endif // _DEBUG


			SetWindowTextW(hwnd, outs.str().c_str());

			_frames = 0;
			_elapsed_time += 1.0f;
		}
	}

	/// リソース監視（メモリ・GPUなど）
	Resource_Monitor _monitor;
};