#pragma once
#include "camera_base.h"
#include <Windows.h>
#include <DirectXMath.h>
#include <vector>
#include <fstream>
#include <string>
#include <filesystem>
#include "Engine/Graphics/UI/ImGui/ImGui.h"
#include "camera_manager.h"

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
class Cinematic_Camera : public Camera_Base {
private:
	std::vector<Camera_Way_Point> _user_way_points;
	std::vector<Camera_Way_Point> _spline_nodes;

	bool _is_playing = false;
	bool _is_loop = false;

	float _current_time = 0.0f;
	float _time_per_segment = 2.0f;

	std::string _root_directory = "data/cinematic_paths/";
	char _save_name_buffer[64] = "new_path";
	std::string _selected_file_path = "";
	std::vector<std::string> _detected_files;
	// クラスのメンバ変数として追加
	std::unordered_map<size_t, bool> _wp_expanded;

public:
	Cinematic_Camera() {
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
			if (entry.is_regular_file() && entry.path().extension() == ".txt") {
				_detected_files.push_back(entry.path().filename().string());
			}
		}
	}

	/**
	 * @brief 演出再生を開始する（ループの有無でノード構築を分岐）
	 */
	void play(bool loop = false, float duration_per_node = 2.0f) {
		if (_user_way_points.size() < 2) return;

		_is_playing = true;
		_is_loop = loop;
		_current_time = 0.0f;
		_time_per_segment = duration_per_node;

		_spline_nodes.clear();
		size_t n = _user_way_points.size();

		if (_is_loop) {
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

	void stop() { _is_playing = false; }

	void update(float deltaTime) override {
		if (!_is_playing || _user_way_points.size() < 2) return;

		_current_time += deltaTime;

		// 総再生セグメント数の決定（ループなら最後の点から最初の点へ戻る1区間が増える）
		float total_segments = _is_loop ? static_cast<float>(_user_way_points.size()) : static_cast<float>(_user_way_points.size() - 1);
		float total_duration = total_segments * _time_per_segment;

		if (_current_time >= total_duration) {
			if (_is_loop) {
				_current_time = fmodf(_current_time, total_duration);
			}
			else {
				_current_time = total_duration;
				_is_playing = false;
				return;
			}
		}

		// 現在のセグメントインデックスと進捗 t
		float raw_index = _current_time / _time_per_segment;
		int current_segment = static_cast<int>(floorf(raw_index));
		float t = raw_index - static_cast<float>(current_segment);

		// 安全ガード
		if (current_segment >= static_cast<int>(total_segments)) {
			current_segment = static_cast<int>(total_segments) - 1;
			t = 1.0f;
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

	// (save_to_file, load_from_file, add_way_point, clear_way_points は前回同様のため実装割愛)
	bool save_to_file(const std::string& filename) { std::string full_path = _root_directory + filename; if (filename.find(".txt") == std::string::npos) { full_path += ".txt"; } std::ofstream ofs(full_path); if (!ofs.is_open()) return false; ofs << _user_way_points.size() << "\n"; for (const auto& wp : _user_way_points) { dx::XMFLOAT4 pos, tar; dx::XMStoreFloat4(&pos, wp.position); dx::XMStoreFloat4(&tar, wp.target); ofs << pos.x << " " << pos.y << " " << pos.z << " " << pos.w << " " << tar.x << " " << tar.y << " " << tar.z << " " << tar.w << "\n"; } refresh_file_list(); return true; }
	bool load_from_file(const std::string& filename) { std::string full_path = _root_directory + filename; std::ifstream ifs(full_path); if (!ifs.is_open()) return false; stop(); _user_way_points.clear(); size_t size = 0; if (!(ifs >> size)) return false; _user_way_points.reserve(size); for (size_t i = 0; i < size; ++i) { dx::XMFLOAT4 pos, tar; ifs >> pos.x >> pos.y >> pos.z >> pos.w >> tar.x >> tar.y >> tar.z >> tar.w; Camera_Way_Point wp; wp.position = dx::XMLoadFloat4(&pos); wp.target = dx::XMLoadFloat4(&tar); _user_way_points.push_back(wp); } return true; }
	void add_way_point(const dx::XMVECTOR& pos, const dx::XMVECTOR& target) { _user_way_points.push_back({ pos, target }); }
	void clear_way_points() { _user_way_points.clear(); _spline_nodes.clear(); stop(); }
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
	/**
	 * @brief [オーバーライド] ImGui用デバッグUIの描画
	 */
	void draw_imgui() override {

		// ===== Path Storage Explorer =====
		if (ImGui::CollapsingHeader("  Path Storage Explorer", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::TextDisabled("Location: %s", _root_directory.c_str());

			ImGui::BeginGroup();
			ImGui::Text("Available Paths:");
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
			if (ImGui::Button("Refresh")) { refresh_file_list(); }
			ImGui::EndGroup();

			ImGui::BeginChild("FileBrowser", ImVec2(0, 80), true);
			if (_detected_files.empty()) {
				ImGui::TextDisabled(" (No .txt paths found) ");
			}
			else {
				for (const auto& file : _detected_files) {
					bool is_selected = (_selected_file_path == file);
					ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.16f, 0.19f, 0.31f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.20f, 0.24f, 0.40f, 1.0f));
					if (ImGui::Selectable(file.c_str(), is_selected)) {
						_selected_file_path = file;
						size_t dot_pos = file.find(".txt");
						std::string name_without_ext = (dot_pos != std::string::npos) ? file.substr(0, dot_pos) : file;
						strcpy_s(_save_name_buffer, name_without_ext.c_str());
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
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.18f, 0.13f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.22f, 0.16f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.89f, 0.63f, 1.0f));
				if (ImGui::Button("Load", ImVec2(btn_w, 24))) { load_from_file(_selected_file_path); }
				ImGui::PopStyleColor(3);

				ImGui::SameLine();

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.10f, 0.10f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.12f, 0.12f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.55f, 0.66f, 1.0f));
				if (ImGui::Button("Delete", ImVec2(btn_w, 24))) {
					fs::remove(_root_directory + _selected_file_path);
					_selected_file_path = "";
					refresh_file_list();
				}
				ImGui::PopStyleColor(3);
			}

			ImGui::Separator();
			ImGui::TextDisabled("Save current waypoints:");
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70);
			ImGui::InputText(".txt", _save_name_buffer, IM_ARRAYSIZE(_save_name_buffer));
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.14f, 0.23f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.13f, 0.17f, 0.28f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.54f, 0.71f, 0.98f, 1.0f));
			if (ImGui::Button("Save")) {
				if (strlen(_save_name_buffer) > 0) {
					std::string save_name = std::string(_save_name_buffer) + ".txt";
					if (save_to_file(save_name)) { _selected_file_path = save_name; }
				}
			}
			ImGui::PopStyleColor(3);
		}

		// ===== Playback Control =====
		if (ImGui::CollapsingHeader("  Playback Control", ImGuiTreeNodeFlags_DefaultOpen)) {

			static float durationPerNode = 2.0f;
			static bool  loopPlayback = false;

			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 140);
			ImGui::SliderFloat("Duration / node", &durationPerNode, 0.5f, 10.0f, "%.1f s");
			ImGui::Checkbox("Loop playback", &loopPlayback);

			float btn_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.18f, 0.13f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.22f, 0.16f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.89f, 0.63f, 1.0f));
			if (ImGui::Button("Play", ImVec2(btn_w, 28))) { play(loopPlayback, durationPerNode); }
			ImGui::PopStyleColor(3);

			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.10f, 0.10f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.12f, 0.12f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.55f, 0.66f, 1.0f));
			if (ImGui::Button("Stop", ImVec2(btn_w, 28))) { stop(); }
			ImGui::PopStyleColor(3);

			if (_is_playing) {
				float total_segments = _is_loop
					? static_cast<float>(_user_way_points.size())
					: static_cast<float>(_user_way_points.size() - 1);
				float total_duration = total_segments * _time_per_segment;
				ImGui::ProgressBar(_current_time / total_duration, ImVec2(-1, 0), "Playing...");
			}
		}

		// ===== Registered Waypoints =====
		char wp_header[64];
		sprintf_s(wp_header, "  Registered Waypoints  (%d pts)", static_cast<int>(_user_way_points.size()));
		if (ImGui::CollapsingHeader(wp_header, ImGuiTreeNodeFlags_DefaultOpen)) {

			// --- ツールバー: Capture / Clear All ---
			if (ImGui::Button("+ Capture Current Camera", ImVec2(ImGui::GetContentRegionAvail().x - 90, 24))) {
				auto activeCam = Camera_Manager::instance().get_active_camera();
				if (activeCam && activeCam.get() != this) {
					const auto& data = activeCam->get_camera();
					add_way_point(data.position, data.target);
				}
			}
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.10f, 0.10f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.12f, 0.12f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.55f, 0.66f, 1.0f));
			if (ImGui::Button("Clear All", ImVec2(-1, 24))) { clear_way_points(); }
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
							dx::XMVECTOR forward = dx::XMVector3Normalize(
								dx::XMVectorSubtract(_user_way_points[i].target, _user_way_points[i].position));
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
							ImGui::PopStyleColor(3);
							ImGui::PopID();
							break;
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
						stop();
						ImGui::PopStyleColor(3);
						ImGui::PopID();
						break;
					}
					ImGui::PopStyleColor(3);

					// --- 展開行: クリックで開閉 ---
					bool& expanded = _wp_expanded[i]; // std::unordered_map<size_t,bool> を持たせる想定
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);

					// 行全体をクリック可能にするため Selectable を透明で敷く
					ImGui::TableSetColumnIndex(1);
					char sel_lbl[32]; sprintf_s(sel_lbl, "##sel%zu", i);
					if (ImGui::Selectable(sel_lbl, expanded,
						ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
						ImVec2(0, 0)))
					{
						expanded = !expanded;
					}

					if (expanded) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(1);
						ImGui::Spacing();

						const float btn_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

						// Insert After ボタン
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.14f, 0.23f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.13f, 0.17f, 0.28f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.54f, 0.71f, 0.98f, 1.0f));
						char ins_lbl[64]; sprintf_s(ins_lbl, "Insert after [%zu]", i);
						if (ImGui::Button(ins_lbl, ImVec2(btn_w, 22))) {
							auto activeCam = Camera_Manager::instance().get_active_camera();
							if (activeCam && activeCam.get() != this) {
								const auto& data = activeCam->get_camera();
								insert_way_point_after(i, data.position, data.target);
								ImGui::PopStyleColor(3);
								ImGui::PopID();
								break;
							}
						}
						ImGui::PopStyleColor(3);

						ImGui::SameLine();

						// Overwrite ボタン
						if (ImGui::Button("Overwrite", ImVec2(btn_w, 22))) {
							auto activeCam = Camera_Manager::instance().get_active_camera();
							if (activeCam && activeCam.get() != this) {
								const auto& data = activeCam->get_camera();
								_user_way_points[i].position = data.position;
								_user_way_points[i].target = data.target;
								if (_is_playing) stop();
							}
						}

						ImGui::Spacing();

					}

					ImGui::PopID();
				}

				ImGui::EndTable();
			}
		}
	}
};