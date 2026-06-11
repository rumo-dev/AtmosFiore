#pragma once
#include <string_view>
#include <unordered_map>

// アイコン定義（実際の値に合わせて修正してください）
inline constexpr std::string_view ICON_MAIN = "\ue2da";
inline constexpr std::string_view ICON_DASHBOARD = "\ue154";
inline constexpr std::string_view ICON_SCENE = "\ue2ca";
inline constexpr std::string_view ICON_DEBUG = "\ue224";
inline constexpr std::string_view ICON_RENDER = "\ue8c2";
inline constexpr std::string_view ICON_LIGHT = "\ue5b6";
inline constexpr std::string_view ICON_RESOURCE = "\ueaa2";
inline constexpr std::string_view ICON_CAMERA = "\ue10e";

inline std::string_view get_icon(std::string_view name) {
	static const std::unordered_map<std::string_view, std::string_view> icon_map = {
		{"Main", ICON_MAIN},
		{"Dashboard", ICON_DASHBOARD},
		{"Scene",     ICON_SCENE},
		{"Debug",     ICON_DEBUG},
		{"Rendering", ICON_RENDER},
		{"Light",     ICON_LIGHT},
		{"Resource",  ICON_RESOURCE},
		{"Camera",    ICON_CAMERA}
	};

	for (const auto& [key, icon] : icon_map) {
		if (name.find(key) != std::string_view::npos) {
			return icon;
		}
	}
	return "";
}