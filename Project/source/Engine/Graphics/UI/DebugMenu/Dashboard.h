#pragma once
#include <vector>
#include <string>
#include <functional>
#include <mutex>
#include <unordered_map>

#include "Engine/Graphics/UI/ImGui/imgui.h"
#include "Engine/utilities/resource_monitor.h"

struct SharedMetricsData {
	mutable std::mutex mtx;

	// --- 設定項目 (Settings) ---
	std::unordered_map<std::string, bool> feature_toggles;
	std::unordered_map<std::string, float> feature_values;

	// --- パフォーマンス計測 (Metrics) ---
	float fps = 0.0f;
	float frame_time = 0.0f;
	std::vector<float> graph_history;

	void SetToggle(const std::string& key, bool value) {
		std::lock_guard<std::mutex> lock(mtx);
		feature_toggles[key] = value;
	}

	bool GetToggle(const std::string& key) const {
		std::lock_guard<std::mutex> lock(mtx);
		return feature_toggles.count(key) ? feature_toggles.at(key) : false;
	}

	void UpdateFPS(float current_fps) {
		std::lock_guard<std::mutex> lock(mtx);
		fps = current_fps;
		graph_history.push_back(current_fps);
		if (graph_history.size() > 100) {
			graph_history.erase(graph_history.begin());
		}
	}
};

using RenderFunc = std::function<void(SharedMetricsData&)>;

struct SubTab {
	std::string name;
	std::string tooltip; // 【改善】ツールチップを追加
	RenderFunc render_func;
};

struct TabModule {
	std::string name;
	std::vector<SubTab> subtabs;
};

class Dashboard {
public:
	static Dashboard& Instance() { static Dashboard instance; return instance; }

	void InitializeUI();
	void RegisterModule(const TabModule& mod) { modules.push_back(mod); }
	void Render();
	void SetupStyle();

	SharedMetricsData data;
	ImFont* font_large = nullptr;

private:
	Dashboard() = default;
	std::vector<TabModule> modules;
	int active_mod_idx = 0;
	int active_sub_idx = 0;

	void RenderSideBar();
	void RenderGroupCard(const char* label, ImVec2 size, std::function<void()> content);
	void RenderMainContent();

	bool some_bool = false;
	float some_float = 0.0f;
	Resource_Monitor _monitor;
};