#pragma once
#include <cstddef>

namespace Audio {

struct AudioBufferView {
	float* const* channels; // array of channel pointers (SoA)
	size_t frames;
	size_t channelCount;
};

class IAudioDSPNode {
public:
	virtual ~IAudioDSPNode() = default;
	virtual void Process(const AudioBufferView& in, AudioBufferView& out) noexcept = 0;
	// ApplyParameterBlock: thread-safe snapshot apply (audio thread will call)
	virtual void ApplyParameterBlock(const void* paramBlock, size_t size) noexcept = 0;
};

} // namespace Audio
