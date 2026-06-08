#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>
#include "Engine/Graphics/UI/ImGui/imgui.h"

// ゲーム側とデバッグツール間で共有するデータ
struct SharedMetricsData {
	std::mutex mtx;
	float fps = 0.0f;
	std::vector<float> graph_history;

	// カテゴリごとの設定フラグ
	std::unordered_map<std::string, bool> feature_toggles;
	// 数値設定
	std::unordered_map<std::string, float> feature_values;

	void Initialize() {
		// 初期値を設定
		feature_toggles["FeatherEnabled"] = true;
		feature_values["FeatherSlider"] = 50.0f;

		for (int i = 1; i <= 11; ++i) {
			std::string key = "CrownToggle_" + std::to_string(i);
			feature_toggles[key] = false;
		}
	}

	void UpdateMetrics(float new_fps) {
		std::lock_guard<std::mutex> lock(mtx);
		fps = new_fps;
		graph_history.push_back(new_fps / 100.0f); // グラフ用に正規化
		if (graph_history.size() > 50) graph_history.erase(graph_history.begin());
	}
};

class Dashboard {
public:
	static Dashboard& Instance() {
		static Dashboard instance;
		return instance;
	}
	Dashboard() = default;

	// メインの描画ルーチン
	void Render(SharedMetricsData& data);

private:
	void SetupBaseStyle(); // 全体の色味と角丸
	void RenderSideBar(); // 左側のアイコンメニュー
	void RenderMainContent(SharedMetricsData& data); // 右側の設定項目エリア

	// カスタムパーツ
	void CustomHeader(const char* label); // カテゴリ見出し
	bool CustomToggle(const char* label, bool* v); // モダンなトグル
	void CustomSlider(const char* label, float* v, float v_min, float v_max); // モダンなスライダー
};