#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <cstring>
#include <d2d1.h>
#include <stdexcept>
#include <DirectXMath.h>2522
/**
 * @file color_util.h
 * @brief カラー操作ユーティリティ（RGBA構造体・変換・プリセット・ランダム生成など）
 *
 * RGBAカラー構造体、プリセットカラー、HSV/RGB変換、HTMLカラーコード変換、
 * ランダムカラー生成などの機能を提供します。
 */

namespace Color_Utils {

	/**
	 * @brief RGBAカラー構造体。float* に暗黙変換可能。
	 */
	struct Color {
		float r, g, b, a;

		/**
		 * @brief デフォルトコンストラクタ（黒・不透明）
		 */
		constexpr Color() : r(0), g(0), b(0), a(1) {}

		/**
		 * @brief 個別指定によるRGBA初期化
		 * @param r 赤成分（0.0～1.0）
		 * @param g 緑成分（0.0～1.0）
		 * @param b 青成分（0.0～1.0）
		 * @param a アルファ成分（0.0～1.0, 省略時1.0）
		 */
		constexpr Color(float r, float g, float b, float a = 1.0f)
			: r(r), g(g), b(b), a(a) {}

		/**
		 * @brief float[4] からの初期化
		 * @param arr RGBA値配列（float[4]）
		 */
		explicit Color(const float* arr) {
			std::memcpy(&r, arr, sizeof(float) * 4);
		}

		/**
		 * @brief float* への暗黙変換（非const）
		 * @return float*（r成分のアドレス）
		 */
		operator float* () { return &r; }

		/**
		 * @brief const float* への暗黙変換
		 * @return const float*（r成分のアドレス）
		 */
		operator const float* () const { return &r; }
		/**
		* @brief D2D1_COLOR_F への暗黙変換
		* @return D2D1_COLOR_F
		*/
		operator D2D1_COLOR_F() const { return D2D1::ColorF(r, g, b, a); }
		/*
		*@brief DirectX::XMFLOAT4 への暗黙変換
		* @return DirectX::XMFLOAT4
		*/
		operator DirectX::XMFLOAT4() const { return DirectX::XMFLOAT4(r, g, b, a); }
	};

	/**
	 * @namespace Colors
	 * @brief プリセットカラー定義
	 */
	namespace Colors {
		/// @brief 白色
		constexpr Color white(1.0f, 1.0f, 1.0f);
		/// @brief 黒色
		constexpr Color black(0.0f, 0.0f, 0.0f);
		/// @brief 赤色
		constexpr Color red(1.0f, 0.0f, 0.0f);
		/// @brief 緑色
		constexpr Color green(0.0f, 1.0f, 0.0f);
		/// @brief 青色
		constexpr Color blue(0.0f, 0.0f, 1.0f);
		/// @brief 完全透明
		constexpr Color transparent(0.0f, 0.0f, 0.0f, 0.0f);
		/**
		 * @brief グレースケールカラー
		 * @param v 明度（0.0～1.0）
		 * @return グレー色
		 */
		constexpr Color gray(float v) { return Color(v, v, v); }
	}

	/**
	 * @brief HSV→RGB変換
	 * @param h 色相（0.0～1.0）
	 * @param s 彩度（0.0～1.0）
	 * @param v 明度（0.0～1.0）
	 * @param alpha アルファ値（0.0～1.0, 省略時1.0）
	 * @return RGBカラー
	 */
	inline Color hsv_to_rgb(float h, float s, float v, float alpha = 1.0f) {
		float r = 0.0f, g = 0.0f, b = 0.0f;
		int i = static_cast<int>(h * 6);
		float f = h * 6 - i;
		float p = v * (1 - s);
		float q = v * (1 - f * s);
		float t = v * (1 - (1 - f) * s);
		switch (i % 6) {
		case 0: r = v, g = t, b = p; break;
		case 1: r = q, g = v, b = p; break;
		case 2: r = p, g = v, b = t; break;
		case 3: r = p, g = q, b = v; break;
		case 4: r = t, g = p, b = v; break;
		case 5: r = v, g = p, b = q; break;
		}
		return Color(r, g, b, alpha);
	}

	/**
	 * @brief HTMLカラーコード（#RRGGBB または #RRGGBBAA）からColorを生成
	 * @param code HTMLカラーコード文字列
	 * @return 変換されたColor（失敗時は黒）
	 */
	inline Color from_hex(const std::string& code) {
		std::string hex = code;
		if (hex[0] == '#') hex = hex.substr(1);
		if (hex.length() != 6 && hex.length() != 8) return Colors::black;

		auto hex_to_float = [](const std::string& str) -> float {
			return static_cast<float>(std::stoi(str, nullptr, 16)) / 255.0f;
			};

		float r = hex_to_float(hex.substr(0, 2));
		float g = hex_to_float(hex.substr(2, 2));
		float b = hex_to_float(hex.substr(4, 2));
		float a = hex.length() == 8 ? hex_to_float(hex.substr(6, 2)) : 1.0f;

		return Color(r, g, b, a);
	}

	/**
	 * @brief ColorをHTMLカラーコードに変換（#RRGGBB or #RRGGBBAA）
	 * @param color 変換対象のColor
	 * @param withAlpha アルファ値も含める場合true
	 * @return HTMLカラーコード文字列
	 */
	inline std::string to_hex(const Color& color, bool withAlpha = false) {
		auto floatToHex = [](float f) -> std::string {
			int val = std::clamp(static_cast<int>(f * 255.0f + 0.5f), 0, 255);
			std::stringstream ss;
			ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << val;
			return ss.str();
			};

		std::string hex = "#";
		hex += floatToHex(color.r);
		hex += floatToHex(color.g);
		hex += floatToHex(color.b);
		if (withAlpha) hex += floatToHex(color.a);
		return hex;
	}

	/**
	 * @brief HSV空間によるランダムカラー生成
	 * @param saturation 彩度（0.0～1.0, 省略時1.0）
	 * @param value 明度（0.0～1.0, 省略時1.0）
	 * @param alpha アルファ値（0.0～1.0, 省略時1.0）
	 * @return ランダムなColor
	 */
	inline Color random_hsv(float saturation = 1.0f, float value = 1.0f, float alpha = 1.0f) {
		static std::mt19937 rng{ std::random_device{}() };
		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		return hsv_to_rgb(dist(rng), saturation, value, alpha);
	}
	/**
 * @brief HEXカラーコードを D2D1::ColorF に変換します。
 *
 * この関数は `#RRGGBB` 形式の文字列を受け取り、
 * 各成分を 0.0?1.0 の範囲に正規化して D2D1::ColorF に変換します。
 *
 * @param hexColor 変換する HEX カラーコード。形式は "#RRGGBB"。
 * @return D2D1::ColorF 変換後の色
 *
 * @throws std::invalid_argument HEX形式でない場合に投げられます。
 *
 * @example
 * D2D1::ColorF titleColor = HexToColorF("#C9A7EB");
 * D2D1::ColorF backgroundColor = HexToColorF("#1E1B26");
 */
	inline D2D1::ColorF hex_to_colorF(const std::string& hexColor)
	{
		// #RRGGBB形式であることを確認
		if (hexColor.size() != 7 || hexColor[0] != '#') {
			throw std::invalid_argument("Invalid hex color format. Use #RRGGBB.");
		}

		// 16進数文字列を整数に変換
		unsigned int r, g, b;
		std::stringstream ss;

		ss << std::hex << hexColor.substr(1, 2);
		ss >> r;
		ss.clear();

		ss << std::hex << hexColor.substr(3, 2);
		ss >> g;
		ss.clear();

		ss << std::hex << hexColor.substr(5, 2);
		ss >> b;

		// 0?1に変換して ColorF を返す
		return D2D1::ColorF(
			static_cast<FLOAT>(r) / 255.0f,
			static_cast<FLOAT>(g) / 255.0f,
			static_cast<FLOAT>(b) / 255.0f
		);
	}
} // namespace Color_Utils
