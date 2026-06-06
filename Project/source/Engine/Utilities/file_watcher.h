#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include <filesystem>
#include <chrono>

class File_Watcher {
public:
	using Callback = std::function<void(const std::wstring&)>;

	File_Watcher(const std::wstring& path, Callback callback)
		: m_path(path), m_callback(callback)
	{
		if (std::filesystem::exists(path))
			m_lastWriteTime = std::filesystem::last_write_time(path);
	}

	void update()
	{
		if (!std::filesystem::exists(m_path)) return;

		auto currentWriteTime = std::filesystem::last_write_time(m_path);
		if (currentWriteTime != m_lastWriteTime) {
			m_lastWriteTime = currentWriteTime;
			m_callback(m_path);   // ← コールバック（ここで reload する）
		}
	}

private:
	std::wstring m_path;
	std::filesystem::file_time_type m_lastWriteTime;
	Callback m_callback;
};
