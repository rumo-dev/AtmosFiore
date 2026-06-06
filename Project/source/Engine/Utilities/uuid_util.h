#pragma once

#include <string>
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <array>
#include <regex>

namespace UUID_Util
{
	/**
	 * @brief UUIDの生データ型（16バイト固定長配列）
	 */
	using uuid_bytes = std::array<uint8_t, 16>;

	/**
	 * @brief ランダムなUUID v4を生成します（RFC 4122 準拠）
	 *
	 * 内部的には64bit乱数を2つ生成し、16バイトの配列に格納。
	 * その後、UUID v4の仕様に従いバージョンビットとバリアントビットを設定します。
	 *
	 * @return std::string 生成されたUUID文字列（形式: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx）
	 */
	inline std::string generate_uuid_v4()
	{
		std::random_device rd;
		std::mt19937_64 gen(rd());
		std::uniform_int_distribution<uint64_t> dis;

		uint64_t part1 = dis(gen);
		uint64_t part2 = dis(gen);

		uuid_bytes bytes;
		for (int i = 0; i < 8; ++i) {
			bytes[i] = static_cast<uint8_t>((part1 >> (8 * (7 - i))) & 0xFF);
			bytes[i + 8] = static_cast<uint8_t>((part2 >> (8 * (7 - i))) & 0xFF);
		}

		// バージョンとバリアントを設定
		bytes[6] = (bytes[6] & 0x0F) | 0x40; // version 4
		bytes[8] = (bytes[8] & 0x3F) | 0x80; // variant 10xxxxxx

		std::ostringstream oss;
		for (int i = 0; i < 16; ++i) {
			if (i == 4 || i == 6 || i == 8 || i == 10)
				oss << '-';
			oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
		}
		return oss.str();
	}

	/**
	 * @brief ランダムなUUID v4を生成し、ハイフン無しの32文字形式で返します
	 *
	 * 内部で generate_uuid_v4() を呼び出し、ハイフンを削除して返却します。
	 *
	 * @return std::string 生成された32文字のUUID（例: 550e8400e29b41d4a716446655440000）
	 */
	inline std::string generate_compact_uuid()
	{
		std::string uuid = generate_uuid_v4();
		uuid.erase(std::remove(uuid.begin(), uuid.end(), '-'), uuid.end());
		return uuid;
	}

	/**
	 * @brief UUIDをバイト列（16バイト配列）として返します
	 *
	 * 内部では generate_uuid_v4() を呼び出し、16進文字列を1バイトごとに変換して格納します。
	 *
	 * @return uuid_bytes 生成されたUUIDの16バイト配列
	 */
	inline uuid_bytes generate_uuid_bytes()
	{
		std::string uuid = generate_uuid_v4();
		uuid_bytes result = {};
		int index = 0;

		for (char c : uuid)
		{
			if (c == '-') continue;

			uint8_t byte = static_cast<uint8_t>(std::stoi(std::string(1, c), nullptr, 16));
			if (index % 2 == 0)
				result[index / 2] = byte << 4;
			else
				result[index / 2] |= byte;

			++index;
		}
		return result;
	}

	/**
	 * @brief 文字列がUUID v4 形式に準拠しているか検証します
	 *
	 * RFC 4122 に準拠した形式（バージョン4、バリアント1）を対象とします。
	 *
	 * @param uuid 判定対象の文字列
	 * @return true UUID v4 形式に一致する場合
	 * @return false 一致しない場合
	 */
	inline bool is_valid_uuid(const std::string& uuid)
	{
		static const std::regex uuid_regex(
			R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-4[0-9a-fA-F]{3}-[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12})");
		return std::regex_match(uuid, uuid_regex);
	}

#ifdef USE_IMGUI
#include "Engine/Graphics/UI/ImGui/imgui.h"
	/**
	 * @brief UUIDをImGui上に表示します（デバッグ用）
	 *
	 * @param uuid 表示するUUID文字列
	 * @param label ラベル文字列（デフォルト: "UUID"）
	 */
	inline void print_uuid_debug(const std::string& uuid, const char* label = "UUID")
	{
		ImGui::Text("%s: %s", label, uuid.c_str());
	}
#endif
}
