#pragma once
#include <string>
#include "Engine/utilities/file_watcher.h"
class Shader_Base {
public:
	virtual ~Shader_Base() = default;

	virtual bool load(
		const std::wstring& hlslPath,
		const std::wstring& csoPath,
		const std::string& entry,
		const std::string& target) = 0;

	bool load_and_store(
		const std::wstring& hlslPath,
		const std::wstring& csoPath,
		const std::string& entry,
		const std::string& target)
	{
		m_hlslPath = hlslPath;
		m_csoPath = csoPath;
		m_entry = entry;
		m_target = target;

		return load(hlslPath, csoPath, entry, target);
	}

	bool reload() {
		log_printf("=== シェーダーリロード開始 === %ls\n", LogLevel::Info, m_hlslPath.c_str());
		bool success = load(m_hlslPath, m_csoPath, m_entry, m_target);
		if (success)
			log_printf("=== シェーダーリロード成功 === %ls\n", LogLevel::Success, m_hlslPath.c_str());
		else
			log_printf("=== シェーダーリロード失敗 === %ls\n", LogLevel::Error, m_hlslPath.c_str());
		return success;
	}

protected:
	std::wstring m_hlslPath;
	std::wstring m_csoPath;
	std::string  m_entry;
	std::string  m_target;
};
