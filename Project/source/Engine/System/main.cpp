#include <time.h>

#include "framework.h"
#include "graphics_core.h"


LRESULT CALLBACK window_procedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	Frame_Work* p =
		reinterpret_cast<Frame_Work*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	return p
		? p->handle_message(hwnd, msg, wparam, lparam)
		: DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(
	HINSTANCE instance,
	HINSTANCE prev_instance,
	LPSTR cmd_line,
	int cmd_show)
{
	srand(static_cast<unsigned int>(time(nullptr)));

#if defined(DEBUG) || defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	WNDCLASSEXW wcex{};
	wcex.cbSize = sizeof(WNDCLASSEXW);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = window_procedure;
	wcex.hInstance = instance;
	wcex.hIcon = LoadIcon(instance, MAKEINTRESOURCE(333));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszClassName = APPLICATION_NAME;

	if (!RegisterClassExW(&wcex))
	{
		return -1;
	}

	HWND hwnd = nullptr;

#if defined(DEBUG) || defined(_DEBUG)

	// デバッグ：ウィンドウモード
	RECT rc{ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };

	DWORD style =
		WS_OVERLAPPED |
		WS_CAPTION |
		WS_SYSMENU |
		WS_MINIMIZEBOX;

	AdjustWindowRect(&rc, style, FALSE);

	int window_width = rc.right - rc.left;
	int window_height = rc.bottom - rc.top;

	int screen_width = GetSystemMetrics(SM_CXSCREEN);
	int screen_height = GetSystemMetrics(SM_CYSCREEN);

	int x = (screen_width - window_width) / 2;
	int y = (screen_height - window_height) / 2;

	hwnd = CreateWindowExW(
		0,
		APPLICATION_NAME,
		L"",
		style,
		x,
		y,
		window_width,
		window_height,
		nullptr,
		nullptr,
		instance,
		nullptr);

#else

	// リリース：ボーダーレスフルスクリーン
	hwnd = CreateWindowExW(
		0,
		APPLICATION_NAME,
		L"",
		WS_POPUP,
		0,
		0,
		GetSystemMetrics(SM_CXSCREEN),
		GetSystemMetrics(SM_CYSCREEN),
		nullptr,
		nullptr,
		instance,
		nullptr);

#endif

	if (!hwnd)
	{
		return -1;
	}

	ShowWindow(hwnd, cmd_show);
	UpdateWindow(hwnd);

	Frame_Work framework(hwnd);

	SetWindowLongPtrW(
		hwnd,
		GWLP_USERDATA,
		reinterpret_cast<LONG_PTR>(&framework));

	return framework.run();
}