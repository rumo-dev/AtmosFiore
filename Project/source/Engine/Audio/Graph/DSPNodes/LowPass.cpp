#include "LowPass.h"
#include <algorithm>
#include <cstring>

namespace Audio {

LowPassNode::LowPassNode()
	: m_params{20000.0f, 0.707f}, m_pending{}, m_hasPending(false) {
}

LowPassNode::~LowPassNode() = default;

void LowPassNode::Process(const AudioBufferView& in, AudioBufferView& out) noexcept {
	// naive passthrough implementation for skeleton
	size_t ch = std::min(in.channelCount, out.channelCount);
	size_t samples = in.frames;
	for (size_t c = 0; c < ch; ++c) {
		float* src = in.channels[c];
		float* dst = out.channels[c];
		std::memcpy(dst, src, sizeof(float) * samples);
	}
	if (m_hasPending.exchange(false)) {
		m_params = m_pending;
	}
}

void LowPassNode::ApplyParameterBlock(const void* paramBlock, size_t size) noexcept {
	if (paramBlock == nullptr || size < sizeof(Params)) return;
	std::memcpy(&m_pending, paramBlock, sizeof(Params));
	m_hasPending.store(true);
}

} // namespace Audio
