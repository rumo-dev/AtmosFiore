#include "Spatializer.h"
#include "../Platform/AudioPlatformStubs.h"

// Ensure target architecture macro for Windows headers in mixed environments
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

namespace Audio {

Spatializer::Spatializer()
	: m_handle(nullptr), m_dspSettings(), m_initialized(false) {
}

Spatializer::~Spatializer() {}

bool Spatializer::Initialize(uint32_t srcChannelMask, uint32_t dstChannelMask) {
	X3DAudioInitialize(srcChannelMask, dstChannelMask, m_handle);
	m_initialized = true;
	return true;
}

void Spatializer::UpdateListener(const X3DAUDIO_LISTENER& listener) noexcept {
	if (!m_initialized) return;
}

void Spatializer::Compute(const X3DAUDIO_EMITTER& emitter, std::array<float, 3>& outL, std::array<float, 3>& outR) noexcept {
	if (!m_initialized) return;
	outL = {1.f, 0.f, 0.f};
	outR = {0.f, 1.f, 0.f};
}

} // namespace Audio
