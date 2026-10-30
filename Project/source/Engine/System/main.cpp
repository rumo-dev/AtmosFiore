#include <time.h>
#include <string>

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
	// Assets are loaded with paths relative to the working directory.  Explorer,
	// shortcuts and launchers do not necessarily start us in the exe directory.
	std::wstring executable_path(MAX_PATH, L'\0');
	for (;;)
	{
		const DWORD length = GetModuleFileNameW(nullptr, executable_path.data(),
			static_cast<DWORD>(executable_path.size()));
		if (length == 0)
		{
			MessageBoxW(nullptr, L"実行ファイルの場所を取得できません。", APPLICATION_NAME, MB_OK | MB_ICONERROR);
			return -1;
		}
		if (length < executable_path.size())
		{
			executable_path.resize(length);
			break;
		}
		executable_path.resize(executable_path.size() * 2);
	}
	const size_t separator = executable_path.find_last_of(L"\\/");
	if (separator == std::wstring::npos)
	{
		MessageBoxW(nullptr, L"実行ファイルのフォルダーを取得できません。", APPLICATION_NAME, MB_OK | MB_ICONERROR);
		return -1;
	}

	// Look for the data folder next to the exe first (distributed build), then in
	// parent folders (Visual Studio puts the exe in x64\Debug etc. while data
	// lives in the project folder).  Whichever folder holds data becomes the
	// working directory.
	auto has_data = [](const std::wstring& dir)
	{
		const std::wstring prefix = dir.empty() ? L"" : dir + L"\\";
		return GetFileAttributesW((prefix + L"data\\shaders\\cso").c_str()) != INVALID_FILE_ATTRIBUTES &&
			GetFileAttributesW((prefix + L"data\\AtmosFioreLogo.png").c_str()) != INVALID_FILE_ATTRIBUTES;
	};

	std::wstring base_dir = executable_path.substr(0, separator);
	bool found = false;
	for (int depth = 0; depth < 6 && !base_dir.empty(); ++depth)
	{
		if (has_data(base_dir))
		{
			found = true;
			break;
		}
		const size_t parent = base_dir.find_last_of(L"\\/");
		if (parent == std::wstring::npos)
		{
			break;
		}
		base_dir.resize(parent);
	}

	// Fall back to the current working directory (e.g. VS "Working Directory").
	if (!found && has_data(L""))
	{
		found = true;
	}
	else if (found && !SetCurrentDirectoryW(base_dir.c_str()))
	{
		MessageBoxW(nullptr, L"data フォルダーのある場所に移動できません。", APPLICATION_NAME, MB_OK | MB_ICONERROR);
		return -1;
	}

	if (!found)
	{
		MessageBoxW(nullptr, L"data フォルダーが見つかりません。ZIP をすべて展開し、exe と data を同じフォルダーに置いてください。",
			APPLICATION_NAME, MB_OK | MB_ICONERROR);
		return -1;
	}

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
	wcex.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
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

	Frame_Work framework(hwnd, cmd_show);

	SetWindowLongPtrW(
		hwnd,
		GWLP_USERDATA,
		reinterpret_cast<LONG_PTR>(&framework));

	return framework.run();
}
