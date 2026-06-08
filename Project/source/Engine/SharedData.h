#pragma once
#include <mutex>
#include <vector>

// ゲームとデバッグツール間で共有するデータ構造
struct SharedDebugData {
	std::mutex mtx;
	float fps = 0.0f;
	float cpu_usage = 0.0f;
	std::vector<float> graph_history;

	void Update(float val) {
		std::lock_guard<std::mutex> lock(mtx);
		graph_history.push_back(val);
		if (graph_history.size() > 100) graph_history.erase(graph_history.begin());
	}
};