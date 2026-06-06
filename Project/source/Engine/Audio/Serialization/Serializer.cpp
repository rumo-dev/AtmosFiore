#include "Serializer.h"
#include <fstream>

namespace Audio {

bool Serializer::SaveGraph(const std::string& path) {
	std::ofstream ofs(path, std::ios::binary);
	if (!ofs) return false;
	// minimal placeholder
	ofs << "{}";
	return true;
}

bool Serializer::LoadGraph(const std::string& path) {
	std::ifstream ifs(path, std::ios::binary);
	if (!ifs) return false;
	// minimal placeholder
	return true;
}

} // namespace Audio
