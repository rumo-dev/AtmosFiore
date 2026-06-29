//=============================================================================
//  AudioSystemUsage.cpp
//  AudioSystem 使用例
//=============================================================================

#include "AudioSystem.h"

//--- グローバル (または Game クラスのメンバ) ---
AudioSystem g_audio;

//=============================================================================
//  初期化
//=============================================================================
void GameInitialize()
{
    // JSON パスを渡して初期化
    // JSON に書かれているクリップ / エフェクトチェーンが自動登録される
    g_audio.Initialize(
        "Assets/audio_data.json",   // JSON ファイルパス
        "Assets/Audio/",            // 音声ファイルのベースディレクトリ
        64                          // 最大同時再生ボイス数
    );

    // ─────────────────────────────────────────────
    //  コードからも動的に音源を追加登録できる
    // ─────────────────────────────────────────────
    g_audio.RegisterClip(
        "bgm_dungeon",              // キー名
        "BGM/dungeon.wav",          // baseDir からの相対パス
        0.7f,                       // デフォルトボリューム
        1.0f,                       // デフォルトピッチ
        "reverb_hall"               // エフェクトチェーン名
    );

    // 登録情報を JSON へ保存 (autoSave=true なら不要だが明示的に呼んでも OK)
    g_audio.SaveClipRegistry();

    // リスナー (プレイヤー) の初期位置
    g_audio.SetListenerPosition(0.0f, 0.0f, 0.0f);
    g_audio.SetListenerOrientation(0.0f, 0.0f, 1.0f,   // front
                                   0.0f, 1.0f, 0.0f);  // up
}

//=============================================================================
//  BGM 再生
//=============================================================================
void StartTitleScene()
{
    // キー名だけ渡す → 内部で baseDir + relativePath を合成してロード
    g_audio.PlayBGM("bgm_title");
}

void StartFieldScene()
{
    // 再生パラメータを指定する例
    AudioPlayParam param;
    param.volume = 0.9f;
    param.loop   = true;    // BGM は PlayBGM 内部で自動ループ化されるが明示可
    g_audio.PlayBGM("bgm_field", param);
}

//=============================================================================
//  SE 再生 (通常 / 3D 立体音響)
//=============================================================================
void OnPlayerJump()
{
    // シンプルな SE 再生
    g_audio.PlaySE("se_jump");
}

void OnCoinPickup()
{
    g_audio.PlaySE("se_coin");
}

void OnExplosionAt(float x, float y, float z)
{
    // 3D 立体音響で爆発音を再生
    AudioPlayParam param;
    param.spatialize = true;
    param.posX = x;
    param.posY = y;
    param.posZ = z;
    int vid = g_audio.PlaySE("se_explosion", param);

    // 後から位置を動かすことも可能
    // g_audio.SetPosition(vid, newX, newY, newZ);
}

void OnSwordSwing(float x, float y, float z)
{
    AudioPlayParam param;
    param.spatialize = true;
    param.posX = x; param.posY = y; param.posZ = z;
    param.pitch = 1.1f;  // 少し高めに
    g_audio.PlaySE("se_sword", param);
}

void OnFootstepAt(float x, float y, float z)
{
    AudioPlayParam param;
    param.spatialize = true;
    param.posX = x; param.posY = y; param.posZ = z;
    param.volume = 0.5f;
    // overrideEffectChain で実行時にエフェクトを上書き
    param.overrideEffectChain = "lpf_muffled";
    g_audio.PlaySE("se_footstep", param);
}

//=============================================================================
//  ボリューム調整
//=============================================================================
void SetBGMVolume(float v)   { g_audio.SetBGMMasterVolume(v); }
void SetSEVolume(float v)    { g_audio.SetSEMasterVolume(v);  }

//=============================================================================
//  プレイヤー移動に合わせてリスナーを更新
//=============================================================================
void OnPlayerMove(float x, float y, float z,
                  float fx, float fy, float fz)  // forward ベクトル
{
    g_audio.SetListenerPosition(x, y, z);
    g_audio.SetListenerOrientation(fx, fy, fz, 0, 1, 0);
}

//=============================================================================
//  毎フレーム更新
//=============================================================================
void GameUpdate()
{
    // 3D DSP 更新 / 停止ボイス回収
    g_audio.Update();
}

//=============================================================================
//  ImGui ウィンドウ内でエフェクトエディタを描画する
//=============================================================================
void DrawAudioEditorUI()
{
    // 特定チェーンを指定
    g_audio.DrawEffectGraphEditor("reverb_hall");

    // 空文字で最後に選択したチェーンを維持
    // g_audio.DrawEffectGraphEditor();
}

//=============================================================================
//  エフェクトチェーンをコードから構築して登録する例
//=============================================================================
void RegisterCustomEffectChain()
{
    AudioEffectChain chain;
    chain.name = "custom_cave";

    // ノード 1: LPF
    AudioEffectNode lpf;
    lpf.nodeId  = 100;
    lpf.type    = AudioEffectType::LowPassFilter;
    lpf.enabled = true;
    lpf.param   = LowPassFilterParam{ 3000.0f, 1.0f };
    chain.nodes.push_back(lpf);

    // ノード 2: Reverb
    AudioEffectNode reverb;
    reverb.nodeId  = 101;
    reverb.type    = AudioEffectType::Reverb;
    reverb.enabled = true;
    ReverbParam rv;
    rv.wetDryMix = 60.0f;
    rv.roomSize  = 0.85f;
    rv.decayTime = 2.8f;
    reverb.param = rv;
    chain.nodes.push_back(reverb);

    // ノード 3: Echo
    AudioEffectNode echo;
    echo.nodeId  = 102;
    echo.type    = AudioEffectType::Echo;
    echo.enabled = true;
    echo.param   = EchoParam{ 20.0f, 0.4f, 300.0f };
    chain.nodes.push_back(echo);

    // LPF → Reverb → Echo のリンク
    chain.links.push_back({ 200, 100, 101 });
    chain.links.push_back({ 201, 101, 102 });

    // 登録 (JSON にも自動保存)
    g_audio.RegisterEffectChain(chain);
}

//=============================================================================
//  終了
//=============================================================================
void GameShutdown()
{
    g_audio.Shutdown();
}
