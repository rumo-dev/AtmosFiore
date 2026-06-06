#include "resource_monitor.h"
#include <algorithm> // std::copy
#pragma comment(lib, "Pdh.lib")


Resource_Monitor::Resource_Monitor()
	: _cpu_usage(0.0f), _memory_usage(0.0f)
{
	PdhOpenQuery(NULL, 0, &_cpu_query);
	PdhAddCounter(_cpu_query, L"\\Processor(_Total)\\% Processor Time", 0, &_cpu_total);
	PdhCollectQueryData(_cpu_query);

	_cpu_history.resize(_history_size, 0.0f);
	_memory_history.resize(_history_size, 0.0f);

	_last_update_time = std::chrono::steady_clock::now();
}

Resource_Monitor::~Resource_Monitor()
{
	PdhCloseQuery(_cpu_query);
}

void Resource_Monitor::update()
{
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - _last_update_time);

	if (elapsed.count() >= 1) { // 1秒ごとに更新
		update_cpu_usage();
		update_memory_usage();

		if (_cpu_history.size() >= _history_size) _cpu_history.erase(_cpu_history.begin());
		_cpu_history.push_back(_cpu_usage);

		if (_memory_history.size() >= _history_size) _memory_history.erase(_memory_history.begin());
		_memory_history.push_back(_memory_usage);

		_last_update_time = now;
	}
}

void Resource_Monitor::update_cpu_usage()
{
	PDH_FMT_COUNTERVALUE counter_val;
	PdhCollectQueryData(_cpu_query);
	PdhGetFormattedCounterValue(_cpu_total, PDH_FMT_DOUBLE, NULL, &counter_val);

	_cpu_usage = static_cast<float>(counter_val.doubleValue);
}

void Resource_Monitor::update_memory_usage()
{
	MEMORYSTATUSEX mem_info = {};
	mem_info.dwLength = sizeof(MEMORYSTATUSEX);
	if (GlobalMemoryStatusEx(&mem_info)) {
		DWORDLONG total = mem_info.ullTotalPhys;
		DWORDLONG used = total - mem_info.ullAvailPhys;
		_memory_usage = static_cast<float>(used * 100.0 / total);
	}
}

void Resource_Monitor::render_ui()
{

	ImGui::Text("CPU Usage: %.2f %%", _cpu_usage);
	ImGui::PlotLines("##CPU", _cpu_history.data(), static_cast<int>(_cpu_history.size()), 0, nullptr, 0.0f, 100.0f, ImVec2(0, 60));

	ImGui::Text("Memory Usage: %.2f %%", _memory_usage);
	ImGui::PlotLines("##Memory", _memory_history.data(), static_cast<int>(_memory_history.size()), 0, nullptr, 0.0f, 100.0f, ImVec2(0, 60));




}
