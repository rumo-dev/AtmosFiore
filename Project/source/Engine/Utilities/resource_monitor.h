#pragma once

#include <windows.h>
#include <pdh.h>
#include <vector>
#include <chrono>
#include "Engine/Graphics/UI/ImGui/imgui.h"

class Resource_Monitor {
public:
	Resource_Monitor();
	~Resource_Monitor();

	/// @brief 1秒に1回CPU/メモリ更新
	void update();

	/// @brief ImGuiでCPU/メモリ使用率を描画
	void render_ui();

private:
	void update_cpu_usage();
	void update_memory_usage();

private:
	float _cpu_usage;
	float _memory_usage;

	std::vector<float> _cpu_history;
	std::vector<float> _memory_history;
	const size_t _history_size = 100;

	PDH_HQUERY _cpu_query;
	PDH_HCOUNTER _cpu_total;

	std::chrono::steady_clock::time_point _last_update_time;
};
