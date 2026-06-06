#include "input_layout_parser.h"


using namespace InputLayoutParser;


static std::string ws_to_string(const std::wstring& ws) {
	return std::string(ws.begin(), ws.end()); // naive but often ok for ASCII paths/comments
}


std::vector<InputElementDescInfo> InputLayoutParser::ParseFromHLSL(const std::wstring& hlslPath)
{
	std::vector<InputElementDescInfo> out;
	std::ifstream ifs(hlslPath);
	if (!ifs) return out;


	std::string line;
	// pattern: // @VertexInput SEMANTIC TYPE [INDEX] [SLOT] [INSTANCE]
	std::regex re(R"(^\s*//\s*@VertexInput\s+([A-Za-z0-9_]+)\s+([A-Za-z0-9_]+)\s*(\d*)\s*(\d*)\s*(\d*)\s*$)");


	while (std::getline(ifs, line)) {
		std::smatch m;
		if (std::regex_search(line, m, re)) {
			InputElementDescInfo info;
			info.semanticName = m[1].str();
			std::string type = m[2].str();
			std::string idx = m[3].str();
			std::string slot = m[4].str();
			std::string instance = m[5].str();


			// semantic index: if semantic contains trailing digits, extract them
			// e.g. TEXCOORD0 -> TEXCOORD with index 0
			std::smatch mm;
			if (std::regex_search(info.semanticName, mm, std::regex(R"((\D+)(\d+)$)"))) {
				info.semanticName = mm[1].str();
				info.semanticIndex = std::stoi(mm[2].str());
			}


			if (!idx.empty()) info.semanticIndex = std::stoi(idx);
			if (!slot.empty()) info.inputSlot = std::stoi(slot);
			if (!instance.empty()) {
				info.perInstance = (std::stoi(instance) != 0);
				if (info.perInstance) info.instanceDataStepRate = 1;
			}


			// map type -> DXGI_FORMAT
			if (type == "float") info.format = DXGI_FORMAT_R32_FLOAT;
			else if (type == "float2" || type == "float2_t") info.format = DXGI_FORMAT_R32G32_FLOAT;
			else if (type == "float3" || type == "float3_t") info.format = DXGI_FORMAT_R32G32B32_FLOAT;
			else if (type == "float4" || type == "float4_t") info.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			else if (type == "uint") info.format = DXGI_FORMAT_R32_UINT;
			else if (type == "uchar4") info.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			else {
				// unknown type, skip
				continue;
			}


			out.push_back(info);
		}
	}


	return out;
}


bool InputLayoutParser::SaveLayoutToFile(const std::wstring& layoutPath, const std::vector<InputElementDescInfo>& infos)
{
	std::ofstream ofs(layoutPath, std::ios::trunc);
	if (!ofs) return false;


	// simple text format: one entry per line
	// semantic format semanticIndex inputSlot perInstance instanceDataStepRate
	for (const auto& e : infos) {
		ofs << e.semanticName << ' ' << FormatToString(e.format) << ' ' << e.semanticIndex << ' ' << e.inputSlot << ' ' << (e.perInstance ? 1 : 0) << ' ' << e.instanceDataStepRate << '\n';
	}


	return true;
}

std::vector<InputElementDescInfo> InputLayoutParser::LoadLayoutFromFile(const std::wstring& layoutPath)
{
	std::vector<InputElementDescInfo> out;
	std::ifstream ifs(layoutPath);
	if (!ifs) return out;


	std::string s;
	while (std::getline(ifs, s)) {
		if (s.empty()) continue;
		std::istringstream iss(s);
		InputElementDescInfo e;
		std::string fmt;
		iss >> e.semanticName >> fmt >> e.semanticIndex >> e.inputSlot;
		int perInst = 0; iss >> perInst >> e.instanceDataStepRate;
		e.perInstance = (perInst != 0);
		e.format = StringToFormat(fmt);
		out.push_back(e);
	}
	return out;
}


std::string InputLayoutParser::FormatToString(DXGI_FORMAT f)
{
	switch (f) {
	case DXGI_FORMAT_R32_FLOAT: return "R32_FLOAT";
	case DXGI_FORMAT_R32G32_FLOAT: return "R32G32_FLOAT";
	case DXGI_FORMAT_R32G32B32_FLOAT: return "R32G32B32_FLOAT";
	case DXGI_FORMAT_R32G32B32A32_FLOAT: return "R32G32B32A32_FLOAT";
	case DXGI_FORMAT_R32_UINT: return "R32_UINT";
	case DXGI_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
	default: return "UNKNOWN";
	}
}


DXGI_FORMAT InputLayoutParser::StringToFormat(const std::string& s)
{
	if (s == "R32_FLOAT") return DXGI_FORMAT_R32_FLOAT;
	if (s == "R32G32_FLOAT") return DXGI_FORMAT_R32G32_FLOAT;
	if (s == "R32G32B32_FLOAT") return DXGI_FORMAT_R32G32B32_FLOAT;
	if (s == "R32G32B32A32_FLOAT") return DXGI_FORMAT_R32G32B32A32_FLOAT;
	if (s == "R32_UINT") return DXGI_FORMAT_R32_UINT;
	if (s == "R8G8B8A8_UNORM") return DXGI_FORMAT_R8G8B8A8_UNORM;
	return DXGI_FORMAT_UNKNOWN;
}