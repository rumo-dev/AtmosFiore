#pragma once

//=============================================================================
//  AudioSystem.h
//  DirectX11 / XAudio2 / X3DAudio 音響システム
//
//  機能:
//   - BGM / SE の同時多重再生
//   - 3D 立体音響 (X3DAudio)
//   - 音源をキー名でマップ登録 → PlayBGM("key") / PlaySE("key") で再生
//   - 音響エフェクトチェーンを JSON で管理 (JsonDataManager 使用)
//   - imnodes によるエフェクトグラフ GUI
//
//  依存:
//   - XAudio2.h / x3daudio.h (DirectX SDK / Windows SDK)
//   - xaudio2fx.h (XAPO: Reverb / Echo / EQ)
//   - nlohmann/json (json.hpp)
//   - imnodes.h
//   - JsonDataManager.h (添付)
//   - AudioEffect.h
//
//  コンパイル:
//   - /std:c++17 以上
//   - リンク: xaudio2.lib
//=============================================================================

#pragma comment(lib, "xaudio2.lib")

#include <xaudio2.h>
#include <x3daudio.h>
#include <xaudio2fx.h>
#include <wrl/client.h>

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <optional>
#include <mutex>
#include <functional>

#include "AudioEffect.h"
#include "Engine/Utilities/JsonDataManager.h"
#include "Engine/Graphics/UI/Imgui/imnodes.h"    // ImNodes

//-----------------------------------------------------------------------------
//  前方宣言
//-----------------------------------------------------------------------------
struct AudioClip;
struct AudioVoice;
class  AudioEffectGraphEditor;

//=============================================================================
//  AudioClipInfo  ─ 音源登録情報
//=============================================================================
struct AudioClipInfo
{
	std::string key;         // 登録キー名
	std::string relativePath;// 相対パス (baseDir からの)
	bool        isStreaming = false; // 将来拡張用
	float       defaultVolume = 1.0f;
	float       defaultPitch = 1.0f;
	std::string effectChain;  // 適用するエフェクトチェーン名 (空なら無し)

	json ToJson() const {
		return {
			{"key",           key},
			{"relativePath",  relativePath},
			{"isStreaming",   isStreaming},
			{"defaultVolume", defaultVolume},
			{"defaultPitch",  defaultPitch},
			{"effectChain",   effectChain}
		};
	}
	static AudioClipInfo FromJson(const json& j) {
		AudioClipInfo i;
		i.key = j.value("key", std::string(""));
		i.relativePath = j.value("relativePath", std::string(""));
		i.isStreaming = j.value("isStreaming", false);
		i.defaultVolume = j.value("defaultVolume", 1.0f);
		i.defaultPitch = j.value("defaultPitch", 1.0f);
		i.effectChain = j.value("effectChain", std::string(""));
		return i;
	}
};

//=============================================================================
//  AudioBuffer  ─ デコード済み PCM データ
//=============================================================================
struct AudioBuffer
{
	std::vector<BYTE>  data;
	WAVEFORMATEX       wfx = {};
	UINT32             loopCount = 0; // XAUDIO2_LOOP_INFINITE = 255
};

//=============================================================================
//  PlayingVoice  ─ 再生中ボイス情報
//=============================================================================
struct PlayingVoice
{
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	std::string                clipKey;
	bool                       isBGM = false;
	bool                       looping = false;
	std::vector<IUnknown*>     xapoEffects; // XAPO の COM ポインタ

	// 3D 音源パラメータ
	X3DAUDIO_EMITTER           emitter = {};
	X3DAUDIO_DSP_SETTINGS      dspSettings = {};
	std::vector<float>         matrixCoef;   // DSP 用行列バッファ
	std::vector<float>			channelAzimuths; // ★これを新しく追加

	PlayingVoice() = default;
	PlayingVoice(const PlayingVoice&) = delete;
	PlayingVoice& operator=(const PlayingVoice&) = delete;
	PlayingVoice(PlayingVoice&&) = default;
	PlayingVoice& operator=(PlayingVoice&&) = default;
};

//=============================================================================
//  AudioPlayParam  ─ 再生時オプション
//=============================================================================
struct AudioPlayParam
{
	float   volume = 1.0f;
	float   pitch = 1.0f;      // 周波数比 (1.0 = 元ピッチ)
	bool    loop = false;
	bool    spatialize = false;     // 3D 空間定位を使うか
	float   posX = 0.0f;
	float   posY = 0.0f;
	float   posZ = 0.0f;
	std::string overrideEffectChain;  // JSON キー名で上書き指定 (空なら登録情報を使用)
};

//=============================================================================
//  AudioSystem
//=============================================================================
class AudioSystem
{
public:
	void UpdateInput();
	static AudioSystem& instance()
	{
		static AudioSystem instance;
		return instance;
	}
	//-------------------------------------------------------------------------
	//  初期化 / 終了
	//-------------------------------------------------------------------------

	/// @param jsonPath      音源登録 & エフェクト JSON ファイルパス
	/// @param baseDir       音声ファイルのベースディレクトリ
	/// @param maxVoices     同時再生ボイス上限
	bool Initialize(const std::string& jsonPath,
		const std::string& baseDir = "Assets/Audio/",
		UINT32             maxVoices = 32);

	void Shutdown();

	//-------------------------------------------------------------------------
	//  フレーム更新 (毎フレーム呼ぶ)
	//  3D 音源の更新、停止ボイスの回収を行う
	//-------------------------------------------------------------------------
	void Update();

	//=========================================================================
	//  音源登録
	//=========================================================================

	/// 音源をキー名でマップに登録する
	/// @param key           ゲーム内識別キー
	/// @param relativePath  baseDir からの相対パス (例: "BGM/field.wav")
	/// @param effectChain   適用エフェクトチェーン名 (空なら無し)
	bool RegisterClip(const std::string& key,
		const std::string& relativePath,
		float              defaultVolume = 1.0f,
		float              defaultPitch = 1.0f,
		const std::string& effectChain = "");

	/// 登録済み音源を削除する
	void UnregisterClip(const std::string& key);

	/// 全音源情報を JSON へ保存する
	bool SaveClipRegistry() const;

	//=========================================================================
	//  再生 / 停止
	//  ─ 引数に JSON キー名を渡すと baseDir + relativePath を合成して読み込む
	//=========================================================================

	/// BGM 再生 (既存 BGM を停止してから再生)
	/// @param key    RegisterClip で登録したキー名
	/// @param param  再生パラメータ (省略可)
	/// @return       再生ボイス ID (-1 = 失敗)
	int PlayBGM(const std::string& key,
		const AudioPlayParam& param = {});

	/// SE 再生 (多重再生可)
	/// @param key    RegisterClip で登録したキー名
	/// @param param  再生パラメータ (省略可)
	/// @return       再生ボイス ID (-1 = 失敗)
	int PlaySE(const std::string& key,
		const AudioPlayParam& param = {});

	/// 指定ボイスを停止する
	void Stop(int voiceId, float fadeOutSec = 0.0f);

	/// 全 BGM を停止する
	void StopAllBGM(float fadeOutSec = 0.0f);

	/// 全 SE を停止する
	void StopAllSE();

	/// 全ボイスを停止する
	void StopAll();

	//=========================================================================
	//  ボイス操作
	//=========================================================================

	void SetVolume(int voiceId, float volume);
	void SetPitch(int voiceId, float pitch);
	void SetPosition(int voiceId, float x, float y, float z);

	/// BGM 全体のマスターボリューム
	void SetBGMMasterVolume(float v);
	/// SE 全体のマスターボリューム
	void SetSEMasterVolume(float v);

	bool IsPlaying(int voiceId) const;

	//=========================================================================
	//  3D リスナー設定
	//=========================================================================
	void SetListenerPosition(float x, float y, float z);
	void SetListenerOrientation(float fx, float fy, float fz,
		float ux, float uy, float uz);
	void SetListenerVelocity(float vx, float vy, float vz);

	//=========================================================================
	//  エフェクトチェーン管理 (JSON)
	//=========================================================================

	/// エフェクトチェーンを登録する
	void RegisterEffectChain(const AudioEffectChain& chain);

	/// 名前でエフェクトチェーンを取得する
	const AudioEffectChain* GetEffectChain(const std::string& name) const;

	/// エフェクトチェーン一覧を返す
	std::vector<std::string> GetEffectChainNames() const;

	/// エフェクトチェーンを JSON へ保存する
	bool SaveEffectChains() const;

	//=========================================================================
	//  imnodes GUI
	//=========================================================================

	/// エフェクトグラフエディタを描画する (ImGui ウィンドウ内で呼ぶ)
	/// @param chainName  編集対象チェーン名 (空文字 = 現在選択中のチェーン)
	void DrawEffectGraphEditor(const std::string& chainName = "");

	/// 全 JSON を再読み込みする (ランタイムリロード)
	bool ReloadJson();

	//=========================================================================
	//  ゲッター
	//=========================================================================
	IXAudio2* GetXAudio2()      const { return m_pXAudio2.Get(); }
	IXAudio2MasteringVoice* GetMasterVoice() const { return m_pMasterVoice; }
	const std::string& GetBaseDir()      const { return m_baseDir; }
	JsonDataManager& GetJsonManager() { return m_json; }

private:
	//=========================================================================
	//  内部実装
	//=========================================================================

	//--- XAudio2 ---
	Microsoft::WRL::ComPtr<IXAudio2> m_pXAudio2;
	IXAudio2MasteringVoice* m_pMasterVoice = nullptr;
	IXAudio2SubmixVoice* m_pBGMSubmix = nullptr;
	IXAudio2SubmixVoice* m_pSESubmix = nullptr;
	X3DAUDIO_HANDLE                  m_x3dInstance = {};
	X3DAUDIO_LISTENER                m_listener = {};
	DWORD                            m_channelMask = 0;
	UINT32                           m_outputChannels = 0;

	//--- 音源マップ ---
	std::string                                   m_baseDir;
	std::unordered_map<std::string, AudioClipInfo> m_clipRegistry;  // キー → 登録情報
	std::unordered_map<std::string, AudioBuffer>   m_bufferCache;   // キー → バッファ

	//--- 再生中ボイス ---
	std::unordered_map<int, PlayingVoice>    m_voices;
	int                                      m_nextVoiceId = 0;
	mutable std::mutex                       m_voiceMutex;

	//--- マスターボリューム ---
	float m_bgmMasterVolume = 1.0f;
	float m_seMasterVolume = 1.0f;

	//--- エフェクトチェーン ---
	std::unordered_map<std::string, AudioEffectChain> m_effectChains;

	//--- JSON 管理 (JsonDataManager 使用) ---
	JsonDataManager m_json;

	//--- imnodes 編集状態 ---
	std::string m_editingChainName;
	int         m_imNodeNextId = 1;

	//--- 内部メソッド ---

	/// WAV ファイルを読み込んで AudioBuffer を生成する
	bool LoadWavFile(const std::string& fullPath, AudioBuffer& outBuffer);

	/// キーから fullPath を合成して AudioBuffer を取得する (キャッシュ付き)
	const AudioBuffer* GetOrLoadBuffer(const std::string& key);

	/// SourceVoice を生成してエフェクトを適用する
	IXAudio2SourceVoice* CreateSourceVoice(const AudioBuffer& buf,
		const AudioEffectChain* pChain,
		bool isBGM);

	/// XAPO エフェクト (Reverb) を SourceVoice に適用する
	void ApplyReverbEffect(IXAudio2SourceVoice* pVoice,
		const ReverbParam& param);

	/// XAPO エフェクト (Echo) を SourceVoice に適用する
	void ApplyEchoEffect(IXAudio2SourceVoice* pVoice,
		const EchoParam& param);

	/// XAPO エフェクト (EQ) を SourceVoice に適用する
	void ApplyEQEffect(IXAudio2SourceVoice* pVoice,
		const EqualizerParam& param);

	/// フィルタ (LPF / HPF) を SourceVoice に適用する
	void ApplyFilter(IXAudio2SourceVoice* pVoice,
		AudioEffectType type, float freq, float oneOverQ);

	/// X3DAudio DSP 設定を更新して SourceVoice に適用する
	void Update3DAudio(int voiceId, PlayingVoice& voice);

	/// 再生共通処理
	int PlayInternal(const std::string& key,
		const AudioPlayParam& param,
		bool isBGM);

	/// JSON からクリップ登録情報を読み込む
	void LoadClipRegistryFromJson();

	/// JSON からエフェクトチェーンを読み込む
	void LoadEffectChainsFromJson();

	/// imnodes でエフェクトノードのプロパティを描画する
	void DrawNodeProperties(AudioEffectNode& node);

	/// 停止済みボイスを回収する
	void CollectStoppedVoices();

	/// 新しいノード ID を発行する
	int NewNodeId() { return m_imNodeNextId++; }
};

//=============================================================================
//  JSON スキーマ仕様
//=============================================================================
/*
  audio_data.json の構造:

  {
	"baseDir": "Assets/Audio/",

	"clips": {
	  "bgm_field": {
		"key":           "bgm_field",
		"relativePath":  "BGM/field.wav",
		"defaultVolume": 0.8,
		"defaultPitch":  1.0,
		"effectChain":   "reverb_hall"
	  },
	  "se_explosion": {
		"key":           "se_explosion",
		"relativePath":  "SE/explosion.wav",
		"defaultVolume": 1.0,
		"defaultPitch":  1.0,
		"effectChain":   "echo_light"
	  }
	},

	"effectChains": {
	  "reverb_hall": {
		"name":  "reverb_hall",
		"nodes": [
		  {
			"nodeId":  1,
			"type":    "Volume",
			"enabled": true,
			"param":   { "volume": 0.9 }
		  },
		  {
			"nodeId":  2,
			"type":    "Reverb",
			"enabled": true,
			"param": {
			  "wetDryMix": 60.0,
			  "roomSize":  0.8,
			  "decayTime": 2.5,
			  "earlyDelay": 15.0,
			  "lateDelay":  40.0,
			  "density":   100.0,
			  "lowEQGain":   8.0,
			  "highEQGain":  8.0
			}
		  }
		],
		"links": [
		  { "linkId": 1, "fromNode": 1, "toNode": 2 }
		]
	  },
	  "echo_light": {
		"name":  "echo_light",
		"nodes": [
		  {
			"nodeId":  10,
			"type":    "Echo",
			"enabled": true,
			"param": {
			  "wetDryMix": 30.0,
			  "feedback":  0.3,
			  "delay":     200.0
			}
		  }
		],
		"links": []
	  }
	}
  }
*/
