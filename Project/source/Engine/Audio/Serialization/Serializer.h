#pragma once
#include <string>

namespace Audio {

class Serializer {
public:
	static bool SaveGraph(const std::string& path);
	static bool LoadGraph(const std::string& path);
};

} // namespace Audio
