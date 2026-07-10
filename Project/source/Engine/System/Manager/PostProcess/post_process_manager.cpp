#include "post_process_manager.h"
#include "Engine/System/graphics_core.h"
#include "Engine/System/Manager/resource_manager.h"
#include "Engine/Graphics/UI/DebugMenu/CustomWidgets.h"
#include "Game/Effect/ChromaticAberration/ChromaticAberration.h"
#include "Game/Effect/lensDistortion/LensDistortion.h"
#include "Game/Effect/vignetting/Vignetting.h"

// ─── 静的メンバ定義 ───────────────────────────────────────────────
Framebuffer                  Post_Process_Manager::fsquad;
std::unique_ptr<bloom>       Post_Process_Manager::bloomer = nullptr;
std::unique_ptr<Fog>         Post_Process_Manager::fogger = nullptr;
std::unique_ptr<shadow>      Post_Process_Manager::shadower = nullptr;
std::unique_ptr<Adaptation>  Post_Process_Manager::adaptation = nullptr;
std::unique_ptr<ToneMapping> Post_Process_Manager::tone_mapper = nullptr;
std::unique_ptr<DepthOfField>Post_Process_Manager::dofer = nullptr;
std::unique_ptr<Sky>         Post_Process_Manager::skyer = nullptr;
std::unique_ptr<Exposure>    Post_Process_Manager::exposurer = nullptr;
std::unique_ptr<ChromaticAberration> Post_Process_Manager::ca_effect = nullptr;
std::unique_ptr<LensDistortion>      Post_Process_Manager::lens_distortion = nullptr;
std::unique_ptr<Vignetting>          Post_Process_Manager::vignetting = nullptr;

// ★ 新フォグ
std::unique_ptr<VolumetricFog>       Post_Process_Manager::vol_fog = nullptr;
std::unique_ptr<HeightFog>           Post_Process_Manager::hgt_fog = nullptr;
std::unique_ptr<DistanceFog>         Post_Process_Manager::dst_fog = nullptr;
std::unique_ptr<ExponentialFog>      Post_Process_Manager::exp_fog = nullptr;

// ─── 初期化 ───────────────────────────────────────────────────────
void Post_Process_Manager::initialize()
{
	auto* device = Graphics_Core::instance().get_device();
	auto  w = static_cast<uint32_t>(Graphics_Core::instance().get_screen_width());
	auto  h = static_cast<uint32_t>(Graphics_Core::instance().get_screen_height());

	fsquad = Framebuffer(device, w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, false, false);

	bloomer = std::make_unique<bloom>(device, w, h);
	fogger = std::make_unique<Fog>(device, w, h);
	shadower = std::make_unique<shadow>(device, w, h);
	adaptation = std::make_unique<Adaptation>(device, w, h);
	tone_mapper = std::make_unique<ToneMapping>(device, w, h);
	dofer = std::make_unique<DepthOfField>(device, w, h);
	skyer = std::make_unique<Sky>(device, w, h);
	exposurer = std::make_unique<Exposure>(device, w, h);
	ca_effect = std::make_unique<ChromaticAberration>(device, w, h);
	lens_distortion = std::make_unique<LensDistortion>(device, w, h);
	vignetting = std::make_unique<Vignetting>(device, w, h);

	// ★ 新フォグ初期化
	vol_fog = std::make_unique<VolumetricFog>(device, w, h);
	hgt_fog = std::make_unique<HeightFog>(device, w, h);
	dst_fog = std::make_unique<DistanceFog>(device, w, h);
	exp_fog = std::make_unique<ExponentialFog>(device, w, h);
}

// ─── 更新 ─────────────────────────────────────────────────────────
void Post_Process_Manager::update(float elapsedtime)
{
	fogger->fog_constans.Time += elapsedtime;
	adaptation->delta_time = elapsedtime;
	skyer->time += elapsedtime;
	hgt_fog->config.time += elapsedtime;
	vol_fog->config.time += elapsedtime;


}

// ─── 開始 ─────────────────────────────────────────────────────────
void Post_Process_Manager::begin()
{
	auto* ctx = Graphics_Core::instance().get_device_context();

	fsquad.Clear(ctx, 0.5f, 0.5f, 0.5f, 1.0f);
	fsquad.Activate(ctx, Graphics_Core::instance().get_geometry_buffer()->GetDepthStencilView());
	Render_State::instance().set_blend_state(ctx, Blend_State::Alpha);

	ID3D11ShaderResourceView* srvs[GBUFFER_COUNT + 3]{};
	Graphics_Core::instance().get_geometry_buffer()->GetShaderResourceViews(srvs);
	srvs[GBUFFER_COUNT + 0] = shadower->get_point_shadow_front_map();
	srvs[GBUFFER_COUNT + 1] = shadower->get_point_shadow_back_map();
	srvs[GBUFFER_COUNT + 2] = shadower->get_directional_shadow_map();

	DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Lighting Pass"); {
		Graphics_Core::instance().get_fullscreen_quad()->Blit(
			ctx, srvs, 0, GBUFFER_COUNT + 3,
			Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("DEFERRED_LIGHTING_PS")
		);
	}
}

// ─── 終了 ─────────────────────────────────────────────────────────
void Post_Process_Manager::end()
{
	fsquad.Deactivate(Graphics_Core::instance().get_device_context());
}

// ─── ポストエフェクトパイプライン ────────────────────────────────
//
//  Sky
//  → VolumetricFog   (3D レイマーチング散乱)
//  → HeightFog        (高さ指数フォグ)
//  → DistanceFog      (距離リニアフォグ)
//  → ExponentialFog   (距離指数フォグ)
//  → DoF
//  → Exposure
//  → ChromaticAberration
//  → LensDistortion
//  → Vignetting
//  → Bloom
//  → Adaptation
//  → ToneMapping
//
void Post_Process_Manager::draw()
{
	auto* ctx = Graphics_Core::instance().get_device_context();

	// 1. Sky
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Sky Pass");
		skyer->make(ctx, fsquad.GetColorMap());
	}

	// ★ 2. VolumetricFog（Sky 直後 ＝ HDR 生輝度に対してフォグを乗せる）
	//       シェーダー側で GBuffer2(position) を t1 として読む。
	//       事前にバインドしておく必要がある場合はここで行うこと。
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"VolumetricFog");
		vol_fog->make(ctx, skyer->get_color_map());
	}

	// ★ 3. HeightFog
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"HeightFog");
		hgt_fog->make(ctx, vol_fog->get_color_map());
	}

	// ★ 4. DistanceFog
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"DistanceFog");
		dst_fog->make(ctx, hgt_fog->get_color_map());
	}

	// ★ 5. ExponentialFog
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"ExponentialFog");
		exp_fog->make(ctx, dst_fog->get_color_map());
	}

	// 6. DoF（フォグ後に被写界深度を適用することでボケ端にフォグが馴染む）
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"DoF");
		dofer->make(ctx, exp_fog->get_color_map());
	}

	// 7. Exposure
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Exposure");
		exposurer->make(ctx, dofer->GetColorMap());
	}
	//exposurer->make(ctx, exp_fog->get_color_map());

	// 8. ChromaticAberration
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"ChromaticAberration");
		ca_effect->make(ctx, exposurer->GetColorMap());
	}

	// 9. LensDistortion
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"LensDistortion");
		lens_distortion->make(ctx, ca_effect->GetColorMap());
	}

	// 10. Vignetting
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Vignetting");
		vignetting->make(ctx, lens_distortion->GetColorMap());
	}

	// 11. Bloom
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Bloom");
		bloomer->make(ctx, vignetting->GetColorMap());
	}

	// 12. Adaptation（自動露出）
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Adaptation");
		adaptation->make(ctx, bloomer->getColorMap());
	}

	// 13. ToneMapping
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"ToneMapping");
		tone_mapper->make(ctx, adaptation->get_color_map());
	}
}

// ─── 最終描画 ─────────────────────────────────────────────────────
void Post_Process_Manager::render()
{
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Post-Process Final");
		Graphics_Core::instance().get_fullscreen_quad()->Blit(
			Graphics_Core::instance().get_device_context(),
			tone_mapper->get_color_map_address(), 0, 1
		);
	}
}

// ─── GUI ─────────────────────────────────────────────────────────
static bool CheckboxInt(const char* label, int& value, const char* tooltip = nullptr)
{
#ifdef _DEBUG
	bool temp = value != 0;
	if (ImGui::Checkbox(label, &temp)) {
		value = temp ? 1 : 0;
		return true;
	}
	if (tooltip && ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", tooltip);
#endif
	return false;
}

static void SliderFloatWithTooltip(const char* label, float* value, float min, float max,
	const char* tooltip = nullptr)
{
#ifdef _DEBUG
	ImGui::SliderFloat(label, value, min, max);
	if (tooltip && ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", tooltip);
#endif
}

// ── Exposure GUI ─────────────────────────────────────────────────
void Post_Process_Manager::drawExposureGUI()
{
	exposurer->DrawDebugUI();
}

// ── Lens Imperfections GUI ────────────────────────────────────────
void Post_Process_Manager::drawLensImperfectionsGUI()
{
	if (ImGui::BeginTabBar("LensImperfectionsTabs"))
	{
		if (ImGui::BeginTabItem("Chromatic Aberration"))
		{
			ca_effect->DrawDebugUI();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Lens Distortion"))
		{
			lens_distortion->DrawDebugUI();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Vignetting"))
		{
			vignetting->DrawDebugUI();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

// ★ ── Fog GUI（全フォグ種をタブで統合） ─────────────────────────
void Post_Process_Manager::drawFogGUI()
{
	if (ImGui::BeginTabBar("FogTypeTabs"))
	{
		if (ImGui::BeginTabItem("Volumetric"))
		{
			ImGui::TextDisabled("Ray-marching + FBM noise + light scattering");
			ImGui::Separator();
			vol_fog->DrawDebugUI();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Height"))
		{
			ImGui::TextDisabled("Exponential density based on world Y");
			ImGui::Separator();
			hgt_fog->DrawDebugUI();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Distance"))
		{
			ImGui::TextDisabled("Linear fog between Start / End distance");
			ImGui::Separator();
			dst_fog->DrawDebugUI();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Exponential"))
		{
			ImGui::TextDisabled("exp / exp2 fog by camera distance");
			ImGui::Separator();
			exp_fog->DrawDebugUI();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

// ─── 既存 GUI（変更なし）────────────────────────────────────────
void Post_Process_Manager::drawDebugView()
{
	const float available_w = ImGui::GetContentRegionAvail().x;
	const float left_w = available_w * 0.4f;
	const float right_w = available_w * 0.6f;

	enum class DebugTex { None, GBuffer, PointFront, PointBack, DirShadow };
	static DebugTex selected_type = DebugTex::None;
	static int      selected_gbuffer_idx = 0;

	ImGui::BeginChild("##debug_left", ImVec2(left_w, 0), false);

	if (ImGui::CollapsingHeader("GBuffer", ImGuiTreeNodeFlags_DefaultOpen)) {
		ID3D11ShaderResourceView* srvs[GBUFFER_COUNT + 3]{};
		Graphics_Core::instance().get_geometry_buffer()->GetShaderResourceViews(srvs);
		for (int i = 0; i < GBUFFER_COUNT; ++i) {
			if (srvs[i]) {
				char label[32];
				snprintf(label, sizeof(label), "GBuffer %d", i);
				bool is_selected = (selected_type == DebugTex::GBuffer && selected_gbuffer_idx == i);
				if (ImGui::Selectable(label, is_selected)) {
					selected_type = DebugTex::GBuffer;
					selected_gbuffer_idx = i;
				}
			}
		}
	}

	if (ImGui::CollapsingHeader("Shadow Maps")) {
		auto drawShadowSelectable = [&](const char* label, DebugTex type, ID3D11ShaderResourceView* srv) {
			if (!srv) { ImGui::TextDisabled("%s (N/A)", label); return; }
			bool is_selected = (selected_type == type);
			if (ImGui::Selectable(label, is_selected)) selected_type = type;
			};
		drawShadowSelectable("Point Front Depth", DebugTex::PointFront, shadower->get_point_shadow_front_map());
		drawShadowSelectable("Point Back Depth", DebugTex::PointBack, shadower->get_point_shadow_back_map());
		drawShadowSelectable("Directional Depth", DebugTex::DirShadow, shadower->get_directional_shadow_map());
		ImGui::Separator();
		shadower->shadow_gui();
	}

	ImGui::EndChild();

	ImGui::SameLine();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();
	float  h = ImGui::GetContentRegionAvail().y;
	dl->AddLine(p, ImVec2(p.x, p.y + h), IM_COL32(80, 80, 90, 255), 1.0f);
	ImGui::SetCursorScreenPos(ImVec2(p.x + 6.0f, p.y));

	ImGui::BeginChild("##debug_right", ImVec2(right_w - 6.0f, 0), false);

	ID3D11ShaderResourceView* preview_srv = nullptr;
	ImVec2 preview_size = ImVec2(1280.0f, 720.0f);

	if (selected_type == DebugTex::GBuffer) {
		ID3D11ShaderResourceView* srvs[GBUFFER_COUNT + 3]{};
		Graphics_Core::instance().get_geometry_buffer()->GetShaderResourceViews(srvs);
		if (selected_gbuffer_idx < GBUFFER_COUNT) preview_srv = srvs[selected_gbuffer_idx];
	}
	else if (selected_type == DebugTex::PointFront)
		preview_srv = shadower->get_point_shadow_front_map();
	else if (selected_type == DebugTex::PointBack)
		preview_srv = shadower->get_point_shadow_back_map();
	else if (selected_type == DebugTex::DirShadow)
		preview_srv = shadower->get_directional_shadow_map();

	if (preview_srv) {
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float  tex_w = avail.x;
		float  tex_h = tex_w * (preview_size.y / preview_size.x);
		if (tex_h > avail.y) { tex_h = avail.y; tex_w = tex_h * (preview_size.x / preview_size.y); }
		ImGui::Image(reinterpret_cast<ImTextureID>(preview_srv), ImVec2(tex_w, tex_h));
	}
	else {
		ImGui::TextDisabled("Select a buffer to preview.");
	}

	ImGui::EndChild();
}

void Post_Process_Manager::drawBloomGUI()
{
	const float available_w = ImGui::GetContentRegionAvail().x;
	const float left_w = available_w * 0.4f;
	const float right_w = available_w * 0.6f;

	ImGui::BeginChild("##bloom_left", ImVec2(left_w, 0), false);
	CheckboxInt("Enable Bloom", bloomer->is_bloom, "Enable bloom effect");
	if (bloomer->is_bloom) {
		SliderFloatWithTooltip("Threshold", &bloomer->bloom_extraction_threshold,
			0.0f, 1.0f, "Brightness threshold for bloom");
		SliderFloatWithTooltip("Intensity", &bloomer->bloom_intensity,
			0.0f, 5.0f, "Strength of bloom effect");
	}
	ImGui::EndChild();

	ImGui::SameLine();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();
	float  h = ImGui::GetContentRegionAvail().y;
	dl->AddLine(p, ImVec2(p.x, p.y + h), IM_COL32(80, 80, 90, 255), 1.0f);
	ImGui::SetCursorScreenPos(ImVec2(p.x + 6.0f, p.y));

	ImGui::BeginChild("##bloom_right", ImVec2(right_w - 6.0f, 0), false);
	if (bloomer->is_bloom) {
		static int selected_index = 0;
		auto assets = bloomer->GetDebugAssets();
		CustomUI::ImageGallery("BloomBufferGallery", assets, &selected_index, 64.0f);
	}
	else {
		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImGui::SetCursorPos(ImVec2(avail.x * 0.3f, avail.y * 0.5f));
		ImGui::TextDisabled("Bloom effect is disabled.");
	}
	ImGui::EndChild();
}

void Post_Process_Manager::drawAdaptationGUI()
{
	const float available_w = ImGui::GetContentRegionAvail().x;
	const float left_w = available_w * 0.4f;
	const float right_w = available_w * 0.6f;

	ImGui::BeginChild("##adapt_left", ImVec2(left_w, 0), false);
	SliderFloatWithTooltip("Target Lum", &adaptation->target_lum, 0.0f, 1.0f, "Target luminance for adaptation");
	SliderFloatWithTooltip("Speed to Light", &adaptation->speed_to_light, 0.0f, 10.0f, "Speed of adaptation to lighter scenes");
	SliderFloatWithTooltip("Speed to Dark", &adaptation->speed_to_dark, 0.0f, 10.0f, "Speed of adaptation to darker scenes");
	ImGui::EndChild();

	ImGui::SameLine();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();
	float  h = ImGui::GetContentRegionAvail().y;
	dl->AddLine(p, ImVec2(p.x, p.y + h), IM_COL32(80, 80, 90, 255), 1.0f);
	ImGui::SetCursorScreenPos(ImVec2(p.x + 6.0f, p.y));

	ImGui::BeginChild("##adapt_right", ImVec2(right_w - 6.0f, 0), false);
	if (adaptation->get_color_map()) {
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float  tex_w = avail.x;
		float  tex_h = tex_w * (9.0f / 16.0f);
		if (tex_h > avail.y) { tex_h = avail.y; tex_w = tex_h * (16.0f / 9.0f); }
		ImGui::Image(reinterpret_cast<ImTextureID>(adaptation->get_color_map()), ImVec2(tex_w, tex_h));
	}
	else {
		ImGui::TextDisabled("(No texture available)");
	}
	ImGui::EndChild();
}

void Post_Process_Manager::drawToneMappingGUI()
{
	const float available_w = ImGui::GetContentRegionAvail().x;
	const float left_w = available_w * 0.55f;
	const float right_w = available_w * 0.45f;

	ImGui::BeginChild("##tonemapping_left", ImVec2(left_w, 0), false);
	CheckboxInt("Enable Tone Mapping", tone_mapper->is_enabled, "Enable tone mapping post effect");

	if (tone_mapper->is_enabled) {
		const char* items[] = {
			"ACES","Reinhard","Unreal","Neutral","Linear","Hable","AgX","GT",
			"Drago","Exponential","Logarithmic","Ward","Lottes","Hejl",
			"RomBinDaHouse","ReinhardExtended","FilmicSimple","ACESApprox",
			"PBRNeutral","Sigmoid","Piecewise","Cineon","Exposure","GammaOnly",
			"PQApprox","HLGApprox","OpenDRTLike","CameraResponse","Uchimura",
			"ClampOnly","WhitePreservingLuma","FilmicALU","AgXPunchy","CustomCurve"
		};
		int current = static_cast<int>(tone_mapper->mapping_type);
		if (ImGui::Combo("Algorithm", &current, items, IM_ARRAYSIZE(items)))
			tone_mapper->mapping_type = static_cast<ToneMapping::ToneMappingType>(current);

		ImGui::Separator();
		ImGui::SliderFloat("Exposure", &tone_mapper->exposure, 0.01f, 10.0f);
		ImGui::SliderFloat("Gamma", &tone_mapper->gamma, 1.0f, 3.0f);
	}
	ImGui::EndChild();

	ImGui::SameLine();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();
	float  h = ImGui::GetContentRegionAvail().y;
	dl->AddLine(p, ImVec2(p.x, p.y + h), IM_COL32(80, 80, 90, 255), 1.0f);
	ImGui::SetCursorScreenPos(ImVec2(p.x + 6.0f, p.y));

	ImGui::BeginChild("##tonemapping_right", ImVec2(right_w - 6.0f, 0), false);
	if (tone_mapper->is_enabled) {
		const char* items[] = {
			"ACES","Reinhard","Unreal","Neutral","Linear","Hable","AgX","GT",
			"Drago","Exponential","Logarithmic","Ward","Lottes","Hejl",
			"RomBinDaHouse","ReinhardExtended","FilmicSimple","ACESApprox",
			"PBRNeutral","Sigmoid","Piecewise","Cineon","Exposure","GammaOnly",
			"PQApprox","HLGApprox","OpenDRTLike","CameraResponse","Uchimura",
			"ClampOnly","WhitePreservingLuma","FilmicALU","AgXPunchy","CustomCurve"
		};
		int current = static_cast<int>(tone_mapper->mapping_type);

		using TM = ToneMapping::ToneMappingType;
		if (tone_mapper->mapping_type == TM::GT || tone_mapper->mapping_type == TM::Uchimura) {
			ImGui::SeparatorText("GT / Uchimura");
			ImGui::SliderFloat("Linear Start (m)", &tone_mapper->gt_param, 0.01f, 1.0f);
			ImGui::SliderFloat("Max White", &tone_mapper->max_white, 1.0f, 20.0f);
		}
		else if (tone_mapper->mapping_type == TM::ReinhardExtended) {
			ImGui::SeparatorText("Reinhard Extended");
			ImGui::SliderFloat("Max White", &tone_mapper->max_white, 1.0f, 20.0f);
		}
		else if (tone_mapper->mapping_type == TM::Drago ||
			tone_mapper->mapping_type == TM::Ward ||
			tone_mapper->mapping_type == TM::Logarithmic) {
			ImGui::SeparatorText("HDR Parameters");
			ImGui::SliderFloat("HDR White", &tone_mapper->max_white, 1.0f, 50.0f);
		}
		else if (tone_mapper->mapping_type == TM::Exposure ||
			tone_mapper->mapping_type == TM::Exponential) {
			ImGui::SeparatorText("Exposure");
			ImGui::SliderFloat("Exposure Strength", &tone_mapper->exposure, 0.01f, 20.0f);
		}
		else if (tone_mapper->mapping_type == TM::CustomCurve) {
			ImGui::SeparatorText("Custom Curve");
			ImGui::SliderFloat("Shoulder", &tone_mapper->shoulder, 0.01f, 2.0f);
			ImGui::SliderFloat("Linear Strength", &tone_mapper->linear_strength, 0.01f, 2.0f);
			ImGui::SliderFloat("Linear Angle", &tone_mapper->linear_angle, 0.01f, 2.0f);
			ImGui::SliderFloat("Toe Strength", &tone_mapper->toe_strength, 0.01f, 2.0f);
		}
		else {
			ImGui::TextDisabled("(No extra parameters)");
		}

		ImGui::Spacing();
		ImGui::TextDisabled("Current: %s", items[current]);
	}
	ImGui::EndChild();
}
void Post_Process_Manager::drawDOFGUI() {
	CheckboxInt("Enable Dof", dofer->is_dof, "Enable bloom effect");
}
