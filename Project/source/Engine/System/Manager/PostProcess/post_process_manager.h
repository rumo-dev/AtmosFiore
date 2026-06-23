#pragma once
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/Shader/shader.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"

#include "Game/Effect/bloom/Bloom.h"
#include "Game/Effect/fog/fog.h"
#include "Game/Effect/shadow/shadow.h"
#include "Game/Effect/adaptation/adaptation.h"
#include "Game/Effect/tone_mapping/toon_mapping.h"
#include "Game/Effect/dof/depth_of_field.h"
#include "Game/Effect/Sky/Sky.h"
#include "Game/Effect/exposure/Exposure.h"

/**
 * @brief ポストプロセス管理クラス
 *
 * レンダリング結果に対して各種ポストエフェクトを適用する。
 *
 * 処理フロー：
 * @code
 * begin()
 *   ↓
 * シーン描画
 *   ↓
 * end()
 *   ↓
 * draw()   // Sky → DoF → Exposure → Bloom → Adaptation → ToneMapping
 *   ↓
 * render() // 最終出力
 * @endcode
 */
class Post_Process_Manager {
public:
	Post_Process_Manager() {};
	~Post_Process_Manager() {};

	static void initialize();
	static void update(float elapsedtime);
	static void begin();
	static void end();
	static void draw();
	static void render();
	static void renderGUI();

public:
	bloom& GetBloom() { return *bloomer; }
	Fog& GetFog() { return *fogger; }
	shadow& GetShadow() { return *shadower; }
	Adaptation& GetAdaptation() { return *adaptation; }
	ToneMapping& GetToneMapping() { return *tone_mapper; }
	DepthOfField& GetDof() { return *dofer; }

	// ★ 追加
	Exposure& GetExposure() { return *exposurer; }

	void drawDebugView();
	void drawBloomGUI();
	void drawAdaptationGUI();
	void drawToneMappingGUI();
	void drawExposureGUI();

private:
	static Framebuffer fsquad;

	static std::unique_ptr<bloom>        bloomer;
	static std::unique_ptr<Fog>          fogger;
	static std::unique_ptr<shadow>       shadower;
	static std::unique_ptr<Adaptation>   adaptation;
	static std::unique_ptr<ToneMapping>  tone_mapper;
	static std::unique_ptr<DepthOfField> dofer;
	static std::unique_ptr<Sky>          skyer;
	static std::unique_ptr<Exposure>     exposurer;

	float time = 0.0f;
};