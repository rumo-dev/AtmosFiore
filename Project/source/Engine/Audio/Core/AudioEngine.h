#pragma once
#include "AudioCommand.h"
#include "VoicePool.h"
#include "../Graph/AudioGraph.h"
#include "../Streaming/StreamBuffer.h"
#include "../Spatial/Spatializer.h"
#include "../Platform/AudioPlatformStubs.h"
#include <memory>

namespace Audio {

class AudioEngine {
public:
	AudioEngine();
	~AudioEngine();

	bool Initialize();
	void Finalize();
	void Update(float dt); // game thread
	void AudioTick();      // audio thread callback

	// API
	int PlaySE(const char* id, const float position[3], int category = 0);
	int PlayBGM(const char* id);
	void Stop(int handle);
	void SetCategoryVolume(int category, float vol);

private:
	AudioCommandQueue m_cmdQueue;
	std::unique_ptr<VoicePool> m_voicePool;
	std::unique_ptr<AudioGraph> m_graph;
	std::unique_ptr<StreamRingBuffer> m_streamBuffer;
	Spatializer m_spatial;

	IXAudio2* m_xaudio;
	IXAudio2MasteringVoice* m_masterVoice;
};

} // namespace Audio
