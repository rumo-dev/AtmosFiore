
#pragma once

#include <Windows.h>
#include <Pdh.h>
#include <Psapi.h>
#include <TlHelp32.h>   
#include <vector>
#include <chrono>
#include <fstream>
#include <numeric>


#include "Engine/Graphics/UI/ImGui/imgui.h"

#pragma comment(lib,"Pdh.lib")
#pragma comment(lib,"Psapi.lib")

class Resource_Monitor
{
public:

	Resource_Monitor();
	~Resource_Monitor();

	void update();
	void render_ui();

private:

	void update_cpu();
	void update_memory();
	void update_process();

	void push(
		std::vector<float>& vec,
		int& idx,
		float value
	);

	float avg(
		const std::vector<float>& vec
	) const;

	void save_log();

private:

	static constexpr int HISTORY = 60;

	PDH_HQUERY cpu_query{};
	PDH_HCOUNTER cpu_counter{};

	float cpu = 0;
	float mem = 0;

	float peak_cpu = 0;
	float peak_mem = 0;

	float fps = 0;
	float frame_ms = 0;

	float process_mem_mb = 0;

	DWORD thread_count = 0;
	DWORD handle_count = 0;

	bool enable_log = false;

	std::ofstream log;

	std::vector<float> cpu_hist;
	std::vector<float> mem_hist;

	int cpu_pos = 0;
	int mem_pos = 0;

	std::chrono::steady_clock::time_point last;
};
