
#include "resource_monitor.h"

Resource_Monitor::Resource_Monitor()
{
	PdhOpenQuery(
		nullptr,
		0,
		&cpu_query
	);

	PdhAddCounter(
		cpu_query,
		L"\\Processor(_Total)\\% Processor Time",
		0,
		&cpu_counter
	);

	// 初回サンプル
	PdhCollectQueryData(
		cpu_query
	);

	cpu_hist.resize(
		HISTORY,
		0
	);

	mem_hist.resize(
		HISTORY,
		0
	);

	last =
		std::chrono::steady_clock::now();
}

Resource_Monitor::~Resource_Monitor()
{
	if (cpu_query)
		PdhCloseQuery(cpu_query);

	if (log.is_open())
		log.close();
}

void Resource_Monitor::update()
{
	if (ImGui::GetCurrentContext())
	{
		fps =
			ImGui::GetIO()
			.Framerate;

		frame_ms =
			fps > 0
			?
			1000.f / fps
			:
			0;
	}

	auto now =
		std::chrono::steady_clock::now();

	auto elapsed =
		std::chrono::duration_cast<
		std::chrono::milliseconds
		>(
			now - last
		);

	if (elapsed.count() < 1000)
		return;

	update_cpu();
	update_memory();
	update_process();

	push(
		cpu_hist,
		cpu_pos,
		cpu
	);

	push(
		mem_hist,
		mem_pos,
		mem
	);

	peak_cpu =
		max(
			peak_cpu,
			cpu
		);

	peak_mem =
		max(
			peak_mem,
			mem
		);

	if (enable_log)
	{
		if (!log.is_open())
		{
			log.open(
				"resource_log.csv"
			);

			log
				<<
				"CPU,MEM,FPS\n";
		}

		log
			<<
			cpu
			<< ","
			<< mem
			<< ","
			<< fps
			<< "\n";
	}

	last =
		now;
}

void Resource_Monitor::update_cpu()
{
	if (!cpu_query)
		return;

	PDH_FMT_COUNTERVALUE val{};

	PdhCollectQueryData(
		cpu_query
	);

	if (
		PdhGetFormattedCounterValue(
			cpu_counter,
			PDH_FMT_DOUBLE,
			nullptr,
			&val
		)
		==
		ERROR_SUCCESS
		)
	{
		cpu =
			(float)
			val.doubleValue;
	}
}

void Resource_Monitor::update_memory()
{
	MEMORYSTATUSEX m{};

	m.dwLength =
		sizeof(m);

	if (
		GlobalMemoryStatusEx(
			&m
		)
		)
	{
		mem =
			(float)(
				(
					m.ullTotalPhys -
					m.ullAvailPhys
					)
				*
				100.0
				/
				m.ullTotalPhys
				);
	}
}

void Resource_Monitor::update_process()
{
	PROCESS_MEMORY_COUNTERS_EX pm{};

	if (
		GetProcessMemoryInfo(
			GetCurrentProcess(),
			(PROCESS_MEMORY_COUNTERS*)&pm,
			sizeof(pm)
		)
		)
	{
		process_mem_mb =
			pm.WorkingSetSize
			/
			1024.f
			/
			1024.f;
	}

	GetProcessHandleCount(
		GetCurrentProcess(),
		&handle_count
	);

	thread_count = 0;

	HANDLE snap =
		CreateToolhelp32Snapshot(
			TH32CS_SNAPTHREAD,
			0
		);

	if (
		snap
		!=
		INVALID_HANDLE_VALUE
		)
	{
		THREADENTRY32 te{};

		te.dwSize =
			sizeof(te);

		if (
			Thread32First(
				snap,
				&te
			)
			)
		{
			do
			{
				if (
					te.th32OwnerProcessID
					==
					GetCurrentProcessId()
					)
				{
					thread_count++;
				}

			} while (
				Thread32Next(
					snap,
					&te
				)
				);
		}

		CloseHandle(
			snap
		);
	}
}

void Resource_Monitor::push(
	std::vector<float>& v,
	int& idx,
	float value
)
{
	v[idx] =
		value;

	idx =
		(idx + 1)
		%
		HISTORY;
}

void Resource_Monitor::render_ui()
{
	ImGui::SeparatorText(
		"Resource Monitor"
	);

	ImGui::Text(
		"FPS %.1f (%.2fms)",
		fps,
		frame_ms
	);

	ImGui::Separator();

	ImGui::Text(
		"CPU %.1f%% Peak %.1f%%",
		cpu,
		peak_cpu
	);

	ImGui::ProgressBar(
		cpu / 100.f,
		{ -1,20 }
	);

	ImGui::PlotLines(
		"##CPU",
		cpu_hist.data(),
		HISTORY,
		cpu_pos,
		nullptr,
		0,
		100,
		{ 0,60 }
	);

	ImGui::Text(
		"Memory %.1f%% Peak %.1f%%",
		mem,
		peak_mem
	);

	ImGui::ProgressBar(
		mem / 100.f,
		{ -1,20 }
	);

	ImGui::PlotLines(
		"##MEM",
		mem_hist.data(),
		HISTORY,
		mem_pos,
		nullptr,
		0,
		100,
		{ 0,60 }
	);

	ImGui::Separator();

	ImGui::Text(
		"Process Memory %.1f MB",
		process_mem_mb
	);

	ImGui::Text(
		"Threads %u",
		thread_count
	);

	ImGui::Text(
		"Handles %u",
		handle_count
	);

	ImGui::Checkbox(
		"CSV Log",
		&enable_log
	);

	if (
		ImGui::Button(
			"Reset Peaks"
		)
		)
	{
		peak_cpu = 0;
		peak_mem = 0;
	}
}
