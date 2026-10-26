#pragma once
#include <array>
#include <DirectXMath.h>
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/DLSS/dlss_manager.h"
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
#include "Game/Effect/chromaticAberration/ChromaticAberration.h"
#include "Game/Effect/lensDistortion/LensDistortion.h"
#include "Game/Effect/vignetting/Vignetting.h"

// ★ 新規フォグシステム
#include "Game/Effect/fog/volumetric_fog.h"
#include "Game/Effect/fog/height_fog.h"
#include "Game/Effect/fog/distance_fog.h"
#include "Game/Effect/fog/exponential_fog.h"

/**
 * @brief ポストプロセス管理クラス
 *
 * 処理フロー：
 * @code
 * begin()
 *   ↓
 * シーン描画
 *   ↓
 * end()
 *   ↓
 * draw()
 *   Sky
 *   → VolumetricFog   (レイマーチング散乱フォグ)
 *   → HeightFog        (高さベースフォグ)
 *   → DistanceFog      (距離ベースフォグ)
 *   → ExponentialFog   (指数フォグ)
 *   → DoF → Exposure → ChromaticAberration
 *   → LensDistortion → Vignetting → Bloom
 *   → Adaptation → ToneMapping
 *   ↓
 * render()
 * @endcode
 */
class Post_Process_Manager {
public:
	Post_Process_Manager() = default;
	~Post_Process_Manager() = default;

	static void initialize();
	static void update(float elapsedtime);
	static void begin();
	static void end();
	static void draw();
	static void render();
	static void SetSSREnabled(bool enabled) { ssr_enabled = enabled; }
	static bool IsSSREnabled() { return ssr_enabled; }
	static void SetSSGIEnabled(bool enabled) { ssgi_enabled = enabled; }
	static bool IsSSGIEnabled() { return ssgi_enabled; }
	static void SetSSRPerformanceMode(bool enabled);
	static void SetTAAEnabled(bool enabled);
	static bool IsTAAEnabled() { return taa_enabled; }
	static void ResetTemporalHistory();
	// Returns the current Halton jitter in NDC and advances the sequence once per frame.
	static DirectX::XMFLOAT2 GetTAAJitterNdc();
	static void SetDLSSRenderResolution(UINT width, UINT height);
	static void renderGUI();
	void drawPipelineGUI();
	bool savePipelineSettings() const;
	bool loadPipelineSettings();

public:
	// ── 既存アクセサ ─────────────────────────────────────────────
	bloom& GetBloom() { return *bloomer; }
	Fog& GetFog() { return *fogger; }
	shadow& GetShadow() { return *shadower; }
	Adaptation& GetAdaptation() { return *adaptation; }
	ToneMapping& GetToneMapping() { return *tone_mapper; }
	DepthOfField& GetDof() { return *dofer; }
	Exposure& GetExposure() { return *exposurer; }
	ChromaticAberration& GetChromaticAberration() { return *ca_effect; }
	LensDistortion& GetLensDistortion() { return *lens_distortion; }
	Vignetting& GetVignetting() { return *vignetting; }
	DLSSManager& GetDLSS() { return *dlss; }

	// ★ 新フォグアクセサ
	VolumetricFog& GetVolumetricFog() { return *vol_fog; }
	HeightFog& GetHeightFog() { return *hgt_fog; }
	DistanceFog& GetDistanceFog() { return *dst_fog; }
	ExponentialFog& GetExponentialFog() { return *exp_fog; }

	// ── GUI ──────────────────────────────────────────────────────
	void drawDebugView();
	void drawBloomGUI();
	void drawAdaptationGUI();
	void drawToneMappingGUI();
	void drawExposureGUI();
	void drawLensImperfectionsGUI();
	void drawFogGUI();
	void drawDOFGUI();

private:
	enum class EffectId { Sky, Bloom, Adaptation, VolumetricFog, HeightFog, DistanceFog, ExponentialFog, DoF, Exposure, ChromaticAberration, LensDistortion, Vignetting, ToneMapping };
	static std::array<EffectId, 13> effect_order;
	static std::array<bool, 13> effect_enabled;
	static std::array<bool, 13> saved_effect_enabled;
	static bool ssr_performance_mode;
	static Framebuffer adaptation_input;
	static ID3D11ShaderResourceView* final_color_map;
	static Framebuffer fsquad;
	static Framebuffer ssao_buffer;
	static Framebuffer ssao_blur_buffer;
	static Framebuffer ssao_vertical_buffer;
	static Framebuffer ssao_filtered_buffer;
	static Framebuffer ssr_buffer;
	static Framebuffer ssr_composite_buffer;
	static Framebuffer ssgi_buffer;
	static Framebuffer ssgi_composite_buffer;
	static std::array<Framebuffer, 2> taa_history;
	static Microsoft::WRL::ComPtr<ID3D11Buffer> taa_constant_buffer;
	static bool taa_enabled;
	static bool taa_history_valid;
	static UINT taa_history_index;
	static float taa_history_weight;
	static bool ssao_enabled;
	static bool ssr_enabled;
	static bool ssgi_enabled;
	static Microsoft::WRL::ComPtr<ID3D11Buffer> ssgi_constant_buffer;
	static std::unique_ptr<DLSSManager> dlss;
	static bool dlss_applied_this_frame;
	static UINT render_width;
	static UINT render_height;

	// ── 既存エフェクト ────────────────────────────────────────────
	static std::unique_ptr<bloom>               bloomer;
	static std::unique_ptr<Fog>                 fogger;
	static std::unique_ptr<shadow>              shadower;
	static std::unique_ptr<Adaptation>          adaptation;
	static std::unique_ptr<ToneMapping>         tone_mapper;
	static std::unique_ptr<DepthOfField>        dofer;
	static std::unique_ptr<Sky>                 skyer;
	static std::unique_ptr<Exposure>            exposurer;
	static std::unique_ptr<ChromaticAberration> ca_effect;
	static std::unique_ptr<LensDistortion>      lens_distortion;
	static std::unique_ptr<Vignetting>          vignetting;
	static std::unique_ptr<VolumetricFog>       vol_fog;
	static std::unique_ptr<HeightFog>           hgt_fog;
	static std::unique_ptr<DistanceFog>         dst_fog;
	static std::unique_ptr<ExponentialFog>      exp_fog;

	float time = 0.0f;
};
