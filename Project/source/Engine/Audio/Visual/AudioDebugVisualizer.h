#pragma once
#include <vector>
#include <array>

namespace Audio {

struct DebugSourceVisualization {
	int id;
	std::array<float,3> position;
	float radius;
	float vu; // 0..1
};

class AudioDebugVisualizer {
public:
	AudioDebugVisualizer();
	~AudioDebugVisualizer();

	void SubmitSource(const DebugSourceVisualization& s) noexcept;
	void Flush(); // send to ModelManager / renderer on main thread

private:
	std::vector<DebugSourceVisualization> m_list;
};

} // namespace Audio
