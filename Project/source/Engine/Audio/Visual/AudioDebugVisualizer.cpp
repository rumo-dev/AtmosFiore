#include "AudioDebugVisualizer.h"
#include <mutex>

namespace Audio {

AudioDebugVisualizer::AudioDebugVisualizer() {}
AudioDebugVisualizer::~AudioDebugVisualizer() {}

void AudioDebugVisualizer::SubmitSource(const DebugSourceVisualization& s) noexcept {
	// called from audio thread -> push into vector. In production use lock-free ring or double buffer.
	m_list.push_back(s);
}

void AudioDebugVisualizer::Flush() {
	// called from main thread: draw and clear
	for (const auto& s : m_list) {
		// ModelManager API calls would be here
		(void)s;
	}
	m_list.clear();
}

} // namespace Audio
