#pragma once
#include <cmath>
#include <algorithm>
#include <functional>
#include <string_view>
#include <vector>
#include <array>
#include "Engine/Graphics/UI/ImGui/imgui.h"
#include "math_util.h"

/**
 * @file easing_util.h
 * @brief イージング関数と関連ユーティリティの定義
 *
 * アニメーションや補間処理で利用する各種イージング関数を提供します。
 * ImGuiを用いたデバッググラフ描画やエディタUIも含みます。
 */

namespace Easing_Util
{

	/**
	 * @brief 0.0～1.0の範囲に値をクランプします。
	 * @param t クランプ対象の値
	 * @return クランプされた値
	 */
	inline constexpr float clamp(float t) {
		return (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
	}

	/**
	 * @brief 線形イージング
	 * @param t 0.0～1.0の正規化値
	 * @return 線形補間値
	 */
	inline float linear(float t) {
		return clamp(t);
	}

	/**
	 * @brief 二次イージング（加速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_quad(float t) {
		t = clamp(t); return t * t;
	}

	/**
	 * @brief 二次イージング（減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_out_quad(float t) {
		t = clamp(t); return 1 - (1 - t) * (1 - t);
	}

	/**
	 * @brief 二次イージング（加速→減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_out_quad(float t) {
		t = clamp(t); return t < 0.5f ? 2 * t * t : 1 - std::powf(-2 * t + 2, 2) / 2;
	}

	/**
	 * @brief 三次イージング（加速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_cubic(float t) {
		t = clamp(t); return t * t * t;
	}

	/**
	 * @brief 三次イージング（減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_out_cubic(float t) {
		t = clamp(t); return 1 - std::powf(1 - t, 3);
	}

	/**
	 * @brief 三次イージング（加速→減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_out_cubic(float t) {
		t = clamp(t); return t < 0.5f ? 4 * t * t * t : 1 - std::powf(-2 * t + 2, 3) / 2;
	}

	/**
	 * @brief 四次イージング（加速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_quart(float t) {
		t = clamp(t); return t * t * t * t;
	}

	/**
	 * @brief 四次イージング（減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_out_quart(float t) {
		t = clamp(t); return 1 - std::powf(1 - t, 4);
	}

	/**
	 * @brief 四次イージング（加速→減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_out_quart(float t) {
		t = clamp(t); return t < 0.5f ? 8 * t * t * t * t : 1 - std::powf(-2 * t + 2, 4) / 2;
	}

	/**
	 * @brief 五次イージング（加速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_quint(float t) {
		t = clamp(t); return t * t * t * t * t;
	}

	/**
	 * @brief 五次イージング（減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_out_quint(float t) {
		t = clamp(t); return 1 - std::powf(1 - t, 5);
	}

	/**
	 * @brief 五次イージング（加速→減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_out_quint(float t) {
		t = clamp(t); return t < 0.5f ? 16 * t * t * t * t * t : 1 - std::powf(-2 * t + 2, 5) / 2;
	}

	/**
	 * @brief サインイージング（加速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_sine(float t) {
		t = clamp(t); return 1 - std::cosf((t * PI) / 2);
	}

	/**
	 * @brief サインイージング（減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_out_sine(float t) {
		t = clamp(t); return std::sinf((t * PI) / 2);
	}

	/**
	 * @brief サインイージング（加速→減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_out_sine(float t) {
		t = clamp(t); return -(std::cosf(PI * t) - 1) / 2;
	}

	/**
	 * @brief 指数イージング（加速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_expo(float t) {
		t = clamp(t); return (t == 0) ? 0 : std::powf(2, 10 * t - 10);
	}

	/**
	 * @brief 指数イージング（減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_out_expo(float t) {
		t = clamp(t); return (t == 1) ? 1 : 1 - std::powf(2, -10 * t);
	}

	/**
	 * @brief 指数イージング（加速→減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_out_expo(float t) {
		t = clamp(t);
		if (t == 0) return 0;
		if (t == 1) return 1;
		return t < 0.5f ? std::powf(2, 20 * t - 10) / 2
			: (2 - std::powf(2, -20 * t + 10)) / 2;
	}

	/**
	 * @brief 円弧イージング（加速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_circ(float t) {
		t = clamp(t); return 1 - std::sqrtf(1 - t * t);
	}

	/**
	 * @brief 円弧イージング（減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_out_circ(float t) {
		t = clamp(t); return std::sqrtf(1 - std::powf(t - 1, 2));
	}

	/**
	 * @brief 円弧イージング（加速→減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_out_circ(float t) {
		t = clamp(t);
		return t < 0.5f
			? (1 - std::sqrtf(1 - 4 * t * t)) / 2
			: (std::sqrtf(1 - std::powf(-2 * t + 2, 2)) + 1) / 2;
	}

	/**
	 * @brief バックイージング（加速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_back(float t) {
		constexpr float c1 = 1.70158f, c3 = c1 + 1;
		t = clamp(t); return c3 * t * t * t - c1 * t * t;
	}

	/**
	 * @brief バックイージング（減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_out_back(float t) {
		constexpr float c1 = 1.70158f, c3 = c1 + 1;
		t = clamp(t); t -= 1;
		return 1 + c3 * t * t * t + c1 * t * t;
	}

	/**
	 * @brief バックイージング（加速→減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_out_back(float t) {
		constexpr float c1 = 1.70158f, c2 = c1 * 1.525f;
		t = clamp(t);
		return t < 0.5f
			? (std::powf(2 * t, 2) * ((c2 + 1) * 2 * t - c2)) / 2
			: (std::powf(2 * t - 2, 2) * ((c2 + 1) * (2 * t - 2) + c2) + 2) / 2;
	}

	/**
	 * @brief エラスティックイージング（加速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_elastic(float t) {
		t = clamp(t);
		if (t == 0) return 0;
		if (t == 1) return 1;
		return -std::powf(2, 10 * t - 10) * std::sinf((t * 10 - 10.75f) * (2 * PI / 3));
	}

	/**
	 * @brief エラスティックイージング（減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_out_elastic(float t) {
		t = clamp(t);
		if (t == 0) return 0;
		if (t == 1) return 1;
		return std::powf(2, -10 * t) * std::sinf((t * 10 - 0.75f) * (2 * PI / 3)) + 1;
	}

	/**
	 * @brief エラスティックイージング（加速→減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_out_elastic(float t) {
		t = clamp(t);
		if (t == 0) return 0;
		if (t == 1) return 1;
		if (t < 0.5f)
			return -0.5f * std::powf(2, 20 * t - 10) * std::sinf((20 * t - 11.125f) * (2 * PI / 4.5f));
		else
			return std::powf(2, -20 * t + 10) * std::sinf((20 * t - 11.125f) * (2 * PI / 4.5f)) * 0.5f + 1;
	}

	/**
	 * @brief バウンスイージング（減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_out_bounce(float t) {
		t = clamp(t);
		constexpr float n1 = 7.5625f, d1 = 2.75f;
		if (t < 1 / d1) {
			return n1 * t * t;
		}
		else if (t < 2 / d1) {
			t -= 1.5f / d1;
			return n1 * t * t + 0.75f;
		}
		else if (t < 2.5f / d1) {
			t -= 2.25f / d1;
			return n1 * t * t + 0.9375f;
		}
		else {
			t -= 2.625f / d1;
			return n1 * t * t + 0.984375f;
		}
	}

	/**
	 * @brief バウンスイージング（加速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_bounce(float t) {
		return 1 - ease_out_bounce(1 - clamp(t));
	}

	/**
	 * @brief バウンスイージング（加速→減速）
	 * @param t 0.0～1.0の正規化値
	 * @return 補間値
	 */
	inline float ease_in_out_bounce(float t) {
		t = clamp(t);
		return t < 0.5f
			? (1 - ease_out_bounce(1 - 2 * t)) / 2
			: (1 + ease_out_bounce(2 * t - 1)) / 2;
	}

	/**
	 * @enum EasingType
	 * @brief イージング関数の種類を表す列挙型
	 */
	enum class EasingType {
		Linear,
		InQuad, OutQuad, InOutQuad,
		InCubic, OutCubic, InOutCubic,
		InQuart, OutQuart, InOutQuart,
		InQuint, OutQuint, InOutQuint,
		InSine, OutSine, InOutSine,
		InExpo, OutExpo, InOutExpo,
		InCirc, OutCirc, InOutCirc,
		InBack, OutBack, InOutBack,
		InElastic, OutElastic, InOutElastic,
		InBounce, OutBounce, InOutBounce,
		Count
	};

	/**
	 * @brief イージング関数名の配列
	 */
	inline constexpr std::array<std::string_view, static_cast<size_t>(EasingType::Count)> easing_names = {
		"Linear",
		"InQuad", "OutQuad", "InOutQuad",
		"InCubic", "OutCubic", "InOutCubic",
		"InQuart", "OutQuart", "InOutQuart",
		"InQuint", "OutQuint", "InOutQuint",
		"InSine", "OutSine", "InOutSine",
		"InExpo", "OutExpo", "InOutExpo",
		"InCirc", "OutCirc", "InOutCirc",
		"InBack", "OutBack", "InOutBack",
		"InElastic", "OutElastic", "InOutElastic",
		"InBounce", "OutBounce", "InOutBounce"
	};

	/**
	 * @brief イージング関数を取得します。
	 * @param type イージングタイプ
	 * @return 対応するイージング関数
	 */
	inline std::function<float(float)> get_easing_function(EasingType type) {
		switch (type) {
		case EasingType::Linear: return linear;
		case EasingType::InQuad: return ease_in_quad;
		case EasingType::OutQuad: return ease_out_quad;
		case EasingType::InOutQuad: return ease_in_out_quad;
		case EasingType::InCubic: return ease_in_cubic;
		case EasingType::OutCubic: return ease_out_cubic;
		case EasingType::InOutCubic: return ease_in_out_cubic;
		case EasingType::InQuart: return ease_in_quart;
		case EasingType::OutQuart: return ease_out_quart;
		case EasingType::InOutQuart: return ease_in_out_quart;
		case EasingType::InQuint: return ease_in_quint;
		case EasingType::OutQuint: return ease_out_quint;
		case EasingType::InOutQuint: return ease_in_out_quint;
		case EasingType::InSine: return ease_in_sine;
		case EasingType::OutSine: return ease_out_sine;
		case EasingType::InOutSine: return ease_in_out_sine;
		case EasingType::InExpo: return ease_in_expo;
		case EasingType::OutExpo: return ease_out_expo;
		case EasingType::InOutExpo: return ease_in_out_expo;
		case EasingType::InCirc: return ease_in_circ;
		case EasingType::OutCirc: return ease_out_circ;
		case EasingType::InOutCirc: return ease_in_out_circ;
		case EasingType::InBack: return ease_in_back;
		case EasingType::OutBack: return ease_out_back;
		case EasingType::InOutBack: return ease_in_out_back;
		case EasingType::InElastic: return ease_in_elastic;
		case EasingType::OutElastic: return ease_out_elastic;
		case EasingType::InOutElastic: return ease_in_out_elastic;
		case EasingType::InBounce: return ease_in_bounce;
		case EasingType::OutBounce: return ease_out_bounce;
		case EasingType::InOutBounce: return ease_in_out_bounce;
		default: return linear;
		}
	}

	/**
	 * @brief イージング関数名を取得します。
	 * @param type イージングタイプ
	 * @return 関数名（文字列ビュー）
	 */
	inline std::string_view get_easing_name(EasingType type) {
		size_t i = static_cast<size_t>(type);
		return (i < easing_names.size()) ? easing_names[i] : "Unknown";
	}

	/**
	 * @brief 全イージングタイプのリストを取得します。
	 * @return EasingTypeのベクター
	 */
	inline std::vector<EasingType> get_all_easing_types() {
		std::vector<EasingType> list;
		for (int i = 0; i < static_cast<int>(EasingType::Count); ++i)
			list.push_back(static_cast<EasingType>(i));
		return list;
	}

	/**
	 * @brief イージング関数のグラフをImGuiで描画します。
	 * @param type イージングタイプ
	 * @param t 現在のt値（0.0～1.0）
	 */
	inline void draw_easing_debug_graph(EasingType type, float& t)
	{
		t = clamp(t);
		const auto& name = get_easing_name(type);
		auto easing_func = get_easing_function(type);

		constexpr int kSampleCount = 100;
		std::array<float, kSampleCount> values;
		for (int i = 0; i < kSampleCount; ++i)
		{
			float x = i / float(kSampleCount - 1);
			values[i] = easing_func(x);
		}

		// 自動Y範囲計算
		float min_y = values[0], max_y = values[0];
		for (int i = 1; i < kSampleCount; ++i)
		{
			min_y = min(min_y, values[i]);
			max_y = max(max_y, values[i]);
		}
		if (std::abs(max_y - min_y) < 1e-4f) {
			max_y += 0.001f;
			min_y -= 0.001f;
		}

		// ImGuiの現在の利用可能横幅取得
		float graph_width = ImGui::GetContentRegionAvail().x;
		float graph_height = graph_width * 0.5f; // 縦は横の半分

		ImGui::Text("%s (t=%.2f, value=%.3f)", name.data(), t, easing_func(t));
		ImGui::PlotLines("##easing_plot", values.data(), kSampleCount, 0, nullptr, min_y, max_y, ImVec2(graph_width, graph_height));

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		ImVec2 pos = ImGui::GetItemRectMin();
		ImVec2 plot_size = ImGui::GetItemRectSize();

		// 現在位置の赤線・赤丸描画
		float x_pos = pos.x + plot_size.x * t;
		float y_norm = (easing_func(t) - min_y) / (max_y - min_y);
		float y_pos = pos.y + (1.0f - y_norm) * plot_size.y;

		draw_list->AddLine(
			ImVec2(x_pos, pos.y),
			ImVec2(x_pos, pos.y + plot_size.y),
			IM_COL32(255, 100, 100, 255), 2.0f
		);
		draw_list->AddCircleFilled(
			ImVec2(x_pos, y_pos),
			4.0f,
			IM_COL32(255, 0, 0, 255)
		);

		// グリッド線とラベル描画設定
		const ImU32 line_color = IM_COL32(150, 150, 150, 100);
		const float line_thickness = 1.0f;
		const ImU32 text_color = IM_COL32(200, 200, 200, 200);
		const float font_size = ImGui::GetFontSize();

		// 縦線とラベル（t=0.0～1.0まで0.1刻み）
		for (int i = 0; i <= 10; ++i)
		{
			float mark = i / 10.0f;
			float x = pos.x + plot_size.x * mark;

			// 縦線
			draw_list->AddLine(
				ImVec2(x, pos.y),
				ImVec2(x, pos.y + plot_size.y),
				line_color,
				line_thickness
			);

			// ラベル（x軸の値を下に表示）
			char buf[8];
			snprintf(buf, sizeof(buf), "%.1f", mark);
			ImVec2 text_size = ImGui::CalcTextSize(buf);
			draw_list->AddText(
				ImVec2(x - text_size.x * 0.5f, pos.y + plot_size.y + 4),
				text_color,
				buf
			);
		}

		// 横線とラベル（y=0.0～1.0まで0.1刻み）
		for (int i = 0; i <= 10; ++i)
		{
			float mark = i / 10.0f;
			float y = pos.y + plot_size.y * (1.0f - mark);

			// 横線
			draw_list->AddLine(
				ImVec2(pos.x, y),
				ImVec2(pos.x + plot_size.x, y),
				line_color,
				line_thickness
			);

			// ラベル（y軸の値を左に表示）
			char buf[8];
			snprintf(buf, sizeof(buf), "%.1f", mark);
			ImVec2 text_size = ImGui::CalcTextSize(buf);
			draw_list->AddText(
				ImVec2(pos.x - text_size.x - 4, y - font_size * 0.5f),
				text_color,
				buf
			);
		}
	}

	/**
	 * @brief イージング関数選択・編集UIをImGuiで表示します。
	 * @param current_type 現在選択中のイージングタイプ（参照渡し）
	 * @param t 現在のt値（参照渡し、0.0～1.0）
	 * @param title ウィンドウタイトル（省略可）
	 */
	inline void draw_easing_editor(EasingType& current_type, float& t, const char* title = "Easing Editor")
	{
#ifdef USE_IMGUI
		ImGui::Begin("EasingEditor");
		if (ImGui::TreeNode(title))
		{
			// コンボ選択
			if (ImGui::BeginCombo("Easing Type", get_easing_name(current_type).data()))
			{
				for (int i = 0; i < static_cast<int>(EasingType::Count); ++i)
				{
					EasingType type = static_cast<EasingType>(i);
					bool selected = (type == current_type);
					if (ImGui::Selectable(get_easing_name(type).data(), selected))
					{
						current_type = type;
					}
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			// tスライダー
			ImGui::SliderFloat("t", &t, 0.0f, 1.0f);

			// グラフ描画
			draw_easing_debug_graph(current_type, t);
			ImGui::TreePop();

			ImGui::Dummy(ImVec2(0.0f, 20.0f)); // 縦に20ピクセル空ける
			ImGui::Separator();

		}

		ImGui::End();
#endif
	}
}
