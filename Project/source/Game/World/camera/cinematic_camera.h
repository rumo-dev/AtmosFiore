#pragma once
#include "camera_base.h"
#include <Windows.h>
#include <DirectXMath.h>
#include <vector>
#include <fstream>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include "Engine/Graphics/UI/ImGui/ImGui.h"
#include "Engine/Graphics/UI/DebugMenu/CustomWidgets.h"   // ★パスは実際のプロジェクト構成に合わせて調整してください
#include "camera_manager.h"
#include "Engine/Utilities/JsonDataManager.h"

namespace dx = DirectX;
namespace fs = std::filesystem;
/**
 * @brief カメラの経由地データ構造体
 */
struct Camera_Way_Point {
	dx::XMVECTOR position;
	dx::XMVECTOR target;
};

/**
 * @brief シネマティック（スプライン曲線移動）カメラクラス
 */
class Cinematic_Camera : public ICamera {
private:
	static constexpr int kTimelineFPS = 30; // タイムライン表示・スクラブ用のフレームレート

	std::vector<Camera_Way_Point> _user_way_points;
	std::vector<Camera_Way_Point> _spline_nodes;

	bool _is_playing = false;
	bool _is_loop = false;

	float _current_time = 0.0f;
	float _time_per_segment = 2.0f;

	// --- Playback 設定（UIで編集 / JSONへ保存される値） ---
	float _default_duration_per_node = 2.0f;
	bool  _default_loop_playback = false;

	// タイムライン描画用の作業バッファ（毎フレーム再構築するため永続化はしない）
	std::vector<int> _timeline_keyframes;

	JsonDataManager _json_manager;
	std::string _root_directory = "data/cinematic_paths/";
	char _save_name_buffer[64] = "new_path";
	std::string _selected_file_path = "";
	std::vector<std::string> _detected_files;

	std::unordered_map<size_t, bool> _wp_expanded;
	bool _wp_section_open = true;

public:
	Cinematic_Camera() : _json_manager("", false) {
		camera_.position = dx::XMVectorSet(0.0f, 5.0f, -10.0f, 1.0f);
		camera_.target = dx::XMVectorZero();
		camera_.up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		if (!fs::exists(_root_directory)) {
			fs::create_directory(_root_directory);
		}
		refresh_file_list();
	}

	void refresh_file_list() {
		_detected_files.clear();
		if (!fs::exists(_root_directory)) return;
		for (const auto& entry : fs::directory_iterator(_root_directory)) {
			if (entry.is_regular_file() && entry.path().extension() == ".json") {
				_detected_files.push_back(entry.path().filename().string());
			}
		}
	}

	//=========================================================================
	//  スプライン構築 / 評価
	//=========================================================================

	/**
	 * @brief 現在のウェイポイント配列から Catmull-Rom 用の制御点配列を構築する
	 * @param loop true ならループ用にダミー制御点を周回させる
	 */
	void rebuild_spline_nodes(bool loop) {
		_spline_nodes.clear();
		size_t n = _user_way_points.size();
		if (n < 2) return;

		if (loop) {
			// ★【ループモードのノード構築】
			// 継ぎ目を滑らかにするため、前後に円環状に隣り合うノードをダミーとして仕込む
			_spline_nodes.reserve(n + 3);

			// 1. 先頭ダミー：最後のノード (n-1)
			_spline_nodes.push_back(_user_way_points[n - 1]);

			// 2. 本番ノードをすべてコピー
			for (const auto& wp : _user_way_points) {
				_spline_nodes.push_back(wp);
			}

			// 3. 末尾ダミー1：最初のノード (0) -> これで一周して繋がる
			_spline_nodes.push_back(_user_way_points[0]);

			// 4. 末尾ダミー2：2番目のノード (1) -> 最初のノードを通り過ぎる瞬間のカーブの勢いを維持
			_spline_nodes.push_back(_user_way_points[1]);
		}
		else {
			// 【通常モード（直線的）のノード構築】
			_spline_nodes.reserve(n + 2);
			dx::XMVECTOR start_dummy_pos = dx::XMVectorSubtract(_user_way_points[0].position, dx::XMVectorSubtract(_user_way_points[1].position, _user_way_points[0].position));
			dx::XMVECTOR start_dummy_tar = dx::XMVectorSubtract(_user_way_points[0].target, dx::XMVectorSubtract(_user_way_points[1].target, _user_way_points[0].target));
			_spline_nodes.push_back({ start_dummy_pos, start_dummy_tar });

			for (const auto& wp : _user_way_points) {
				_spline_nodes.push_back(wp);
			}

			dx::XMVECTOR end_dummy_pos = dx::XMVectorAdd(_user_way_points[n - 1].position, dx::XMVectorSubtract(_user_way_points[n - 1].position, _user_way_points[n - 2].position));
			dx::XMVECTOR end_dummy_tar = dx::XMVectorAdd(_user_way_points[n - 1].target, dx::XMVectorSubtract(_user_way_points[n - 1].target, _user_way_points[n - 2].target));
			_spline_nodes.push_back({ end_dummy_pos, end_dummy_tar });
		}
	}

	/**
	 * @brief 指定した再生時刻(秒)でのカメラ位置/注視点を評価して camera_ に書き込む
	 *        update() からも、タイムラインのスクラブ(seek)からも共通で利用される。
	 */
	void evaluate_at_time(float time) {
		if (_user_way_points.size() < 2 || _spline_nodes.size() < 4) return;
		if (_time_per_segment <= 0.0f) return;

		float total_segments = _is_loop
			? static_cast<float>(_user_way_points.size())
			: static_cast<float>(_user_way_points.size() - 1);
		float total_duration = total_segments * _time_per_segment;
		if (total_duration <= 0.0f) return;

		time = std::clamp(time, 0.0f, total_duration);

		// 現在のセグメントインデックスと進捗 t
		float raw_index = time / _time_per_segment;
		int current_segment = static_cast<int>(floorf(raw_index));
		float t = raw_index - static_cast<float>(current_segment);

		// 安全ガード
		if (current_segment >= static_cast<int>(total_segments)) {
			current_segment = static_cast<int>(total_segments) - 1;
			t = 1.0f;
		}
		if (current_segment < 0) {
			current_segment = 0;
			t = 0.0f;
		}

		// _spline_nodes から4本の制御点インデックスを取り出す
		int p0 = current_segment;
		int p1 = current_segment + 1;
		int p2 = current_segment + 2;
		int p3 = current_segment + 3;

		// Catmull-Rom スプライン補間
		camera_.position = dx::XMVectorCatmullRom(_spline_nodes[p0].position, _spline_nodes[p1].position, _spline_nodes[p2].position, _spline_nodes[p3].position, t);
		camera_.target = dx::XMVectorCatmullRom(_spline_nodes[p0].target, _spline_nodes[p1].target, _spline_nodes[p2].target, _spline_nodes[p3].target, t);

		// アップベクトルの直交化
		dx::XMVECTOR world_up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		dx::XMVECTOR forward = dx::XMVector3Normalize(dx::XMVectorSubtract(camera_.target, camera_.position));
		dx::XMVECTOR right = dx::XMVector3Normalize(dx::XMVector3Cross(world_up, forward));
		camera_.up = dx::XMVector3Normalize(dx::XMVector3Cross(forward, right));
	}

	/**
	 * @brief 演出再生を開始する（ループの有無でノード構築を分岐）
	 */
	void play(bool loop = false, float duration_per_node = 2.0f) {
		if (_user_way_points.size() < 2) return;

		_is_playing = true;
		_is_loop = loop;
		_current_time = 0.0f;
		_time_per_segment = (duration_per_node > 0.0f) ? duration_per_node : 0.01f;

		rebuild_spline_nodes(_is_loop);
	}

	void stop() { _is_playing = false; }

	/**
	 * @brief 任意の再生時刻(秒)へジャンプし、その時点のカメラ姿勢を即時反映する。
	 *        再生中でなければ Playback 設定(_default_*) を使ってノードを再構築する
	 *        （タイムラインのスクラブ操作から呼び出される）
	 */
	void seek(float time_seconds) {
		if (_user_way_points.size() < 2) return;

		if (!_is_playing) {
			_is_loop = _default_loop_playback;
			_time_per_segment = (_default_duration_per_node > 0.0f) ? _default_duration_per_node : 0.01f;
			rebuild_spline_nodes(_is_loop);
		}

		float total_segments = _is_loop
			? static_cast<float>(_user_way_points.size())
			: static_cast<float>(_user_way_points.size() - 1);
		float total_duration = total_segments * _time_per_segment;

		_current_time = std::clamp(time_seconds, 0.0f, total_duration);
		evaluate_at_time(_current_time);
	}

	void update(float deltaTime) override {
		if (!_is_playing || _user_way_points.size() < 2) return;

		_current_time += deltaTime;

		// 総再生セグメント数の決定（ループなら最後の点から最初の点へ戻る1区間が増える）
		float total_segments = _is_loop ? static_cast<float>(_user_way_points.size()) : static_cast<float>(_user_way_points.size() - 1);
		float total_duration = total_segments * _time_per_segment;

		if (_current_time >= total_duration) {
			if (_is_loop) {
				_current_time = (total_duration > 0.0f) ? fmodf(_current_time, total_duration) : 0.0f;
			}
			else {
				_current_time = total_duration;
				_is_playing = false;
			}
		}

		evaluate_at_time(_current_time);
	}

	//=========================================================================
	//  JSON 保存 / 読込
	//=========================================================================

	// JSON版 保存関数
	bool save_to_file(const std::string& filename) {
		_json_manager.Clear();

		// 各ウェイポイントをJSON形式へ変換
		for (const auto& wp : _user_way_points) {
			dx::XMFLOAT4 pos, tar;
			dx::XMStoreFloat4(&pos, wp.position);
			dx::XMStoreFloat4(&tar, wp.target);

			json item;
			item["pos"] = { pos.x, pos.y, pos.z };
			item["tar"] = { tar.x, tar.y, tar.z };
			_json_manager.Append("waypoints", item);
		}

		// 再生設定（ループ有無 / ノード間の再生時間）もまとめて保存する
		json settings;
		settings["loop"] = _default_loop_playback;
		settings["duration_per_node"] = _default_duration_per_node;
		_json_manager.Set("settings", settings);

		_json_manager.SetFilepath(_root_directory + filename);
		bool success = _json_manager.Save();

		if (success) {
			refresh_file_list();
			CustomUI::AddToast("Saved: " + filename, ImGuiCol_PlotLinesHovered, 2.5f);
		}
		else {
			CustomUI::AddToast("Failed to save: " + filename, ImGuiCol_NavHighlight, 3.0f);
		}
		return success;
	}

	// JSON版 ロード関数
	bool load_from_file(const std::string& filename) {
		_json_manager.SetFilepath(_root_directory + filename);
		if (!_json_manager.Load()) {
			CustomUI::AddToast("Failed to load: " + filename, ImGuiCol_NavHighlight, 3.0f);
			return false;
		}

		stop();
		_user_way_points.clear();
		_wp_expanded.clear();

		try {
			// 配列データを取得して展開（"waypoints" が存在しない/不正な場合は空配列扱い）
			const json& root = _json_manager.All();
			const json wps = root.value("waypoints", json::array());

			for (const auto& item : wps) {
				auto p = item.at("pos");
				auto t = item.at("tar");

				_user_way_points.push_back({
					dx::XMVectorSet(p[0], p[1], p[2], 1.0f),
					dx::XMVectorSet(t[0], t[1], t[2], 1.0f)
					});
			}
		}
		catch (const std::exception&) {
			CustomUI::AddToast("Invalid waypoint data: " + filename, ImGuiCol_NavHighlight, 3.0f);
			_user_way_points.clear();
		}

		// 再生設定の復元（存在しない場合はデフォルト値を使用）
		_default_loop_playback = _json_manager.GetOr<bool>("settings.loop", false);
		_default_duration_per_node = _json_manager.GetOr<float>("settings.duration_per_node", 2.0f);

		_current_time = 0.0f;
		_spline_nodes.clear();

		CustomUI::AddToast("Loaded: " + filename, ImGuiCol_PlotLinesHovered, 2.5f);
		return true;
	}

	void add_way_point(const dx::XMVECTOR& pos, const dx::XMVECTOR& target) { _user_way_points.push_back({ pos, target }); }

	void clear_way_points() {
		_user_way_points.clear();
		_spline_nodes.clear();
		_wp_expanded.clear();
		_current_time = 0.0f;
		stop();
	}

	/**
	 * @brief 指定したインデックスの「直後」にウェイポイントを挿入する
	 * @param index 挿入基準となるノードのインデックス
	 * @param pos 挿入するカメラ座標
	 * @param target 挿入する注視点座標
	 */
	void insert_way_point_after(size_t index, const dx::XMVECTOR& pos, const dx::XMVECTOR& target) {
		if (index >= _user_way_points.size()) {
			// インデックスが不正、または末尾以上の場合は普通に末尾に追加
			_user_way_points.push_back({ pos, target });
			return;
		}
		// 指定インデックスの次の位置（begin + index + 1）にインサート
		_user_way_points.insert(_user_way_points.begin() + index + 1, { pos, target });

		// パスが変わったため、再生中なら安全のために一度止める
		stop();
	}

	//=========================================================================
	//  [オーバーライド] ImGui用デバッグUIの描画
	//=========================================================================

	void draw_tab_storage() {
		CustomUI::SectionLabel("Saved Cinematic Paths");

		ImGui::TextDisabled("Location: %s", _root_directory.c_str());
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70.0f);
		if (CustomUI::Button("Refresh", ImVec2(70, 0))) { refresh_file_list(); }

		ImGui::BeginChild("FileBrowser", ImVec2(0, 90), true);
		if (_detected_files.empty()) {
			ImGui::TextDisabled(" (No .json paths found) ");
		}
		else {
			for (const auto& file : _detected_files) {
				bool is_selected = (_selected_file_path == file);
				ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.16f, 0.19f, 0.31f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.20f, 0.24f, 0.40f, 1.0f));
				if (ImGui::Selectable(file.c_str(), is_selected)) {
					_selected_file_path = file;
					size_t dot_pos = file.find(".json");
					std::string name = (dot_pos != std::string::npos) ? file.substr(0, dot_pos) : file;
					strcpy_s(_save_name_buffer, name.c_str());
				}
				ImGui::PopStyleColor(2);
			}
		}
		ImGui::EndChild();

		if (!_selected_file_path.empty()) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.85f, 0.40f, 1.0f));
			ImGui::TextUnformatted(_selected_file_path.c_str());
			ImGui::PopStyleColor();

			float btn_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
			if (CustomUI::Button("Load", ImVec2(btn_w, 26))) { load_from_file(_selected_file_path); }
			ImGui::SameLine();
			if (CustomUI::Button("Delete", ImVec2(btn_w, 26))) {
				fs::remove(_root_directory + _selected_file_path);
				CustomUI::AddToast("Deleted: " + _selected_file_path, ImGuiCol_NavHighlight, 2.5f);
				_selected_file_path = "";
				refresh_file_list();
			}
		}

		CustomUI::Separator();
		ImGui::TextDisabled("Save current waypoints as:");
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
		CustomUI::InputText(".json", _save_name_buffer, IM_ARRAYSIZE(_save_name_buffer));
		ImGui::SameLine();
		if (CustomUI::Button("Save", ImVec2(70, 0))) {
			if (strlen(_save_name_buffer) > 0) {
				std::string save_name = std::string(_save_name_buffer) + ".json";
				if (save_to_file(save_name)) { _selected_file_path = save_name; }
			}
		}
	}

	void draw_tab_playback() {
		CustomUI::SectionLabel("Playback Settings");

		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 140.0f);
		CustomUI::SliderFloat("Duration / node", &_default_duration_per_node, 0.5f, 10.0f, "%.1f s");
		CustomUI::Checkbox("Loop playback", &_default_loop_playback);

		float btn_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
		bool can_play = _user_way_points.size() >= 2;

		if (!can_play) ImGui::BeginDisabled();
		if (CustomUI::Button(_is_playing ? "Pause" : "Play", ImVec2(btn_w, 28))) {
			if (_is_playing) stop();
			else play(_default_loop_playback, _default_duration_per_node);
		}
		ImGui::SameLine();
		if (CustomUI::Button("Stop", ImVec2(btn_w, 28))) {
			stop();
			seek(0.0f);
		}
		if (!can_play) ImGui::EndDisabled();

		CustomUI::SectionLabel("Timeline");

		if (!can_play) {
			ImGui::TextDisabled("Add at least 2 waypoints to enable the timeline.");
			return;
		}

		// 再生中はアクティブな再生パラメータを、停止中はUI上の設定値をタイムライン表示に使う
		bool  loop_for_display = _is_playing ? _is_loop : _default_loop_playback;
		float seg_duration = _is_playing ? _time_per_segment : _default_duration_per_node;
		if (seg_duration <= 0.0f) seg_duration = 0.01f;

		int total_segments = loop_for_display
			? static_cast<int>(_user_way_points.size())
			: static_cast<int>(_user_way_points.size() - 1);
		float total_duration = total_segments * seg_duration;
		int max_frames = max(1, static_cast<int>(std::round(total_duration * kTimelineFPS)));

		// 各ウェイポイントの再生位置をキーフレーム（ひし形マーカー）として可視化
		// ※ 毎フレーム再構築するため、タイムライン上での右クリック追加/削除は次フレームで元に戻る
		_timeline_keyframes.clear();
		_timeline_keyframes.reserve(_user_way_points.size() + (loop_for_display ? 1 : 0));
		for (int i = 0; i < static_cast<int>(_user_way_points.size()); ++i) {
			_timeline_keyframes.push_back(static_cast<int>(std::round(i * seg_duration * kTimelineFPS)));
		}
		if (loop_for_display) {
			// 最後のウェイポイントから先頭へ戻る、ループの閉区間を表すマーカー
			_timeline_keyframes.push_back(max_frames);
		}

		int current_frame = static_cast<int>(std::round(_current_time * kTimelineFPS));
		current_frame = std::clamp(current_frame, 0, max_frames);

		bool changed = CustomUI::EditableTimeline("##CineTimeline", &current_frame, max_frames, _timeline_keyframes, ImVec2(0, 50));
		if (changed) {
			float new_time = static_cast<float>(current_frame) / static_cast<float>(kTimelineFPS);
			seek(new_time);
		}

		ImGui::Spacing();
		CustomUI::LoadingBar("Progress", total_duration > 0.0f ? (_current_time / total_duration) : 0.0f);
		ImGui::TextDisabled("Frame %d / %d   (%.2fs / %.2fs)", current_frame, max_frames, _current_time, total_duration);
	}

	void draw_tab_waypoints() {
		char wp_header[64];
		sprintf_s(wp_header, "Registered Waypoints  (%d pts)", static_cast<int>(_user_way_points.size()));
		CustomUI::GradientHeader(wp_header, &_wp_section_open);

		if (!_wp_section_open) return;

		ImGui::Spacing();

		// --- ツールバー: Capture / Clear All ---
		if (CustomUI::Button("+ Capture Current Camera", ImVec2(ImGui::GetContentRegionAvail().x - 90, 26))) {
			auto activeCam = Camera_Manager::instance().get_active_camera();
			if (activeCam && activeCam.get() != this) {
				const auto& data = activeCam->get_camera();
				add_way_point(data.position, data.target);
				CustomUI::AddToast("Waypoint captured", ImGuiCol_PlotLinesHovered, 1.8f);
			}
		}
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.10f, 0.10f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.12f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.55f, 0.66f, 1.0f));
		if (CustomUI::Button("Clear All", ImVec2(-1, 26))) {
			clear_way_points();
			CustomUI::AddToast("All waypoints cleared", ImGuiCol_NavHighlight, 2.0f);
		}
		ImGui::PopStyleColor(3);

		ImGui::Spacing();

		// --- テーブルヘッダー ---
		constexpr ImGuiTableFlags tbl_flags =
			ImGuiTableFlags_BordersInnerV |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_RowBg;

		if (ImGui::BeginTable("WaypointTable", 4, tbl_flags, ImVec2(0, 220))) {
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28.0f);
			ImGui::TableSetupColumn("Position (x / y / z)", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Target (x / y / z)", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 160.0f);
			ImGui::TableHeadersRow();

			for (size_t i = 0; i < _user_way_points.size(); ++i) {
				ImGui::PushID(static_cast<int>(i));

				dx::XMFLOAT4 pos, tar;
				dx::XMStoreFloat4(&pos, _user_way_points[i].position);
				dx::XMStoreFloat4(&tar, _user_way_points[i].target);

				// --- サマリー行 ---
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::TextDisabled("%d", static_cast<int>(i));

				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.2f / %.2f / %.2f", pos.x, pos.y, pos.z);

				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%.2f / %.2f / %.2f", tar.x, tar.y, tar.z);

				ImGui::TableSetColumnIndex(3);

				// Jump ボタン
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.15f, 0.08f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.19f, 0.10f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.89f, 0.68f, 1.0f));
				if (ImGui::SmallButton("Jump")) {
					stop();
					auto activeCam = Camera_Manager::instance().get_active_camera();
					if (activeCam) {
						dx::XMVECTOR forward = dx::XMVector3Normalize(dx::XMVectorSubtract(_user_way_points[i].target, _user_way_points[i].position));
						dx::XMFLOAT4 f4; dx::XMStoreFloat4(&f4, forward);
						float pitch_deg = dx::XMConvertToDegrees(asinf(f4.y));
						float yaw_deg = dx::XMConvertToDegrees(atan2f(f4.z, f4.x));
						if (auto sp = std::dynamic_pointer_cast<Spectator_Camera>(activeCam))
							sp->set_rotation(yaw_deg, pitch_deg, _user_way_points[i].position, _user_way_points[i].target);
						else if (auto tp = std::dynamic_pointer_cast<Third_Person_Camera>(activeCam))
							tp->set_rotation(yaw_deg, pitch_deg, _user_way_points[i].position, _user_way_points[i].target);
						else {
							auto& c = const_cast<Camera&>(activeCam->get_camera());
							c.position = _user_way_points[i].position;
							c.target = _user_way_points[i].target;
							c.identity();
						}
						CustomUI::AddToast("Jumped to waypoint " + std::to_string(i), ImGuiCol_PlotLinesHovered, 1.5f);
					}
				}
				ImGui::PopStyleColor(3);

				ImGui::SameLine(0, 3);

				// Insert ボタン
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.14f, 0.23f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.13f, 0.17f, 0.28f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.54f, 0.71f, 0.98f, 1.0f));
				if (ImGui::SmallButton("Ins")) {
					auto activeCam = Camera_Manager::instance().get_active_camera();
					if (activeCam && activeCam.get() != this) {
						const auto& data = activeCam->get_camera();
						insert_way_point_after(i, data.position, data.target);
						CustomUI::AddToast("Inserted after waypoint " + std::to_string(i), ImGuiCol_PlotLinesHovered, 1.8f);
					}
				}
				ImGui::PopStyleColor(3);

				ImGui::SameLine(0, 3);

				// Overwrite ボタン
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.18f, 0.13f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.22f, 0.16f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.89f, 0.63f, 1.0f));
				if (ImGui::SmallButton("Ovr")) {
					auto activeCam = Camera_Manager::instance().get_active_camera();
					if (activeCam && activeCam.get() != this) {
						const auto& data = activeCam->get_camera();
						_user_way_points[i].position = data.position;
						_user_way_points[i].target = data.target;
						if (_is_playing) stop();
						CustomUI::AddToast("Overwrote waypoint " + std::to_string(i), ImGuiCol_PlotLinesHovered, 1.8f);
					}
				}
				ImGui::PopStyleColor(3);

				ImGui::SameLine(0, 3);

				// Delete ボタン
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.10f, 0.10f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.12f, 0.12f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.55f, 0.66f, 1.0f));
				if (ImGui::SmallButton("Del")) {
					_user_way_points.erase(_user_way_points.begin() + i);
					_wp_expanded.erase(i);
					_current_time = 0.0f;
					stop();
					CustomUI::AddToast("Deleted waypoint " + std::to_string(i), ImGuiCol_NavHighlight, 1.8f);
				}
				ImGui::PopStyleColor(3);

				// --- 展開行 ---
				bool& expanded = _wp_expanded[i];
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(1);
				char sel_lbl[32]; sprintf_s(sel_lbl, "##sel%zu", i);
				if (ImGui::Selectable(sel_lbl, expanded, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
					expanded = !expanded;
				}

				if (expanded) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(1);
					ImGui::Spacing();
					const float btn_w2 = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

					// Insert After (詳細)
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.14f, 0.23f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.13f, 0.17f, 0.28f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.54f, 0.71f, 0.98f, 1.0f));
					char ins_lbl[64]; sprintf_s(ins_lbl, "Insert after [%zu]", i);
					if (ImGui::Button(ins_lbl, ImVec2(btn_w2, 22))) {
						auto activeCam = Camera_Manager::instance().get_active_camera();
						if (activeCam && activeCam.get() != this) {
							const auto& data = activeCam->get_camera();
							insert_way_point_after(i, data.position, data.target);
							CustomUI::AddToast("Inserted after waypoint " + std::to_string(i), ImGuiCol_PlotLinesHovered, 1.8f);
						}
					}
					ImGui::PopStyleColor(3);
					ImGui::SameLine();

					// Overwrite (詳細)
					if (ImGui::Button("Overwrite", ImVec2(btn_w2, 22))) {
						auto activeCam = Camera_Manager::instance().get_active_camera();
						if (activeCam && activeCam.get() != this) {
							const auto& data = activeCam->get_camera();
							_user_way_points[i].position = data.position;
							_user_way_points[i].target = data.target;
							if (_is_playing) stop();
							CustomUI::AddToast("Overwrote waypoint " + std::to_string(i), ImGuiCol_PlotLinesHovered, 1.8f);
						}
					}
					ImGui::Spacing();
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}

	void draw_imgui() override {
		if (ImGui::BeginTabBar("WaypointSystemTabs")) {
			if (ImGui::BeginTabItem("Storage")) {
				draw_tab_storage();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Playback")) {
				draw_tab_playback();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Waypoints")) {
				draw_tab_waypoints();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		// トースト通知の描画。
		// ※ 他の場所(グローバルなUIループ)で CustomUI::RenderToasts() を毎フレーム
		//   呼び出している場合は、ここでの呼び出しは不要（重複描画になります）。
		CustomUI::RenderToasts(ImGui::GetIO().DeltaTime);
	}
};