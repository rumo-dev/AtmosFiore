#include "VoicePool.h"
#include "../Platform/AudioPlatformStubs.h"

// Ensure target architecture macro for Windows headers in mixed environments
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <stdexcept>
#include <new>

namespace Audio {

VoiceWrapper::VoiceWrapper(IXAudio2SourceVoice* v) noexcept
	: m_voice(v), m_busy(false) {}

VoiceWrapper::~VoiceWrapper() {
	if (m_voice) {
		m_voice->DestroyVoice();
		m_voice = nullptr;
	}
}

IXAudio2SourceVoice* VoiceWrapper::Get() const noexcept { return m_voice; }

void VoiceWrapper::Stop() noexcept {
	if (m_voice) {
		m_voice->Stop(0);
		m_voice->FlushSourceBuffers();
	}
	m_busy.store(false, std::memory_order_release);
}

void VoiceWrapper::SubmitBuffer(const XAUDIO2_BUFFER& buf) noexcept {
	if (m_voice) {
		m_voice->SubmitSourceBuffer(&buf);
		m_busy.store(true, std::memory_order_release);
	}
}

bool VoiceWrapper::IsBusy() const noexcept {
	return m_busy.load(std::memory_order_acquire);
}

// -------------------- VoicePool --------------------

VoicePool::VoicePool(IXAudio2* xaudio, size_t maxVoices)
	: m_max(maxVoices), m_xaudio(xaudio) {
	m_pool.reserve(m_max);

	for (size_t i = 0; i < m_max; ++i) {
		IXAudio2SourceVoice* src = nullptr;
		// create a source voice with a simple default format (caller should reconfigure)
		WAVEFORMATEX wf = {};
		wf.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
		wf.nChannels = 2;
		wf.nSamplesPerSec = 48000;
		wf.wBitsPerSample = 32;
		wf.nBlockAlign = (wf.nChannels * wf.wBitsPerSample) / 8;
		wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

		HRESULT hr = xaudio->CreateSourceVoice(&src, &wf);
		if (FAILED(hr) || src == nullptr) {
			// free previously created
			for (auto& p : m_pool) p.reset();
			throw std::runtime_error("Failed to create source voice");
		}
		m_pool.emplace_back(std::make_unique<VoiceWrapper>(src));
	}
}

VoicePool::~VoicePool() {
	m_pool.clear();
}

VoiceWrapper* VoicePool::Acquire(const VoiceDescriptor& desc) noexcept {
	// simple strategy: find free voice
	VoiceWrapper* candidate = nullptr;
	for (auto& v : m_pool) {
		if (!v->IsBusy()) {
			candidate = v.get();
			break;
		}
	}
	if (candidate) return candidate;

	// steal lowest priority busy voice (simple linear search)
	int lowestPriority = INT32_MAX;
	size_t idx = SIZE_MAX;
	for (size_t i = 0; i < m_pool.size(); ++i) {
		// Here we don't have per-voice metadata; in production, track descriptor per voice
		// For now, just steal first busy voice
		if (m_pool[i]->IsBusy()) {
			idx = i;
			break;
		}
	}
	if (idx != SIZE_MAX) {
		m_pool[idx]->Stop();
		return m_pool[idx].get();
	}
	return nullptr;
}

void VoicePool::Release(VoiceWrapper* v) noexcept {
	if (v) v->Stop();
}

void VoicePool::UpdatePriorities() noexcept {
	// TODO: update internal priority bookkeeping
}

} // namespace Audio
