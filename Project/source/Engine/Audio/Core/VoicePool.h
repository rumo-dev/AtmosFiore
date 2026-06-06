#pragma once
#include <vector>
#include <memory>
#include <atomic>
#include <cstddef>

// Forward-declare XAudio2 types to avoid pulling Windows headers into every translation unit
struct IXAudio2SourceVoice;
struct IXAudio2;
struct XAUDIO2_BUFFER;

namespace Audio {

struct VoiceDescriptor {
	int priority;
	float lastDistance;
	bool spatial;
};

class VoiceWrapper {
public:
	explicit VoiceWrapper(IXAudio2SourceVoice* v) noexcept;
	~VoiceWrapper();
	IXAudio2SourceVoice* Get() const noexcept;
	void Stop() noexcept;
	void SubmitBuffer(const XAUDIO2_BUFFER& buf) noexcept;
	bool IsBusy() const noexcept;

private:
	IXAudio2SourceVoice* m_voice;
	std::atomic<bool> m_busy;
};

class VoicePool {
public:
	VoicePool(IXAudio2* xaudio, size_t maxVoices);
	~VoicePool();

	VoiceWrapper* Acquire(const VoiceDescriptor& desc) noexcept;
	void Release(VoiceWrapper* v) noexcept;
	void UpdatePriorities() noexcept;

private:
	std::vector<std::unique_ptr<VoiceWrapper>> m_pool;
	size_t m_max;
	IXAudio2* m_xaudio;
};

} // namespace Audio
