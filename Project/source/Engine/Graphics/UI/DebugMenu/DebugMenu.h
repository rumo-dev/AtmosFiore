#pragma once
#include <vector>
#include <string>
#include <mutex>
#include "Engine/Graphics/UI/ImGui/imgui.h"

struct SharedDebugData {
	std::mutex mtx;
	float fps = 0.0f;
	std::vector<float> graph_history;

	void Update(float val) {
		std::lock_guard<std::mutex> lock(mtx);
		graph_history.push_back(val / 100.0f);
		if (graph_history.size() > 50) graph_history.erase(graph_history.begin());
	}
};

class DebugMenu {
public:

	// マルチビューポート前提のため、ウィンドウの生成処理は不要
	// 描画ロジックのみを定義する
	static  void Render(SharedDebugData& data);

private:
	static void SetupNeverloseStyle();
	static void RenderDashboard(SharedDebugData& data);
};