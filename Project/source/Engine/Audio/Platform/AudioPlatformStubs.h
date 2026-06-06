#pragma once
// Minimal platform stubs to allow building in environments without Windows SDK/XAudio2/X3DAudio.
// In the real project, remove or conditionally compile this and use system headers instead.

#include <cstdint>
#include <cstddef>

using HRESULT = long;
#define S_OK 0
#define FAILED(hr) ((hr) != S_OK)

#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif

struct WAVEFORMATEX {
	uint16_t wFormatTag;
	uint16_t nChannels;
	uint32_t nSamplesPerSec;
	uint32_t nAvgBytesPerSec;
	uint16_t nBlockAlign;
	uint16_t wBitsPerSample;
	uint16_t cbSize;
};

struct XAUDIO2_BUFFER { uint32_t Flags; uint32_t AudioBytes; const uint8_t* pAudioData; };

struct IXAudio2SourceVoice {
	virtual void DestroyVoice() {}
	virtual void Stop(uint32_t) {}
	virtual void FlushSourceBuffers() {}
	virtual void SubmitSourceBuffer(const XAUDIO2_BUFFER*) {}
	virtual ~IXAudio2SourceVoice() {}
};

struct IXAudio2MasteringVoice {
	virtual void DestroyVoice() {}
	virtual ~IXAudio2MasteringVoice() {}
};

struct IXAudio2 {
	virtual HRESULT CreateSourceVoice(IXAudio2SourceVoice** pSourceVoice, const WAVEFORMATEX* pwfx) { *pSourceVoice = new IXAudio2SourceVoice(); return S_OK; }
	virtual HRESULT CreateMasteringVoice(IXAudio2MasteringVoice** pMasteringVoice) { *pMasteringVoice = new IXAudio2MasteringVoice(); return S_OK; }
	virtual void Release() {}
	virtual ~IXAudio2() {}
};

inline HRESULT XAudio2Create(IXAudio2** pp, unsigned int, int) { *pp = new IXAudio2(); return S_OK; }

#ifndef XAUDIO2_DEFAULT_PROCESSOR
#define XAUDIO2_DEFAULT_PROCESSOR 0
#endif

// x3daudio minimal stubs
using X3DAUDIO_HANDLE = void*;
struct X3DAUDIO_LISTENER { uint32_t Dummy; };
struct X3DAUDIO_EMITTER { uint32_t Dummy; };
struct X3DAUDIO_DSP_SETTINGS { uint32_t Dummy; };
inline int X3DAudioInitialize(uint32_t, uint32_t, X3DAUDIO_HANDLE&) { return 0; }

// Speaker constants
constexpr uint32_t SPEAKER_FRONT_LEFT = 0x1;
constexpr uint32_t SPEAKER_FRONT_RIGHT = 0x2;
