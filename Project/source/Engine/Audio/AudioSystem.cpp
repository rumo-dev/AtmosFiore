//=============================================================================
//  AudioSystem.cpp
//  DirectX11 / XAudio2 / X3DAudio 音響システム 実装
//=============================================================================

#include "AudioSystem.h"
#include <mmreg.h>          // WAVEFORMATEX
#include <mmsystem.h>       // FOURCC
#include <stdexcept>
#include <cassert>
#include <algorithm>
#include <cmath>
#include <sstream>

#pragma comment(lib, "xaudio2.lib")

//-----------------------------------------------------------------------------
//  ログ
//-----------------------------------------------------------------------------
namespace
{
	void AudioLog(const std::string& msg)
	{
		OutputDebugStringA(("[AudioSystem] " + msg + "\n").c_str());
	}

	// ピッチ(半音)→ 周波数比
	float SemitonesToRatio(float semitones)
	{
		return std::pow(2.0f, semitones / 12.0f);
	}
}

//=============================================================================
//  Initialize / Shutdown
//=============================================================================
bool AudioSystem::Initialize(const std::string& jsonPath,
	const std::string& baseDir,
	UINT32             maxVoices)
{
	m_baseDir = baseDir;

	//--- XAudio2 生成 ---
	HRESULT hr = XAudio2Create(m_pXAudio2.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(hr)) { AudioLog("XAudio2Create failed"); return false; }

#ifdef _DEBUG
	XAUDIO2_DEBUG_CONFIGURATION dbgCfg = {};
	dbgCfg.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
	dbgCfg.BreakMask = XAUDIO2_LOG_ERRORS;
	m_pXAudio2->SetDebugConfiguration(&dbgCfg);
#endif

	//--- マスタリングボイス生成 ---
	hr = m_pXAudio2->CreateMasteringVoice(&m_pMasterVoice);
	if (FAILED(hr)) { AudioLog("CreateMasteringVoice failed"); return false; }

	//--- チャンネル情報取得 (X3DAudio 用) ---
	XAUDIO2_VOICE_DETAILS details;
	m_pMasterVoice->GetVoiceDetails(&details);
	m_outputChannels = details.InputChannels;
	m_pMasterVoice->GetChannelMask(&m_channelMask);

	//--- X3DAudio 初期化 ---
	X3DAudioInitialize(m_channelMask, X3DAUDIO_SPEED_OF_SOUND, m_x3dInstance);

	//--- リスナー初期化 ---
	ZeroMemory(&m_listener, sizeof(m_listener));
	m_listener.OrientFront = { 0.0f, 0.0f, 1.0f };
	m_listener.OrientTop = { 0.0f, 1.0f, 0.0f };
	m_listener.Position = { 0.0f, 0.0f, 0.0f };
	m_listener.Velocity = { 0.0f, 0.0f, 0.0f };

	//--- サブミックスボイス生成 (BGM / SE 分離) ---
	WAVEFORMATEX wfx = {};
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = (WORD)m_outputChannels;
	wfx.nSamplesPerSec = 44100;
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	hr = m_pXAudio2->CreateSubmixVoice(&m_pBGMSubmix, m_outputChannels, 44100);
	if (FAILED(hr)) { AudioLog("BGM SubmixVoice failed"); return false; }

	hr = m_pXAudio2->CreateSubmixVoice(&m_pSESubmix, m_outputChannels, 44100);
	if (FAILED(hr)) { AudioLog("SE SubmixVoice failed"); return false; }

	//--- JSON 読み込み ---
	m_json.SetAutoSave(true);
	if (!jsonPath.empty())
	{
		m_json.Load(jsonPath);
		LoadClipRegistryFromJson();
		LoadEffectChainsFromJson();
	}

	//--- imnodes 初期化 ---
	ImNodes::CreateContext();
	ImNodes::StyleColorsDark();

	AudioLog("Initialize OK  baseDir=" + m_baseDir);
	return true;
}

void AudioSystem::Shutdown()
{
	StopAll();

	// ボイス破棄
	{
		std::lock_guard<std::mutex> lk(m_voiceMutex);
		for (auto& [id, v] : m_voices)
		{
			if (v.pSourceVoice)
			{
				v.pSourceVoice->Stop();
				v.pSourceVoice->DestroyVoice();
			}
		}
		m_voices.clear();
	}

	if (m_pBGMSubmix) { m_pBGMSubmix->DestroyVoice(); m_pBGMSubmix = nullptr; }
	if (m_pSESubmix) { m_pSESubmix->DestroyVoice();  m_pSESubmix = nullptr; }
	if (m_pMasterVoice) { m_pMasterVoice->DestroyVoice(); m_pMasterVoice = nullptr; }

	ImNodes::DestroyContext();

	AudioLog("Shutdown OK");
}

//=============================================================================
//  Update
//=============================================================================
void AudioSystem::Update()
{
	CollectStoppedVoices();

	// 3D 音源を持つボイスの DSP を更新
	std::lock_guard<std::mutex> lk(m_voiceMutex);
	for (auto& [id, v] : m_voices)
	{
		if (v.dspSettings.pMatrixCoefficients)
			Update3DAudio(id, v);
	}
}

//=============================================================================
//  音源登録
//=============================================================================
bool AudioSystem::RegisterClip(const std::string& key,
	const std::string& relativePath,
	float              defaultVolume,
	float              defaultPitch,
	const std::string& effectChain)
{
	AudioClipInfo info;
	info.key = key;
	info.relativePath = relativePath;
	info.defaultVolume = defaultVolume;
	info.defaultPitch = defaultPitch;
	info.effectChain = effectChain;

	m_clipRegistry[key] = info;

	// JSON へ反映
	m_json.Set("clips." + key, info.ToJson());

	AudioLog("RegisterClip: " + key + " -> " + relativePath);
	return true;
}

void AudioSystem::UnregisterClip(const std::string& key)
{
	m_clipRegistry.erase(key);
	m_bufferCache.erase(key);
	m_json.Delete("clips." + key);
}

bool AudioSystem::SaveClipRegistry() const
{
	return m_json.Save();
}

//=============================================================================
//  再生
//=============================================================================
int AudioSystem::PlayBGM(const std::string& key, const AudioPlayParam& param)
{
	StopAllBGM();
	return PlayInternal(key, param, true);
}

int AudioSystem::PlaySE(const std::string& key, const AudioPlayParam& param)
{
	return PlayInternal(key, param, false);
}

int AudioSystem::PlayInternal(const std::string& key,
	const AudioPlayParam& param,
	bool isBGM)
{
	// バッファ取得 (キャッシュ or ロード)
	const AudioBuffer* pBuf = GetOrLoadBuffer(key);
	if (!pBuf)
	{
		AudioLog("PlayInternal: バッファ取得失敗 key=" + key);
		return -1;
	}

	// エフェクトチェーン解決
	const AudioEffectChain* pChain = nullptr;
	{
		std::string chainName = param.overrideEffectChain;
		if (chainName.empty())
		{
			auto it = m_clipRegistry.find(key);
			if (it != m_clipRegistry.end())
				chainName = it->second.effectChain;
		}
		if (!chainName.empty())
			pChain = GetEffectChain(chainName);
	}

	// SourceVoice 生成
	IXAudio2SourceVoice* pVoice = CreateSourceVoice(*pBuf, pChain, isBGM);
	if (!pVoice) return -1;

	// デフォルトパラメータを適用
	{
		auto it = m_clipRegistry.find(key);
		if (it != m_clipRegistry.end())
		{
			float vol = it->second.defaultVolume * param.volume
				* (isBGM ? m_bgmMasterVolume : m_seMasterVolume);
			float pitch = it->second.defaultPitch * param.pitch;
			pVoice->SetVolume(vol);
			pVoice->SetFrequencyRatio(pitch);
		}
	}

	// XAUDIO2_BUFFER の設定
	XAUDIO2_BUFFER xbuf = {};
	xbuf.AudioBytes = static_cast<UINT32>(pBuf->data.size());
	xbuf.pAudioData = pBuf->data.data();
	xbuf.Flags = XAUDIO2_END_OF_STREAM;
	if (param.loop || (isBGM))
	{
		xbuf.LoopCount = XAUDIO2_LOOP_INFINITE;
	}

	pVoice->SubmitSourceBuffer(&xbuf);
	pVoice->Start(0);

	// ボイス登録
	int vid = m_nextVoiceId++;
	PlayingVoice pv;
	pv.pSourceVoice = pVoice;
	pv.clipKey = key;
	pv.isBGM = isBGM;
	pv.looping = param.loop || isBGM;
	// 3D 設定
	if (param.spatialize)
	{
		ZeroMemory(&pv.emitter, sizeof(pv.emitter));
		pv.emitter.Position = { param.posX, param.posY, param.posZ };
		pv.emitter.Velocity = { 0,0,0 };
		pv.emitter.OrientFront = { 0,0,1 };
		pv.emitter.OrientTop = { 0,1,0 };

		pv.emitter.ChannelCount = pBuf->wfx.nChannels;
		pv.emitter.CurveDistanceScaler = 1.0f;
		pv.emitter.InnerRadius = 5.0f;
		pv.emitter.InnerRadiusAngle = X3DAUDIO_PI / 4.0f;

		pv.dspSettings.SrcChannelCount = pBuf->wfx.nChannels;
		pv.dspSettings.DstChannelCount = m_outputChannels;

		// 行列バッファの確保
		UINT32 matSize = pv.dspSettings.SrcChannelCount * pv.dspSettings.DstChannelCount;
		pv.matrixCoef.resize(matSize, 0.0f);

		// ★【追加】マルチチャンネル（ステレオ等）の場合の方位角設定
		if (pv.emitter.ChannelCount > 1)
		{
			pv.channelAzimuths.resize(pv.emitter.ChannelCount, 0.0f);
			if (pv.emitter.ChannelCount == 2)
			{
				// ステレオの一般的なスピーカー配置 (左前45度、右前45度)
				pv.channelAzimuths[0] = -X3DAUDIO_PI / 4.0f; // Left
				pv.channelAzimuths[1] = X3DAUDIO_PI / 4.0f; // Right
			}
		}

		// この後 pv がコンテナにコピーされるため、ここでセットしたポインタは一度無効になりますが、
		// Update3DAudio 内で毎回最新のアドレスに更新されるため問題なくなります。
		pv.dspSettings.pMatrixCoefficients = pv.matrixCoef.data();
		pv.emitter.pChannelAzimuths = pv.emitter.ChannelCount > 1 ? pv.channelAzimuths.data() : nullptr;

		Update3DAudio(vid, pv);
	}

	{
		std::lock_guard<std::mutex> lk(m_voiceMutex);
		m_voices[vid] = std::move(pv);
	}

	AudioLog("Play " + std::string(isBGM ? "BGM" : "SE") + " key=" + key + " id=" + std::to_string(vid));
	return vid;
}

//=============================================================================
//  停止
//=============================================================================
void AudioSystem::Stop(int voiceId, float /*fadeOutSec*/)
{
	std::lock_guard<std::mutex> lk(m_voiceMutex);
	auto it = m_voices.find(voiceId);
	if (it == m_voices.end()) return;

	it->second.pSourceVoice->Stop();
	it->second.pSourceVoice->FlushSourceBuffers();
	it->second.pSourceVoice->DestroyVoice();
	it->second.pSourceVoice = nullptr;
	m_voices.erase(it);
}

void AudioSystem::StopAllBGM(float fadeOutSec)
{
	std::lock_guard<std::mutex> lk(m_voiceMutex);
	for (auto it = m_voices.begin(); it != m_voices.end(); )
	{
		if (it->second.isBGM)
		{
			it->second.pSourceVoice->Stop();
			it->second.pSourceVoice->DestroyVoice();
			it = m_voices.erase(it);
		}
		else ++it;
	}
}

void AudioSystem::StopAllSE()
{
	std::lock_guard<std::mutex> lk(m_voiceMutex);
	for (auto it = m_voices.begin(); it != m_voices.end(); )
	{
		if (!it->second.isBGM)
		{
			it->second.pSourceVoice->Stop();
			it->second.pSourceVoice->DestroyVoice();
			it = m_voices.erase(it);
		}
		else ++it;
	}
}

void AudioSystem::StopAll()
{
	StopAllBGM();
	StopAllSE();
}

//=============================================================================
//  ボイス操作
//=============================================================================
void AudioSystem::SetVolume(int voiceId, float volume)
{
	std::lock_guard<std::mutex> lk(m_voiceMutex);
	auto it = m_voices.find(voiceId);
	if (it != m_voices.end())
		it->second.pSourceVoice->SetVolume(volume);
}

void AudioSystem::SetPitch(int voiceId, float pitch)
{
	std::lock_guard<std::mutex> lk(m_voiceMutex);
	auto it = m_voices.find(voiceId);
	if (it != m_voices.end())
		it->second.pSourceVoice->SetFrequencyRatio(pitch);
}

void AudioSystem::SetPosition(int voiceId, float x, float y, float z)
{
	std::lock_guard<std::mutex> lk(m_voiceMutex);
	auto it = m_voices.find(voiceId);
	if (it == m_voices.end()) return;
	it->second.emitter.Position = { x, y, z };
	Update3DAudio(voiceId, it->second);
}

void AudioSystem::SetBGMMasterVolume(float v)
{
	m_bgmMasterVolume = v;
	if (m_pBGMSubmix) m_pBGMSubmix->SetVolume(v);
}

void AudioSystem::SetSEMasterVolume(float v)
{
	m_seMasterVolume = v;
	if (m_pSESubmix) m_pSESubmix->SetVolume(v);
}

bool AudioSystem::IsPlaying(int voiceId) const
{
	std::lock_guard<std::mutex> lk(m_voiceMutex);
	auto it = m_voices.find(voiceId);
	if (it == m_voices.end()) return false;

	XAUDIO2_VOICE_STATE state;
	it->second.pSourceVoice->GetState(&state);
	return state.BuffersQueued > 0;
}

//=============================================================================
//  3D リスナー
//=============================================================================
void AudioSystem::SetListenerPosition(float x, float y, float z)
{
	m_listener.Position = { x, y, z };
}
void AudioSystem::SetListenerOrientation(float fx, float fy, float fz,
	float ux, float uy, float uz)
{
	m_listener.OrientFront = { fx, fy, fz };
	m_listener.OrientTop = { ux, uy, uz };
}
void AudioSystem::SetListenerVelocity(float vx, float vy, float vz)
{
	m_listener.Velocity = { vx, vy, vz };
}

//=============================================================================
//  エフェクトチェーン
//=============================================================================
void AudioSystem::RegisterEffectChain(const AudioEffectChain& chain)
{
	m_effectChains[chain.name] = chain;
	m_json.Set("effectChains." + chain.name, chain.ToJson());
	AudioLog("RegisterEffectChain: " + chain.name);
}

const AudioEffectChain* AudioSystem::GetEffectChain(const std::string& name) const
{
	auto it = m_effectChains.find(name);
	return (it != m_effectChains.end()) ? &it->second : nullptr;
}

std::vector<std::string> AudioSystem::GetEffectChainNames() const
{
	std::vector<std::string> names;
	for (auto& [n, _] : m_effectChains) names.push_back(n);
	return names;
}

bool AudioSystem::SaveEffectChains() const
{
	return m_json.Save();
}

bool AudioSystem::ReloadJson()
{
	if (!m_json.Load()) return false;
	m_clipRegistry.clear();
	m_effectChains.clear();
	LoadClipRegistryFromJson();
	LoadEffectChainsFromJson();
	return true;
}

//=============================================================================
//  imnodes GUI
//=============================================================================
void AudioSystem::DrawEffectGraphEditor(const std::string& chainName)
{


	// チェーン選択コンボ
	auto chainNames = GetEffectChainNames();
	if (!chainName.empty()) m_editingChainName = chainName;

	if (ImGui::BeginCombo("Chain", m_editingChainName.c_str()))
	{
		for (auto& n : chainNames)
		{
			bool selected = (n == m_editingChainName);
			if (ImGui::Selectable(n.c_str(), selected))
				m_editingChainName = n;
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();

	// 新規チェーン作成
	static char newChainBuf[64] = "";
	ImGui::SetNextItemWidth(150);
	ImGui::InputText("##newchain", newChainBuf, sizeof(newChainBuf));
	ImGui::SameLine();
	if (ImGui::Button("New Chain") && newChainBuf[0] != '\0')
	{
		AudioEffectChain newChain;
		newChain.name = newChainBuf;
		RegisterEffectChain(newChain);
		m_editingChainName = newChainBuf;
		newChainBuf[0] = '\0';
	}

	ImGui::SameLine();
	if (ImGui::Button("Save JSON"))
		SaveEffectChains();

	ImGui::SameLine();
	if (ImGui::Button("Reload JSON"))
		ReloadJson();

	// 編集対象チェーンを取得
	AudioEffectChain* pChain = nullptr;
	if (!m_editingChainName.empty())
	{
		auto it = m_effectChains.find(m_editingChainName);
		if (it != m_effectChains.end()) pChain = &it->second;
	}

	// ノード追加メニュー
	if (pChain)
	{
		ImGui::Separator();
		ImGui::Text("Add Node:");
		ImGui::SameLine();

		const char* effectNames[] = {
			"Volume","Pitch","LowPassFilter","HighPassFilter",
			"Reverb","Echo","Equalizer","Spatialize"
		};
		static int selectedEffect = 0;
		ImGui::SetNextItemWidth(140);
		ImGui::Combo("##addtype", &selectedEffect, effectNames, IM_ARRAYSIZE(effectNames));
		ImGui::SameLine();

		if (ImGui::Button("Add"))
		{
			AudioEffectNode node;
			node.nodeId = NewNodeId();

			AudioEffectType types[] = {
				AudioEffectType::Volume, AudioEffectType::Pitch,
				AudioEffectType::LowPassFilter, AudioEffectType::HighPassFilter,
				AudioEffectType::Reverb, AudioEffectType::Echo,
				AudioEffectType::Equalizer, AudioEffectType::Spatialize
			};
			node.type = types[selectedEffect];

			// デフォルトパラメータ設定
			switch (node.type)
			{
			case AudioEffectType::Volume:         node.param = VolumeParam{};          break;
			case AudioEffectType::Pitch:          node.param = PitchParam{};           break;
			case AudioEffectType::LowPassFilter:  node.param = LowPassFilterParam{};   break;
			case AudioEffectType::HighPassFilter: node.param = HighPassFilterParam{};  break;
			case AudioEffectType::Reverb:         node.param = ReverbParam{};          break;
			case AudioEffectType::Echo:           node.param = EchoParam{};            break;
			case AudioEffectType::Equalizer:      node.param = EqualizerParam{};       break;
			case AudioEffectType::Spatialize:     node.param = SpatializeParam{};      break;
			default: break;
			}

			pChain->nodes.push_back(node);
			m_json.Set("effectChains." + pChain->name, pChain->ToJson());
		}

		ImGui::Separator();
	}

	// imnodes エディタ
	ImNodes::BeginNodeEditor();

	if (pChain)
	{
		// ノード描画
		for (auto& node : pChain->nodes)
		{
			ImNodes::BeginNode(node.nodeId);

			ImNodes::BeginNodeTitleBar();
			ImGui::Text("[%d] %s", node.nodeId, AudioEffectTypeName(node.type));
			ImGui::SameLine();
			ImGui::Checkbox("##en", &node.enabled);
			ImNodes::EndNodeTitleBar();

			// 入力ピン
			ImNodes::BeginInputAttribute(node.nodeId * 10 + 0);
			ImGui::Text("In");
			ImNodes::EndInputAttribute();

			// パラメータ編集
			DrawNodeProperties(node);

			// 出力ピン
			ImNodes::BeginOutputAttribute(node.nodeId * 10 + 1);
			ImGui::Text("Out");
			ImNodes::EndOutputAttribute();

			ImNodes::EndNode();
		}

		// リンク描画
		for (auto& link : pChain->links)
		{
			ImNodes::Link(link.linkId,
				link.fromNode * 10 + 1,   // from出力ピン
				link.toNode * 10 + 0);  // to入力ピン
		}
	}

	ImNodes::EndNodeEditor();

	// リンク生成
	if (pChain)
	{
		int fromAttr, toAttr;
		if (ImNodes::IsLinkCreated(&fromAttr, &toAttr))
		{
			AudioEffectLink link;
			link.linkId = NewNodeId();
			link.fromNode = fromAttr / 10;
			link.toNode = toAttr / 10;
			pChain->links.push_back(link);
			m_json.Set("effectChains." + pChain->name, pChain->ToJson());
		}

		// リンク削除
		int destroyedLink;
		if (ImNodes::IsLinkDestroyed(&destroyedLink))
		{
			auto& links = pChain->links;
			links.erase(std::remove_if(links.begin(), links.end(),
				[&](const AudioEffectLink& l) { return l.linkId == destroyedLink; }),
				links.end());
			m_json.Set("effectChains." + pChain->name, pChain->ToJson());
		}
	}

}

//-----------------------------------------------------------------------------
//  ノードプロパティ描画
//-----------------------------------------------------------------------------
void AudioSystem::DrawNodeProperties(AudioEffectNode& node)
{
	bool changed = false;

	std::visit([&](auto&& p)
		{
			using T = std::decay_t<decltype(p)>;

			if constexpr (std::is_same_v<T, VolumeParam>)
			{
				changed |= ImGui::SliderFloat("Volume##v", &p.volume, 0.0f, 2.0f);
			}
			else if constexpr (std::is_same_v<T, PitchParam>)
			{
				changed |= ImGui::SliderFloat("Semitones##p", &p.semitones, -24.0f, 24.0f);
			}
			else if constexpr (std::is_same_v<T, LowPassFilterParam>)
			{
				changed |= ImGui::SliderFloat("Freq##lpf", &p.frequency, 20.0f, 22050.0f);
				changed |= ImGui::SliderFloat("1/Q##lpf", &p.oneOverQ, 0.1f, 10.0f);
			}
			else if constexpr (std::is_same_v<T, HighPassFilterParam>)
			{
				changed |= ImGui::SliderFloat("Freq##hpf", &p.frequency, 20.0f, 22050.0f);
				changed |= ImGui::SliderFloat("1/Q##hpf", &p.oneOverQ, 0.1f, 10.0f);
			}
			else if constexpr (std::is_same_v<T, ReverbParam>)
			{
				changed |= ImGui::SliderFloat("Wet/Dry##rv", &p.wetDryMix, 0.0f, 100.0f);
				changed |= ImGui::SliderFloat("Room##rv", &p.roomSize, 0.0f, 1.0f);
				changed |= ImGui::SliderFloat("Decay##rv", &p.decayTime, 0.1f, 20.0f);
				changed |= ImGui::SliderFloat("Early##rv", &p.earlyDelay, 0.0f, 300.0f);
				changed |= ImGui::SliderFloat("Late##rv", &p.lateDelay, 0.0f, 300.0f);
				changed |= ImGui::SliderFloat("Density##rv", &p.density, 0.0f, 100.0f);
			}
			else if constexpr (std::is_same_v<T, EchoParam>)
			{
				changed |= ImGui::SliderFloat("Wet/Dry##ec", &p.wetDryMix, 0.0f, 100.0f);
				changed |= ImGui::SliderFloat("Feedback##ec", &p.feedback, 0.0f, 1.0f);
				changed |= ImGui::SliderFloat("Delay##ec", &p.delay, 1.0f, 2000.0f);
			}
			else if constexpr (std::is_same_v<T, EqualizerParam>)
			{
				ImGui::Text("Band 0");
				changed |= ImGui::SliderFloat("G##eq0", &p.gain0, 0.126f, 7.94f);
				changed |= ImGui::SliderFloat("F##eq0", &p.freq0, 20.0f, 20000.0f);
				ImGui::Text("Band 1");
				changed |= ImGui::SliderFloat("G##eq1", &p.gain1, 0.126f, 7.94f);
				changed |= ImGui::SliderFloat("F##eq1", &p.freq1, 20.0f, 20000.0f);
				ImGui::Text("Band 2");
				changed |= ImGui::SliderFloat("G##eq2", &p.gain2, 0.126f, 7.94f);
				changed |= ImGui::SliderFloat("F##eq2", &p.freq2, 20.0f, 20000.0f);
				ImGui::Text("Band 3");
				changed |= ImGui::SliderFloat("G##eq3", &p.gain3, 0.126f, 7.94f);
				changed |= ImGui::SliderFloat("F##eq3", &p.freq3, 20.0f, 20000.0f);
			}
			else if constexpr (std::is_same_v<T, SpatializeParam>)
			{
				changed |= ImGui::DragFloat3("Pos##sp", &p.posX, 0.1f);
				changed |= ImGui::SliderFloat("InnerR##sp", &p.innerRadius, 0.0f, 50.0f);
				changed |= ImGui::SliderFloat("OuterR##sp", &p.outerRadius, 1.0f, 500.0f);
				changed |= ImGui::SliderFloat("MaxDist##sp", &p.maxDistance, 1.0f, 1000.0f);
			}

			if (changed)
				node.param = p;

		}, node.param);

	// JSON 自動保存
	if (changed && !m_editingChainName.empty())
	{
		auto it = m_effectChains.find(m_editingChainName);
		if (it != m_effectChains.end())
			m_json.Set("effectChains." + m_editingChainName, it->second.ToJson());
	}
}

//=============================================================================
//  内部実装
//=============================================================================

//-----------------------------------------------------------------------------
//  WAV 読み込み (RIFF / PCM 対応)
//-----------------------------------------------------------------------------
bool AudioSystem::LoadWavFile(const std::string& fullPath, AudioBuffer& outBuf)
{
	std::ifstream ifs(fullPath, std::ios::binary);
	if (!ifs.is_open())
	{
		AudioLog("LoadWavFile: 開けません: " + fullPath);
		return false;
	}

	auto readU32 = [&]() {
		UINT32 v; ifs.read(reinterpret_cast<char*>(&v), 4); return v;
		};
	auto readU16 = [&]() {
		UINT16 v; ifs.read(reinterpret_cast<char*>(&v), 2); return v;
		};

	// RIFF ヘッダー
	UINT32 riff = readU32();
	if (riff != 0x46464952) { AudioLog("LoadWavFile: RIFF 不正"); return false; }

	readU32(); // ファイルサイズ

	UINT32 wave = readU32();
	if (wave != 0x45564157) { AudioLog("LoadWavFile: WAVE 不正"); return false; }

	bool fmtRead = false;
	bool dataRead = false;

	while (ifs && !dataRead)
	{
		UINT32 chunkId = readU32();
		UINT32 chunkSize = readU32();

		if (chunkId == 0x20746D66) // "fmt "
		{
			UINT16 audioFmt = readU16();
			UINT16 channels = readU16();
			UINT32 sampleRate = readU32();
			UINT32 byteRate = readU32();
			UINT16 blockAlign = readU16();
			UINT16 bitsPerSample = readU16();

			if (chunkSize > 16)
				ifs.seekg(chunkSize - 16, std::ios::cur);

			outBuf.wfx.wFormatTag = WAVE_FORMAT_PCM;
			outBuf.wfx.nChannels = channels;
			outBuf.wfx.nSamplesPerSec = sampleRate;
			outBuf.wfx.nAvgBytesPerSec = byteRate;
			outBuf.wfx.nBlockAlign = blockAlign;
			outBuf.wfx.wBitsPerSample = bitsPerSample;
			outBuf.wfx.cbSize = 0;
			fmtRead = true;
		}
		else if (chunkId == 0x61746164 && fmtRead) // "data"
		{
			outBuf.data.resize(chunkSize);
			ifs.read(reinterpret_cast<char*>(outBuf.data.data()), chunkSize);
			dataRead = true;
		}
		else
		{
			ifs.seekg(chunkSize, std::ios::cur);
		}
	}

	if (!dataRead) { AudioLog("LoadWavFile: data チャンク未検出: " + fullPath); return false; }

	AudioLog("LoadWavFile OK: " + fullPath +
		" ch=" + std::to_string(outBuf.wfx.nChannels) +
		" hz=" + std::to_string(outBuf.wfx.nSamplesPerSec));
	return true;
}

//-----------------------------------------------------------------------------
//  バッファキャッシュ
//-----------------------------------------------------------------------------
const AudioBuffer* AudioSystem::GetOrLoadBuffer(const std::string& key)
{
	// キャッシュ確認
	auto cit = m_bufferCache.find(key);
	if (cit != m_bufferCache.end()) return &cit->second;

	// 登録情報から fullPath 合成
	auto rit = m_clipRegistry.find(key);
	if (rit == m_clipRegistry.end())
	{
		AudioLog("GetOrLoadBuffer: キー未登録: " + key);
		return nullptr;
	}
	const std::string fullPath = m_baseDir + rit->second.relativePath;

	AudioBuffer buf;
	if (!LoadWavFile(fullPath, buf)) return nullptr;

	m_bufferCache[key] = std::move(buf);
	return &m_bufferCache[key];
}

//-----------------------------------------------------------------------------
//  SourceVoice 生成 & エフェクト適用
//-----------------------------------------------------------------------------
IXAudio2SourceVoice* AudioSystem::CreateSourceVoice(const AudioBuffer& buf,
	const AudioEffectChain* pChain,
	bool isBGM)
{
	// サブミックスに送るための SEND_DESCRIPTOR
	XAUDIO2_SEND_DESCRIPTOR sendDesc = {};
	sendDesc.pOutputVoice = isBGM ? m_pBGMSubmix : m_pSESubmix;

	XAUDIO2_VOICE_SENDS sends = {};
	sends.SendCount = 1;
	sends.pSends = &sendDesc;

	IXAudio2SourceVoice* pVoice = nullptr;
	HRESULT hr = m_pXAudio2->CreateSourceVoice(
		&pVoice, &buf.wfx,
		XAUDIO2_VOICE_USEFILTER,  // フィルタ有効
		XAUDIO2_MAX_FREQ_RATIO,
		nullptr, &sends);

	if (FAILED(hr))
	{
		AudioLog("CreateSourceVoice failed hr=" + std::to_string(hr));
		return nullptr;
	}

	// エフェクトチェーンの適用 (トポロジカル順)
	if (pChain)
	{
		auto order = pChain->TopologicalOrder();
		for (int nid : order)
		{
			// ノードを検索
			auto it = std::find_if(pChain->nodes.begin(), pChain->nodes.end(),
				[&](const AudioEffectNode& n) { return n.nodeId == nid; });
			if (it == pChain->nodes.end() || !it->enabled) continue;

			std::visit([&](auto&& p)
				{
					using T = std::decay_t<decltype(p)>;
					if constexpr (std::is_same_v<T, ReverbParam>)
						ApplyReverbEffect(pVoice, p);
					else if constexpr (std::is_same_v<T, EchoParam>)
						ApplyEchoEffect(pVoice, p);
					else if constexpr (std::is_same_v<T, EqualizerParam>)
						ApplyEQEffect(pVoice, p);
					else if constexpr (std::is_same_v<T, LowPassFilterParam>)
						ApplyFilter(pVoice, AudioEffectType::LowPassFilter, p.frequency, p.oneOverQ);
					else if constexpr (std::is_same_v<T, HighPassFilterParam>)
						ApplyFilter(pVoice, AudioEffectType::HighPassFilter, p.frequency, p.oneOverQ);
					else if constexpr (std::is_same_v<T, VolumeParam>)
						pVoice->SetVolume(p.volume);
					else if constexpr (std::is_same_v<T, PitchParam>)
						pVoice->SetFrequencyRatio(SemitonesToRatio(p.semitones));
				}, it->param);
		}
	}

	return pVoice;
}

//-----------------------------------------------------------------------------
//  Reverb
//-----------------------------------------------------------------------------
void AudioSystem::ApplyReverbEffect(IXAudio2SourceVoice* pVoice,
	const ReverbParam& param)
{
	IUnknown* pXAPO = nullptr;
	if (FAILED(XAudio2CreateReverb(&pXAPO))) return;

	// ソースボイス自体のチャンネル数（入力フォーマットのch数）を取得する
	XAUDIO2_VOICE_DETAILS details;
	pVoice->GetVoiceDetails(&details);

	XAUDIO2_EFFECT_DESCRIPTOR desc = {};
	desc.pEffect = pXAPO;
	desc.InitialState = TRUE;
	desc.OutputChannels = details.InputChannels; // m_outputChannels から details.InputChannels に変更

	XAUDIO2_EFFECT_CHAIN chain = { 1, &desc };
	HRESULT hr = pVoice->SetEffectChain(&chain);

	// 構築に成功した場合のみパラメータを設定する（安全対策）
	if (SUCCEEDED(hr))
	{
		XAUDIO2FX_REVERB_PARAMETERS rv = {};
		rv.WetDryMix = param.wetDryMix;
		rv.RoomSize = param.roomSize;
		rv.DecayTime = param.decayTime;
		rv.ReflectionsDelay = static_cast<UINT8>(param.earlyDelay);
		rv.ReverbDelay = static_cast<UINT8>(param.lateDelay);
		rv.Density = param.density;
		rv.LowEQGain = static_cast<UINT8>(param.lowEQGain);
		rv.HighEQGain = static_cast<UINT8>(param.highEQGain);
		rv.LowEQCutoff = 4;
		rv.HighEQCutoff = 6;
		rv.RearDelay = 5;
		rv.PositionLeft = 6;
		rv.PositionRight = 6;
		rv.PositionMatrixLeft = 27;
		rv.PositionMatrixRight = 27;
		rv.RoomFilterFreq = 5000.0f;
		rv.RoomFilterMain = -10.0f;
		rv.RoomFilterHF = -1.0f;
		rv.ReflectionsGain = -26.0f;
		rv.ReverbGain = 10.0f;
		pVoice->SetEffectParameters(0, &rv, sizeof(rv));
	}
	else
	{
		AudioLog("SetEffectChain failed. HRESULT=" + std::to_string(hr));
	}

	pXAPO->Release();
}

//-----------------------------------------------------------------------------
//  Echo
//-----------------------------------------------------------------------------
void AudioSystem::ApplyEchoEffect(IXAudio2SourceVoice* pVoice,
	const EchoParam& param)
{
	IUnknown* pXAPO = nullptr;
	if (FAILED(XAudio2CreateReverb(&pXAPO))) return; // Echo は FXEcho が正規だが省略

	// ※ 実プロジェクトでは FXECHO を使用すること
	// XAudio2CreateReverb はここではスタブ代用
	// 実装例: CreateFX(__uuidof(FXEcho), &pXAPO)
	pXAPO->Release();
}

//-----------------------------------------------------------------------------
//  EQ
//-----------------------------------------------------------------------------
void AudioSystem::ApplyEQEffect(IXAudio2SourceVoice* pVoice,
	const EqualizerParam& param)
{
	// FXEQ を使用
	// ※ Windows SDK 8.0+ に含まれる FXEQ
	// CreateFX(__uuidof(FXEQ), &pXAPO); でインスタンス生成
	// FXEQ_PARAMETERS eqParam を設定して SetEffectParameters()
}

//-----------------------------------------------------------------------------
//  フィルタ (LPF / HPF)
//-----------------------------------------------------------------------------
void AudioSystem::ApplyFilter(IXAudio2SourceVoice* pVoice,
	AudioEffectType type,
	float freq, float oneOverQ)
{
	XAUDIO2_FILTER_PARAMETERS fp = {};
	fp.Type = (type == AudioEffectType::LowPassFilter)
		? LowPassFilter : HighPassFilter;
	fp.Frequency = XAudio2CutoffFrequencyToRadians(freq,
		m_pMasterVoice ? 44100 : 44100);
	fp.OneOverQ = oneOverQ;
	pVoice->SetFilterParameters(&fp);
}

//-----------------------------------------------------------------------------
//  X3DAudio DSP 更新
//-----------------------------------------------------------------------------
void AudioSystem::Update3DAudio(int /*voiceId*/, PlayingVoice& voice)
{
	// ★【重要】ダングリングポインタ対策：計算の直前に、現在のベクターの正しい先頭アドレスを再設定する
	voice.dspSettings.pMatrixCoefficients = voice.matrixCoef.data();

	// ★マルチチャンネル（ステレオ等）対策：ChannelCount が 2 以上の場合は方位角ポインタをセットする
	if (voice.emitter.ChannelCount > 1)
	{
		voice.emitter.pChannelAzimuths = voice.channelAzimuths.data();
	}
	else
	{
		voice.emitter.pChannelAzimuths = nullptr;
	}

	if (!voice.dspSettings.pMatrixCoefficients) return;

	DWORD flags =
		X3DAUDIO_CALCULATE_MATRIX |
		X3DAUDIO_CALCULATE_DOPPLER |
		X3DAUDIO_CALCULATE_LPF_DIRECT;

	// ここで落ちていたのが、上記のポインタ再設定により安全になります
	X3DAudioCalculate(m_x3dInstance,
		&m_listener,
		&voice.emitter,
		flags,
		&voice.dspSettings);

	// 実際の送信先サブミックスボイスを特定する
	IXAudio2Voice* pDestinationVoice = voice.isBGM ? m_pBGMSubmix : m_pSESubmix;
	if (!pDestinationVoice) pDestinationVoice = m_pMasterVoice;

	voice.pSourceVoice->SetOutputMatrix(
		pDestinationVoice,
		voice.dspSettings.SrcChannelCount,
		voice.dspSettings.DstChannelCount,
		voice.dspSettings.pMatrixCoefficients);

	voice.pSourceVoice->SetFrequencyRatio(
		voice.dspSettings.DopplerFactor);

	XAUDIO2_FILTER_PARAMETERS filter;
	filter.Type = LowPassFilter;
	filter.Frequency = voice.dspSettings.LPFDirectCoefficient;
	filter.OneOverQ = 1.0f;
	voice.pSourceVoice->SetFilterParameters(&filter);
}
//-----------------------------------------------------------------------------
//  JSON ロード
//-----------------------------------------------------------------------------
void AudioSystem::LoadClipRegistryFromJson()
{
	auto clipsOpt = m_json.Get<json>("clips");
	if (!clipsOpt || !clipsOpt->is_object()) return;

	for (auto& [key, val] : clipsOpt->items())
	{
		AudioClipInfo info = AudioClipInfo::FromJson(val);
		m_clipRegistry[key] = info;
		AudioLog("LoadClip from JSON: " + key);
	}
}

void AudioSystem::LoadEffectChainsFromJson()
{
	auto chainsOpt = m_json.Get<json>("effectChains");
	if (!chainsOpt || !chainsOpt->is_object()) return;

	for (auto& [name, val] : chainsOpt->items())
	{
		AudioEffectChain chain = AudioEffectChain::FromJson(val);
		m_effectChains[name] = chain;
		AudioLog("LoadEffectChain from JSON: " + name);
	}
}

//-----------------------------------------------------------------------------
//  停止ボイス回収
//-----------------------------------------------------------------------------
void AudioSystem::CollectStoppedVoices()
{
	std::lock_guard<std::mutex> lk(m_voiceMutex);
	for (auto it = m_voices.begin(); it != m_voices.end(); )
	{
		if (!it->second.pSourceVoice)
		{
			it = m_voices.erase(it);
			continue;
		}

		XAUDIO2_VOICE_STATE state;
		it->second.pSourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

		if (state.BuffersQueued == 0 && !it->second.looping)
		{
			it->second.pSourceVoice->DestroyVoice();
			it->second.pSourceVoice = nullptr;
			it = m_voices.erase(it);
		}
		else ++it;
	}
}
void AudioSystem::UpdateInput() {
	// 1フレーム前のキー状態（チャタリング・連打防止用フラグ）
	static bool prevT = false, prevF = false, prevSpace = false;
	static bool prevC = false, prevE = false, prevS = false, prevW = false;
	static bool prev1 = false, prev2 = false, prev3 = false;

	// テストでの停止検証用に、直近で再生したボイスIDを保持しておく変数
	static int lastVoiceId = -1;

	// 各キーの現在の入力状態を取得
	bool currT = (GetAsyncKeyState('T') & 0x8000) != 0;
	bool currF = (GetAsyncKeyState('F') & 0x8000) != 0;
	bool currSpace = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
	bool currC = (GetAsyncKeyState('C') & 0x8000) != 0;
	bool currE = (GetAsyncKeyState('E') & 0x8000) != 0;
	bool currS = (GetAsyncKeyState('S') & 0x8000) != 0;
	bool currW = (GetAsyncKeyState('W') & 0x8000) != 0;

	// 停止検証用追加キー
	bool curr1 = (GetAsyncKeyState('1') & 0x8000) != 0; // 直近の音をStop
	bool curr2 = (GetAsyncKeyState('2') & 0x8000) != 0; // StopAllBGM
	bool curr3 = (GetAsyncKeyState('3') & 0x8000) != 0; // StopAllSE

	// 押された「瞬間」を検知して各関数を実行
	if (currT && !prevT)         lastVoiceId = PlayBGM("bgm_title");

	{
		AudioPlayParam param;
		param.volume = 0.9f;
		param.loop = true;    // BGM は PlayBGM 内部で自動ループ化されるが明示可
		if (currF && !prevF)         lastVoiceId = PlayBGM("bgm_battle", param);
	}
	if (currSpace && !prevSpace) lastVoiceId = PlaySE("se_jump");
	if (currC && !prevC)         lastVoiceId = PlaySE("se_coin");

	// 3Dサウンド系はデモ用として適当な位置座標を引数に設定
	{
		AudioPlayParam param;
		param.spatialize = true;
		param.posX = 15.f;
		param.posY = 0.0f;
		param.posZ = -3.0f;
		if (currE && !prevE)         lastVoiceId = PlaySE("se_explosion", param);
	}
	{
		AudioPlayParam param;
		param.spatialize = true;
		param.posX = 0.0f;
		param.posY = 1.2f;
		param.posZ = -0.5f;
		param.pitch = 1.1f;  // 少し高めに
		if (currS && !prevS)         lastVoiceId = PlaySE("se_sword", param);
	}
	{
		AudioPlayParam param;
		param.spatialize = true;
		param.posX = -2.0f;
		param.posY = -1.0f;
		param.posZ = 4.0f;
		param.volume = 0.5f;
		// overrideEffectChain で実行時にエフェクトを上書き
		param.overrideEffectChain = "lpf_muffled";
		if (currW && !prevW)         lastVoiceId = PlaySE("se_footstep", param);
	}

	//-------------------------------------------------------------------------
	// 停止関数のテスト呼び出し
	//-------------------------------------------------------------------------
	if (curr1 && !prev1 && lastVoiceId != -1) {
		Stop(lastVoiceId);
		lastVoiceId = -1; // リセット
	}
	if (curr2 && !prev2) {
		StopAllBGM();
	}
	if (curr3 && !prev3) {
		StopAllSE();
	}

	// 今回の状態を「前回の状態」として保存
	prevT = currT; prevF = currF; prevSpace = currSpace;
	prevC = currC; prevE = currE; prevS = currS; prevW = currW;
	prev1 = curr1; prev2 = curr2; prev3 = curr3;
}
