#include "post_process_manager.h"
#include "Engine/System/graphics_core.h"
#include "Engine/System/Manager/resource_manager.h"

Framebuffer Post_Process_Manager::fsquad;
//std::unique_ptr<Fullscreen_Quad> Post_Process_Manager::bit_block_transfer = nullptr;
std::unique_ptr<bloom> Post_Process_Manager::bloomer = nullptr;
std::unique_ptr<Fog> Post_Process_Manager::fogger = nullptr;
std::unique_ptr<shadow> Post_Process_Manager::shadower = nullptr;
std::unique_ptr<Adaptation> Post_Process_Manager::adaptation = nullptr;
std::unique_ptr<ToneMapping> Post_Process_Manager::tone_mapper = nullptr;
std::unique_ptr<dof> Post_Process_Manager::dofer = nullptr;



void Post_Process_Manager::initialize() {

	fsquad = Framebuffer(
		Graphics_Core::instance().get_device(),
		Graphics_Core::instance().get_screen_width(),
		Graphics_Core::instance().get_screen_height(),
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		false, false
	);

	//bit_block_transfer = std::make_unique<Fullscreen_Quad>(Graphics_Core::instance().get_device());
	bloomer = std::make_unique<bloom>(
		Graphics_Core::instance().get_device(),
		static_cast<uint32_t>(Graphics_Core::instance().get_screen_width()),
		static_cast<uint32_t>(Graphics_Core::instance().get_screen_height())
	);

	fogger = std::make_unique<Fog>(
		Graphics_Core::instance().get_device(),
		static_cast<uint32_t>(Graphics_Core::instance().get_screen_width()),
		static_cast<uint32_t>(Graphics_Core::instance().get_screen_height())
	);
	shadower = std::make_unique<shadow>(
		Graphics_Core::instance().get_device(),
		static_cast<uint32_t>(Graphics_Core::instance().get_screen_width()),
		static_cast<uint32_t>(Graphics_Core::instance().get_screen_height())
	);
	adaptation = std::make_unique<Adaptation>(
		Graphics_Core::instance().get_device(),
		static_cast<uint32_t>(Graphics_Core::instance().get_screen_width()),
		static_cast<uint32_t>(Graphics_Core::instance().get_screen_height())
	);
	tone_mapper = std::make_unique<ToneMapping>(
		Graphics_Core::instance().get_device(),
		static_cast<uint32_t>(Graphics_Core::instance().get_screen_width()),
		static_cast<uint32_t>(Graphics_Core::instance().get_screen_height())
	);
	dofer = std::make_unique<dof>(
		Graphics_Core::instance().get_device(),
		static_cast<uint32_t>(Graphics_Core::instance().get_screen_width()),
		static_cast<uint32_t>(Graphics_Core::instance().get_screen_height())
	);

}

void Post_Process_Manager::update(float elapsedtime) {
	fogger->fog_constans.Time += elapsedtime;
	adaptation->delta_time = elapsedtime;

}
void Post_Process_Manager::begin() {
	fsquad.Clear(Graphics_Core::instance().get_device_context(),
		0.5f, 0.5f, 0.5f, 1.0f);
	fsquad.Activate(Graphics_Core::instance().get_device_context(), Graphics_Core::instance().get_geometry_buffer()->GetDepthStencilView());
	Render_State::instance().set_blend_state(Graphics_Core::instance().get_device_context(), Blend_State::Alpha);

	ID3D11ShaderResourceView* srvs[GBUFFER_COUNT + 3]{};

	Graphics_Core::instance().get_geometry_buffer()->GetShaderResourceViews(srvs);
	srvs[GBUFFER_COUNT + 0] = shadower->get_point_shadow_front_map();
	srvs[GBUFFER_COUNT + 1] = shadower->get_point_shadow_back_map();
	srvs[GBUFFER_COUNT + 2] = shadower->get_directional_shadow_map();
	Graphics_Core::instance().get_fullscreen_quad()->Blit(Graphics_Core::instance().get_device_context(), srvs, 0, GBUFFER_COUNT + 3, Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("DEFERRED_LIGHTING_PS"));
}
void Post_Process_Manager::end() {
	fsquad.Deactivate(Graphics_Core::instance().get_device_context());
}
void Post_Process_Manager::draw() {
	dofer->make(Graphics_Core::instance().get_device_context(), fsquad.GetColorMap());
	bloomer->make(Graphics_Core::instance().get_device_context(), dofer->getColorMap());
	//Graphics_Core::instance().get_device_context()->GenerateMips(bloomer->getColorMap());
	adaptation->make(Graphics_Core::instance().get_device_context(), bloomer->getColorMap());
	tone_mapper->make(Graphics_Core::instance().get_device_context(), adaptation->get_color_map());




}

void Post_Process_Manager::render() {
	Graphics_Core::instance().get_fullscreen_quad()->Blit(Graphics_Core::instance().get_device_context(),
		tone_mapper->get_color_map_address(), 0, 1);

}
bool CheckboxInt(const char* label, int& value, const char* tooltip = nullptr) {
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

// ヘルパー: float Slider with tooltip
void SliderFloatWithTooltip(const char* label, float* value, float min, float max, const char* tooltip = nullptr) {
#ifdef _DEBUG


	ImGui::SliderFloat(label, value, min, max);
	if (tooltip && ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", tooltip);
#endif // _DEBUG
}



void Post_Process_Manager::drawDebugView()
{

	if (ImGui::CollapsingHeader("GBuffer", ImGuiTreeNodeFlags_DefaultOpen)) {
		ID3D11ShaderResourceView* srvs[GBUFFER_COUNT + 3]{};
		Graphics_Core::instance().get_geometry_buffer()->GetShaderResourceViews(srvs);
		for (int i = 0; i < GBUFFER_COUNT; ++i) {
			if (srvs[i]) {
				ImGui::Text("GBuffer %d", i);
				ImGui::Image((ImTextureID)srvs[i], ImVec2(256, 256));
			}
		}
	}
	if (ImGui::CollapsingHeader("Shadow Maps")) {
		auto drawShadow = [](const char* label, ID3D11ShaderResourceView* srv) {
			ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s:", label);
			srv ? ImGui::Image((ImTextureID)srv, ImVec2(256, 256)) : ImGui::Text("Shadow map not available");
			};
		drawShadow("point_frontShadow_depth", shadower->get_point_shadow_front_map());
		drawShadow("point_back_Shadow_depth", shadower->get_point_shadow_back_map());
		drawShadow("directional_Shadow_depth", shadower->get_directional_shadow_map());
		shadower->shadow_gui();
	}

}

void Post_Process_Manager::drawBloomGUI()
{
	const float available_w = ImGui::GetContentRegionAvail().x;
	const float left_w = available_w * 0.4f;   // 左：パラメーター 40%
	const float right_w = available_w * 0.6f;   // 右：テクスチャ   60%

	// ── 左ペイン：パラメーター ──────────────────────────────────
	ImGui::BeginChild("##bloom_left", ImVec2(left_w, 0), false);

	CheckboxInt("Enable Bloom", bloomer->is_bloom, "Enable bloom effect");
	if (bloomer->is_bloom) {
		SliderFloatWithTooltip("Threshold", &bloomer->bloom_extraction_threshold,
			0.0f, 1.0f, "Brightness threshold for bloom");
		SliderFloatWithTooltip("Intensity", &bloomer->bloom_intensity,
			0.0f, 5.0f, "Strength of bloom effect");
	}

	ImGui::EndChild();

	// ── 仕切り線 ────────────────────────────────────────────────
	ImGui::SameLine();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();
	float h = ImGui::GetContentRegionAvail().y;
	dl->AddLine(p, ImVec2(p.x, p.y + h), IM_COL32(80, 80, 90, 255), 1.0f);
	ImGui::SetCursorScreenPos(ImVec2(p.x + 6.0f, p.y));  // 仕切り線分だけ右にずらす

	// ── 右ペイン：テクスチャプレビュー ──────────────────────────
	ImGui::BeginChild("##bloom_right", ImVec2(right_w - 6.0f, 0), false);

	if (bloomer->is_bloom && bloomer->getColorMap()) {
		ImVec2 avail = ImGui::GetContentRegionAvail();
		// アスペクト比 16:9 を保ってフィット
		float tex_w = avail.x;
		float tex_h = tex_w * (9.0f / 16.0f);
		if (tex_h > avail.y) {
			tex_h = avail.y;
			tex_w = tex_h * (16.0f / 9.0f);
		}
		ImGui::Image((ImTextureID)bloomer->getColorMap(), ImVec2(tex_w, tex_h));
	}
	else {
		// Bloom オフ時はプレースホルダー
		ImGui::TextDisabled("(Bloom disabled)");
	}

	ImGui::EndChild();
}
void Post_Process_Manager::drawAdaptationGUI()
{
	const float available_w = ImGui::GetContentRegionAvail().x;
	const float left_w = available_w * 0.4f;
	const float right_w = available_w * 0.6f;

	// ── 左ペイン：パラメーター ──────────────────────────────────
	ImGui::BeginChild("##adapt_left", ImVec2(left_w, 0), false);

	SliderFloatWithTooltip("Target Lum", &adaptation->target_lum, 0.0f, 1.0f, "Target luminance for adaptation");
	SliderFloatWithTooltip("Speed to Light", &adaptation->speed_to_light, 0.0f, 10.0f, "Speed of adaptation to lighter scenes");
	SliderFloatWithTooltip("Speed to Dark", &adaptation->speed_to_dark, 0.0f, 10.0f, "Speed of adaptation to darker scenes");

	ImGui::EndChild();

	// ── 仕切り線 ────────────────────────────────────────────────
	ImGui::SameLine();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();
	float h = ImGui::GetContentRegionAvail().y;
	dl->AddLine(p, ImVec2(p.x, p.y + h), IM_COL32(80, 80, 90, 255), 1.0f);
	ImGui::SetCursorScreenPos(ImVec2(p.x + 6.0f, p.y));

	// ── 右ペイン：テクスチャプレビュー ──────────────────────────
	ImGui::BeginChild("##adapt_right", ImVec2(right_w - 6.0f, 0), false);

	if (adaptation->get_color_map()) {
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float tex_w = avail.x;
		float tex_h = tex_w * (9.0f / 16.0f);
		if (tex_h > avail.y) { tex_h = avail.y; tex_w = tex_h * (16.0f / 9.0f); }
		ImGui::Image((ImTextureID)adaptation->get_color_map(), ImVec2(tex_w, tex_h));
	}
	else {
		ImGui::TextDisabled("(No texture available)");
	}

	ImGui::EndChild();
}

void Post_Process_Manager::drawToneMappingGUI()
{
	const float available_w = ImGui::GetContentRegionAvail().x;
	const float left_w = available_w * 0.55f;  // パラメーター多いので広め
	const float right_w = available_w * 0.45f;

	// ── 左ペイン：アルゴリズム選択 + 共通パラメーター ───────────
	ImGui::BeginChild("##tonemapping_left", ImVec2(left_w, 0), false);

	CheckboxInt("Enable Tone Mapping", tone_mapper->is_enabled, "Enable tone mapping post effect");

	if (tone_mapper->is_enabled) {
		const char* items[] = {
			"ACES", "Reinhard", "Unreal", "Neutral", "Linear", "Hable", "AgX", "GT",
			"Drago", "Exponential", "Logarithmic", "Ward", "Lottes", "Hejl",
			"RomBinDaHouse", "ReinhardExtended", "FilmicSimple", "ACESApprox",
			"PBRNeutral", "Sigmoid", "Piecewise", "Cineon", "Exposure", "GammaOnly",
			"PQApprox", "HLGApprox", "OpenDRTLike", "CameraResponse", "Uchimura",
			"ClampOnly", "WhitePreservingLuma", "FilmicALU", "AgXPunchy", "CustomCurve"
		};
		int current = static_cast<int>(tone_mapper->mapping_type);
		if (ImGui::Combo("Algorithm", &current, items, IM_ARRAYSIZE(items))) {
			tone_mapper->mapping_type = static_cast<ToneMapping::ToneMappingType>(current);
		}
		ImGui::Separator();
		ImGui::SliderFloat("Exposure", &tone_mapper->exposure, 0.01f, 10.0f);
		ImGui::SliderFloat("Gamma", &tone_mapper->gamma, 1.0f, 3.0f);
	}

	ImGui::EndChild();

	// ── 仕切り線 ────────────────────────────────────────────────
	ImGui::SameLine();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();
	float h = ImGui::GetContentRegionAvail().y;
	dl->AddLine(p, ImVec2(p.x, p.y + h), IM_COL32(80, 80, 90, 255), 1.0f);
	ImGui::SetCursorScreenPos(ImVec2(p.x + 6.0f, p.y));

	// ── 右ペイン：アルゴリズム固有パラメーター ──────────────────
	ImGui::BeginChild("##tonemapping_right", ImVec2(right_w - 6.0f, 0), false);

	if (tone_mapper->is_enabled) {
		const char* items[] = {
			"ACES", "Reinhard", "Unreal", "Neutral", "Linear", "Hable", "AgX", "GT",
			"Drago", "Exponential", "Logarithmic", "Ward", "Lottes", "Hejl",
			"RomBinDaHouse", "ReinhardExtended", "FilmicSimple", "ACESApprox",
			"PBRNeutral", "Sigmoid", "Piecewise", "Cineon", "Exposure", "GammaOnly",
			"PQApprox", "HLGApprox", "OpenDRTLike", "CameraResponse", "Uchimura",
			"ClampOnly", "WhitePreservingLuma", "FilmicALU", "AgXPunchy", "CustomCurve"
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