#pragma once
#include "../../Core/IAudioDSPNode.h"
#include <atomic>

namespace Audio {

class LowPassNode : public IAudioDSPNode {
public:
	LowPassNode();
	~LowPassNode() override;

	void Process(const AudioBufferView& in, AudioBufferView& out) noexcept override;
	void ApplyParameterBlock(const void* paramBlock, size_t size) noexcept override;

	struct Params {
		float cutoff; // Hz
		float resonance;
	};

private:
	Params m_params;
	Params m_pending;
	std::atomic<bool> m_hasPending;
	// per-channel state would be here
};

} // namespace Audio
