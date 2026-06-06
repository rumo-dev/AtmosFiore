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
	bloomer->make(Graphics_Core::instance().get_device_context(), fsquad.GetColorMap());
	Graphics_Core::instance().get_device_context()->GenerateMips(bloomer->getColorMap());
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


void Post_Process_Manager::renderGUI() {
#ifdef _DEBUG
	ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Gbuffer:");
	ID3D11ShaderResourceView* srvs[GBUFFER_COUNT + 3]{};

	Graphics_Core::instance().get_geometry_buffer()->GetShaderResourceViews(srvs);
	for (int i = 0; i < GBUFFER_COUNT; ++i)
	{
		if (srvs[i])
		{
			ImGui::Text("GBuffer %d", i);
			ImGui::Image(
				(ImTextureID)srvs[i],
				ImVec2(256, 256)   // 表示サイズ
			);
		}
	}
	ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "point_frontShadow_depth:");
	shadower->get_point_shadow_front_map() ? ImGui::Image(
		(ImTextureID)shadower->get_point_shadow_front_map(),
		ImVec2(256, 256)   // 表示サイズ
	) : ImGui::Text("Shadow map not available");

	ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "point_back_Shadow_depth:");
	shadower->get_point_shadow_back_map() ? ImGui::Image(
		(ImTextureID)shadower->get_point_shadow_back_map(),
		ImVec2(256, 256)   // 表示サイズ
	) : ImGui::Text("Shadow map not available");

	ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "directional_Shadow_depth:");
	shadower->get_directional_shadow_map() ? ImGui::Image(
		(ImTextureID)shadower->get_directional_shadow_map(),
		ImVec2(256, 256)   // 表示サイズ
	) : ImGui::Text("Shadow map not available");

	shadower->shadow_gui();
	ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Enabled Effects:");
	ImGui::BeginGroup();
	CheckboxInt("Bloom", bloomer->is_bloom, "Enable bloom effect");
	CheckboxInt("Tone Mapping", tone_mapper->is_enabled, "Enable tone mapping post effect");
	ImGui::EndGroup();

	// Bloom設定（Bloomが有効な場合のみ）
	if (bloomer->is_bloom) {
		ImGui::BeginGroup();
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Bloom:");
		SliderFloatWithTooltip("Threshold", &bloomer->bloom_extraction_threshold, 0.0f, 1.0f, "Brightness threshold for bloom");
		SliderFloatWithTooltip("Intensity", &bloomer->bloom_intensity, 0.0f, 5.0f, "Strength of bloom effect");

		// ----------- SRV（テクスチャ）プレビュー -----------
		if (bloomer->getColorMap()) {
			if (ImGui::CollapsingHeader("Bloom Texture Preview")) {
				ImVec2 texSize = ImVec2(640, 360); // 表示サイズ
				ImGui::Image((ImTextureID)bloomer->getColorMap(), texSize);
			}
		}

		ImGui::EndGroup();
	}

	ImGui::Separator();
	ImGui::BeginGroup();
	ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Adaptation:");
	SliderFloatWithTooltip("target_lum", &adaptation->target_lum, 0.0f, 1.0f, "Target luminance for adaptation");
	SliderFloatWithTooltip("Speed to Light", &adaptation->speed_to_light, 0.0f, 10.0f, "Speed of adaptation to lighter scenes");
	SliderFloatWithTooltip("Speed to Dark", &adaptation->speed_to_dark, 0.0f, 10.0f, "Speed of adaptation to darker scenes");

	// ----------- SRV（テクスチャ）プレビュー -----------
	if (adaptation->get_color_map()) {
		if (ImGui::CollapsingHeader("Adaptation Texture Preview")) {
			ImVec2 texSize = ImVec2(640, 360); // 表示サイズ
			ImGui::Image((ImTextureID)adaptation->get_color_map(), texSize);
		}
	}

	ImGui::EndGroup();


	ImGui::Separator();
	// Bloom設定（Bloomが有効な場合のみ）
	if (tone_mapper->is_enabled)
	{
		ImGui::BeginGroup();

		if (ImGui::CollapsingHeader(
			"Tone Mapping Settings",
			ImGuiTreeNodeFlags_DefaultOpen))
		{
			const char* items[] =
			{
				"ACES",
				"Reinhard",
				"Unreal",
				"Neutral",
				"Linear",
				"Hable",
				"AgX",
				"GT",

				"Drago",
				"Exponential",
				"Logarithmic",
				"Ward",
				"Lottes",
				"Hejl",
				"RomBinDaHouse",
				"ReinhardExtended",
				"FilmicSimple",
				"ACESApprox",
				"PBRNeutral",
				"Sigmoid",
				"Piecewise",
				"Cineon",
				"Exposure",
				"GammaOnly",
				"PQApprox",
				"HLGApprox",
				"OpenDRTLike",
				"CameraResponse",
				"Uchimura",
				"ClampOnly",
				"WhitePreservingLuma",
				"FilmicALU",
				"AgXPunchy",
				"CustomCurve"
			};

			int current =
				static_cast<int>(tone_mapper->mapping_type);

			if (ImGui::Combo(
				"Algorithm",
				&current,
				items,
				IM_ARRAYSIZE(items)))
			{
				tone_mapper->mapping_type =
					static_cast<ToneMapping::ToneMappingType>(current);
			}

			ImGui::Separator();

			// ----------------------------------------------------
			// Common Parameters
			// ----------------------------------------------------

			ImGui::SliderFloat(
				"Exposure",
				&tone_mapper->exposure,
				0.01f,
				10.0f);

			ImGui::SliderFloat(
				"Gamma",
				&tone_mapper->gamma,
				1.0f,
				3.0f);

			// ----------------------------------------------------
			// GT / Uchimura
			// ----------------------------------------------------

			if (tone_mapper->mapping_type ==
				ToneMapping::ToneMappingType::GT ||
				tone_mapper->mapping_type ==
				ToneMapping::ToneMappingType::Uchimura)
			{
				ImGui::SeparatorText("GT / Uchimura");

				ImGui::SliderFloat(
					"Linear Start (m)",
					&tone_mapper->gt_param,
					0.01f,
					1.0f);

				ImGui::SliderFloat(
					"Max White",
					&tone_mapper->max_white,
					1.0f,
					20.0f);
			}

			// ----------------------------------------------------
			// Reinhard Extended
			// ----------------------------------------------------

			if (tone_mapper->mapping_type ==
				ToneMapping::ToneMappingType::ReinhardExtended)
			{
				ImGui::SeparatorText("Reinhard Extended");

				ImGui::SliderFloat(
					"Max White",
					&tone_mapper->max_white,
					1.0f,
					20.0f);
			}

			// ----------------------------------------------------
			// Drago / Ward / Logarithmic
			// ----------------------------------------------------

			if (tone_mapper->mapping_type ==
				ToneMapping::ToneMappingType::Drago ||

				tone_mapper->mapping_type ==
				ToneMapping::ToneMappingType::Ward ||

				tone_mapper->mapping_type ==
				ToneMapping::ToneMappingType::Logarithmic)
			{
				ImGui::SeparatorText("HDR Parameters");

				ImGui::SliderFloat(
					"HDR White",
					&tone_mapper->max_white,
					1.0f,
					50.0f);
			}

			// ----------------------------------------------------
			// Exposure-based
			// ----------------------------------------------------

			if (tone_mapper->mapping_type ==
				ToneMapping::ToneMappingType::Exposure ||

				tone_mapper->mapping_type ==
				ToneMapping::ToneMappingType::Exponential)
			{
				ImGui::SeparatorText("Exposure");

				ImGui::SliderFloat(
					"Exposure Strength",
					&tone_mapper->exposure,
					0.01f,
					20.0f);
			}

			// ----------------------------------------------------
			// Custom Curve
			// ----------------------------------------------------

			if (tone_mapper->mapping_type ==
				ToneMapping::ToneMappingType::CustomCurve)
			{
				ImGui::SeparatorText("Custom Curve");

				ImGui::SliderFloat(
					"Shoulder",
					&tone_mapper->shoulder,
					0.01f,
					2.0f);

				ImGui::SliderFloat(
					"Linear Strength",
					&tone_mapper->linear_strength,
					0.01f,
					2.0f);

				ImGui::SliderFloat(
					"Linear Angle",
					&tone_mapper->linear_angle,
					0.01f,
					2.0f);

				ImGui::SliderFloat(
					"Toe Strength",
					&tone_mapper->toe_strength,
					0.01f,
					2.0f);
			}

			// ----------------------------------------------------
			// Debug Info
			// ----------------------------------------------------

			ImGui::Separator();

			ImGui::Text(
				"Current Mode : %s",
				items[current]);
		}

		ImGui::EndGroup();
	}


	ImGui::Separator();



#endif
}
