#pragma once

#include <windows.h>
#include <crtdbg.h>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <filesystem>

#if defined(DEBUG) || defined(_DEBUG)
/**
 * @brief デバッグ時のアサートマクロ
 *
 * 条件式 @p expr が false の場合にデバッグレポートを出力し、ブレークします。
 *
 * @param expr チェック対象の式
 * @param msg エラーメッセージ
 */
#define _ASSERT_EXPR_A(expr, msg) \
		(void)((!!(expr)) || \
		(1 != _CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, NULL, "%s", msg)) || \
		(_CrtDbgBreak(), 0))
#else
#define _ASSERT_EXPR_A(expr, expr_str) ((void)0)
#endif

/**
 * @brief HRESULT からシステムエラーメッセージを取得します
 *
 * @param hr HRESULT 値
 * @return LPWSTR エラーメッセージ（呼び出し側で LocalFree により解放が必要）
 */
inline LPWSTR hr_trace(HRESULT hr)
{
	LPWSTR msg{ 0 };
	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPWSTR>(&msg), 0, NULL);
	return msg;
}

/**
 * @brief 高精度ベンチマーク計測クラス
 *
 * Windows の QueryPerformanceCounter を利用して処理時間を計測します。
 */
class benchmark
{
	LARGE_INTEGER ticks_per_second; ///< 周波数
	LARGE_INTEGER start_ticks;      ///< 計測開始時刻
	LARGE_INTEGER current_ticks;    ///< 計測終了時刻

public:
	/**
	 * @brief コンストラクタ
	 *
	 * パフォーマンスカウンタの周波数を取得します。
	 */
	benchmark()
	{
		QueryPerformanceFrequency(&ticks_per_second);
		QueryPerformanceCounter(&start_ticks);
		QueryPerformanceCounter(&current_ticks);
	}

	~benchmark() = default;
	benchmark(const benchmark&) = delete;
	benchmark& operator=(const benchmark&) = delete;
	benchmark(benchmark&&) noexcept = delete;
	benchmark& operator=(benchmark&&) noexcept = delete;

	/**
	 * @brief 計測を開始します
	 */
	void begin()
	{
		QueryPerformanceCounter(&start_ticks);
	}

	/**
	 * @brief 計測を終了し、経過時間を返します
	 *
	 * @return float 経過時間（秒）
	 */
	float end()
	{
		QueryPerformanceCounter(&current_ticks);
		return static_cast<float>(current_ticks.QuadPart - start_ticks.QuadPart) /
			static_cast<float>(ticks_per_second.QuadPart);
	}
};

#include <string>
#include <vector>
#include <mutex>
#include <cstdarg>
#include <Windows.h>

// ImGuiのヘッダー
#include "Engine/Graphics/UI/ImGui/imgui.h"

inline std::mutex log_mutex;     ///< ログ出力排他用ミューテックス

/**
 * @brief ログレベルを表す列挙型
 */
enum class LogLevel { Info, Success, Warning, Error };

/**
 * @brief ImGui管理用の1行分のログデータ
 */
struct LogEntry {
	std::string timestamp;
	LogLevel level;
	std::string message; // UTF-8形式の文字列
};

inline std::vector<LogEntry> log_history;
inline bool scroll_to_bottom = false; // 新着ログの自動スクロールフラグ

/**
 * @brief 現在日時を文字列で返します
 * * @return std::string "YYYY-MM-DD hh:mm:ss.mmm" 形式の文字列
 */
inline std::string get_current_datetime() {
	SYSTEMTIME st{};
	GetLocalTime(&st);
	char buffer[80];
	sprintf_s(buffer, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	return std::string(buffer);
}

/**
 * @brief ログレベルに応じたImGui用のカラー（RGBA）を返します
 */
inline ImVec4 get_imgui_color(LogLevel level)
{
	switch (level)
	{
	case LogLevel::Info:    return ImVec4(0.3f, 0.8f, 1.0f, 1.0f); // 水色
	case LogLevel::Success: return ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // 緑
	case LogLevel::Warning: return ImVec4(1.0f, 0.8f, 0.2f, 1.0f); // 黄
	case LogLevel::Error:   return ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // 赤
	default:                return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // 白
	}
}

/**
 * @brief ログレベルの文字列を返します
 */
inline const char* get_level_string(LogLevel level)
{
	switch (level)
	{
	case LogLevel::Info:    return "INFO";
	case LogLevel::Success: return "SUCCESS";
	case LogLevel::Warning: return "WARNING";
	case LogLevel::Error:   return "ERROR";
	default:                return "UNKNOWN";
	}
}

/**
 * @brief Shift-JIS (CP932) の文字列を UTF-8 に変換します
 */
inline std::string sjis_to_utf8(const std::string& sjis_str)
{
	if (sjis_str.empty()) return "";

	// 1. Shift-JIS から UTF-16 へ変換
	int size_needed = MultiByteToWideChar(CP_ACP, 0, sjis_str.c_str(), -1, NULL, 0);
	std::wstring wstr(size_needed, 0);
	MultiByteToWideChar(CP_ACP, 0, sjis_str.c_str(), -1, &wstr[0], size_needed);
	if (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();

	// 2. UTF-16 から UTF-8 へ変換
	size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
	std::string utf8_str(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8_str[0], size_needed, NULL, NULL);
	if (!utf8_str.empty() && utf8_str.back() == '\0') utf8_str.pop_back();

	return utf8_str;
}

/**
 * @brief ログをImGui用バッファに出力します（文字化け対策UTF-8変換付き）
 * * @param format printf 形式のフォーマット文字列
 * @param level ログレベル
 * @param ... 可変引数
 */
inline void log_printf(const char* format, LogLevel level, ...) {
	std::lock_guard<std::mutex> lock(log_mutex);

	// 1. フォーマット文字列のパース
	va_list args;
	va_start(args, level);
	char buf[1024];
	wvsprintfA(buf, format, args);
	va_end(args);

	// 2. ソース・実行文字セットは UTF-8 (/utf-8) のためそのまま記録
	log_history.push_back({ get_current_datetime(), level, std::string(buf) });

	// 古いログの自動削除（件数はお好みで調整してください）
	if (log_history.size() > 2000)
	{
		log_history.erase(log_history.begin());
	}

	scroll_to_bottom = true;
}

/**
 * @brief ImGuiのタブや子ウィンドウ内にログの中身を描画する関数
 */
inline void draw_log_contents()
{
	// ログクリアボタン
	if (ImGui::Button("Clear"))
	{
		std::lock_guard<std::mutex> lock(log_mutex);
		log_history.clear();
	}

	ImGui::Separator();

	// ログ表示エリア（タブの残りの全領域を埋める）
	ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	std::unique_lock<std::mutex> lock(log_mutex);

	// 画面外の描画をスキップして軽量化
	ImGuiListClipper clipper;
	clipper.Begin(static_cast<int>(log_history.size()));

	while (clipper.Step())
	{
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
		{
			const auto& entry = log_history[i];
			ImVec4 color = get_imgui_color(entry.level);

			// [日時] [レベル] メッセージ
			ImGui::TextDisabled("[%s]", entry.timestamp.c_str());
			ImGui::SameLine();
			ImGui::TextColored(color, "[%s]", get_level_string(entry.level));
			ImGui::SameLine();
			ImGui::TextUnformatted(entry.message.c_str());
		}
	}
	clipper.End();

	// 新着ログの自動スクロール
	if (scroll_to_bottom)
	{
		ImGui::SetScrollHereY(1.0f);
		scroll_to_bottom = false;
	}

	lock.unlock();
	ImGui::EndChild();
}
#include <d3dcompiler.h>
inline void OutputCompileErrors(ID3DBlob* err) {
	if (!err) return;
	const char* msg = static_cast<const char*>(err->GetBufferPointer());
	OutputDebugStringA(msg);
	log_printf("Shader Compile Error:\n%s", LogLevel::Error, msg);
}




inline std::wstring s2ws(const std::string& str)
{
	if (str.empty()) return std::wstring();

	int size_needed = MultiByteToWideChar(
		CP_UTF8,                // UTF-8
		0,
		str.c_str(),
		(int)str.size(),
		nullptr,
		0
	);

	std::wstring result(size_needed, 0);

	MultiByteToWideChar(
		CP_UTF8,
		0,
		str.c_str(),
		(int)str.size(),
		&result[0],
		size_needed
	);

	return result;
}

inline std::string ws2s(const std::wstring& wstr)
{
	if (wstr.empty()) return std::string();

	int size_needed = WideCharToMultiByte(
		CP_UTF8,
		0,
		wstr.c_str(),
		(int)wstr.size(),
		nullptr,
		0,
		nullptr,
		nullptr
	);

	std::string result(size_needed, 0);

	WideCharToMultiByte(
		CP_UTF8,
		0,
		wstr.c_str(),
		(int)wstr.size(),
		&result[0],
		size_needed,
		nullptr,
		nullptr
	);

	return result;
}
