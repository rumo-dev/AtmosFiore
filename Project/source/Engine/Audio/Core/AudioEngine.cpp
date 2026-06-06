#include "AudioEngine.h"
#include "../Platform/AudioPlatformStubs.h"
#include <stdexcept>

// Link against XAudio2 library when building with the real SDK
#pragma comment(lib, "xaudio2.lib")

namespace Audio {

AudioEngine::AudioEngine()
	: m_cmdQueue(2048)
	, m_voicePool(nullptr)
	, m_graph(nullptr)
	, m_streamBuffer(nullptr)
	, m_xaudio(nullptr)
	, m_masterVoice(nullptr)
{
}

AudioEngine::~AudioEngine() {
	Finalize();
}

bool AudioEngine::Initialize() {
	HRESULT hr = XAudio2Create(&m_xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(hr) || !m_xaudio) return false;
	hr = m_xaudio->CreateMasteringVoice(&m_masterVoice);
	if (FAILED(hr) || !m_masterVoice) return false;

	try {
		m_voicePool = std::make_unique<VoicePool>(m_xaudio, 64);
		m_graph = std::make_unique<AudioGraph>();
		m_streamBuffer = std::make_unique<StreamRingBuffer>(48000 * 5, 2); // 5 sec buffer
	} catch (...) {
		return false;
	}
	m_spatial.Initialize();
	return true;
}

void AudioEngine::Finalize() {
	m_streamBuffer.reset();
	m_graph.reset();
	m_voicePool.reset();
	if (m_masterVoice) { m_masterVoice->DestroyVoice(); m_masterVoice = nullptr; }
	if (m_xaudio) { m_xaudio->Release(); m_xaudio = nullptr; }
}

void AudioEngine::Update(float dt) {
	// process commands queued by game thread
	AudioCommand cmd;
	while (m_cmdQueue.Pop(cmd)) {
		if (cmd.fn) cmd.fn(cmd.userData);
	}
}

void AudioEngine::AudioTick() {
	// called from audio thread: mix graph into master buffers and submit to XAudio2
}

int AudioEngine::PlaySE(const char* id, const float position[3], int category) {
	// enqueue command to create voice and submit buffers
	return -1;
}

int AudioEngine::PlayBGM(const char* id) {
	return -1;
}

void AudioEngine::Stop(int handle) {
}

void AudioEngine::SetCategoryVolume(int category, float vol) {
}

} // namespace Audio
