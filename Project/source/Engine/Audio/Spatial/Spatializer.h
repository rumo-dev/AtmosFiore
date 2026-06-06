#pragma once
#include "../Platform/AudioPlatformStubs.h"
#include <array>

namespace Audio {

class Spatializer {
public:
	Spatializer();
	~Spatializer();
	bool Initialize(uint32_t srcChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT, uint32_t dstChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);
	void UpdateListener(const X3DAUDIO_LISTENER& listener) noexcept;
	void Compute(const X3DAUDIO_EMITTER& emitter, std::array<float, 3>& outL, std::array<float, 3>& outR) noexcept;

private:
	X3DAUDIO_HANDLE m_handle;
	X3DAUDIO_DSP_SETTINGS m_dspSettings;
	bool m_initialized;
};

} // namespace Audio
