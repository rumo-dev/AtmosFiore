#include "post_process_manager.h"
#include "Engine/System/graphics_core.h"
#include "Engine/System/Manager/resource_manager.h"
#include "Engine/Graphics/UI/DebugMenu/CustomWidgets.h"
#include "Game/Effect/ChromaticAberration/ChromaticAberration.h"
#include "Game/Effect/lensDistortion/LensDistortion.h"
#include "Game/Effect/vignetting/Vignetting.h"
#include "Engine/Graphics/Model/Tiny/Json.hpp"
#include <fstream>

// ─── 静的メンバ定義 ───────────────────────────────────────────────
Framebuffer                  Post_Process_Manager::fsquad;
Framebuffer                  Post_Process_Manager::adaptation_input;
ID3D11ShaderResourceView*    Post_Process_Manager::final_color_map = nullptr;
std::array<Post_Process_Manager::EffectId, 13> Post_Process_Manager::effect_order = {
	EffectId::Sky, EffectId::VolumetricFog, EffectId::HeightFog, EffectId::DistanceFog,
	EffectId::ExponentialFog, EffectId::DoF, EffectId::Exposure, EffectId::ChromaticAberration,
	EffectId::LensDistortion, EffectId::Vignetting, EffectId::Bloom, EffectId::Adaptation, EffectId::ToneMapping
};
std::array<bool, 13> Post_Process_Manager::effect_enabled{};
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
	adaptation_input = Framebuffer(device, w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, false, false);
	effect_enabled.fill(true);

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

	// 保存済みのパイプライン設定があれば、エフェクト生成後に起動設定として適用する。
	Post_Process_Manager{}.loadPipelineSettings();
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
	ID3D11ShaderResourceView* current = fsquad.GetColorMap();
	final_color_map = current;
	for (size_t i = 0; i < effect_order.size(); ++i) {
		const size_t effect_index = static_cast<size_t>(effect_order[i]);
		if (!effect_enabled[effect_index]) continue;
		if (effect_order[i] == EffectId::Adaptation) {
			adaptation_input.Clear(ctx);
			adaptation_input.Activate(ctx);
			Graphics_Core::instance().get_fullscreen_quad()->Blit(ctx, &current, 0, 1);
			adaptation_input.Deactivate(ctx);
			adaptation_input.GenerateMips(ctx);
			adaptation->make(ctx, adaptation_input.GetColorMap());
			current = adaptation->get_color_map();
			continue;
		}
		switch (effect_order[i]) {
		case EffectId::Sky: skyer->make(ctx, current); current = skyer->get_color_map(); break;
		case EffectId::Bloom: bloomer->make(ctx, current); current = bloomer->getColorMap(); break;
		case EffectId::VolumetricFog: vol_fog->make(ctx, current); current = vol_fog->get_color_map(); break;
		case EffectId::HeightFog: hgt_fog->make(ctx, current); current = hgt_fog->get_color_map(); break;
		case EffectId::DistanceFog: dst_fog->make(ctx, current); current = dst_fog->get_color_map(); break;
		case EffectId::ExponentialFog: exp_fog->make(ctx, current); current = exp_fog->get_color_map(); break;
		case EffectId::DoF: dofer->make(ctx, current); current = dofer->GetColorMap(); break;
		case EffectId::Exposure: exposurer->make(ctx, current); current = exposurer->GetColorMap(); break;
		case EffectId::ChromaticAberration: ca_effect->make(ctx, current); current = ca_effect->GetColorMap(); break;
		case EffectId::LensDistortion: lens_distortion->make(ctx, current); current = lens_distortion->GetColorMap(); break;
		case EffectId::Vignetting: vignetting->make(ctx, current); current = vignetting->GetColorMap(); break;
		case EffectId::ToneMapping: tone_mapper->make(ctx, current); current = tone_mapper->get_color_map(); break;
		case EffectId::Adaptation: break;
		}
		final_color_map = current;
	}
}

// ─── 最終描画 ─────────────────────────────────────────────────────
void Post_Process_Manager::render()
{
	{
		DX_SCOPED_EVENT(&Graphics_Core::instance().g_MarkerUtil, L"Post-Process Final");
		Graphics_Core::instance().get_fullscreen_quad()->Blit(
			Graphics_Core::instance().get_device_context(),
			&final_color_map, 0, 1
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
void Post_Process_Manager::drawPipelineGUI()
{
	static const char* names[] = { "Sky", "Bloom", "Adaptation", "Volumetric Fog", "Height Fog", "Distance Fog", "Exponential Fog", "DoF", "Exposure", "Chromatic Aberration", "Lens Distortion", "Vignetting", "Tone Mapping" };
	ImGui::TextDisabled("The order below is the execution order.");
	if (ImGui::Button("Save Settings")) ImGui::SetTooltip(savePipelineSettings() ? "Saved to data/post_process_settings.json" : "Could not save settings.");
	ImGui::SameLine();
	if (ImGui::Button("Load Settings")) ImGui::SetTooltip(loadPipelineSettings() ? "Settings loaded." : "Could not load settings.");
	for (size_t i = 0; i < effect_order.size(); ++i) {
		const size_t effect = static_cast<size_t>(effect_order[i]);
		ImGui::PushID(static_cast<int>(i));
		ImGui::Checkbox("##enabled", &effect_enabled[effect]); ImGui::SameLine();
		ImGui::Text("%zu. %s", i + 1, names[effect]); ImGui::SameLine();
		if (ImGui::ArrowButton("##up", ImGuiDir_Up) && i > 0) std::swap(effect_order[i], effect_order[i - 1]);
		ImGui::SameLine();
		if (ImGui::ArrowButton("##down", ImGuiDir_Down) && i + 1 < effect_order.size()) std::swap(effect_order[i], effect_order[i + 1]);
		ImGui::PopID();
	}
}

bool Post_Process_Manager::savePipelineSettings() const
{
	using nlohmann::json;
	json j;
	j["version"] = 1;
	j["order"] = json::array();
	j["enabled"] = json::array();
	for (auto id : effect_order) j["order"].push_back(static_cast<int>(id));
	for (bool enabled : effect_enabled) j["enabled"].push_back(enabled);
	#define SAVE_PARAM(group, field, value) j[group][field] = value
	SAVE_PARAM("bloom", "enabled", bloomer->is_bloom); SAVE_PARAM("bloom", "threshold", bloomer->bloom_extraction_threshold); SAVE_PARAM("bloom", "intensity", bloomer->bloom_intensity);
	SAVE_PARAM("adaptation", "target_lum", adaptation->target_lum); SAVE_PARAM("adaptation", "speed_to_light", adaptation->speed_to_light); SAVE_PARAM("adaptation", "speed_to_dark", adaptation->speed_to_dark);
	SAVE_PARAM("tone_mapping", "enabled", tone_mapper->is_enabled); SAVE_PARAM("tone_mapping", "mapping_type", static_cast<int>(tone_mapper->mapping_type)); SAVE_PARAM("tone_mapping", "exposure", tone_mapper->exposure); SAVE_PARAM("tone_mapping", "gamma", tone_mapper->gamma); SAVE_PARAM("tone_mapping", "gt_param", tone_mapper->gt_param); SAVE_PARAM("tone_mapping", "max_white", tone_mapper->max_white); SAVE_PARAM("tone_mapping", "shoulder", tone_mapper->shoulder); SAVE_PARAM("tone_mapping", "linear_strength", tone_mapper->linear_strength); SAVE_PARAM("tone_mapping", "linear_angle", tone_mapper->linear_angle); SAVE_PARAM("tone_mapping", "toe_strength", tone_mapper->toe_strength);
	SAVE_PARAM("dof", "enabled", dofer->is_dof); SAVE_PARAM("exposure", "enabled", exposurer->is_enabled);
	SAVE_PARAM("chromatic_aberration", "enabled", ca_effect->is_enabled); SAVE_PARAM("chromatic_aberration", "intensity", ca_effect->intensity); SAVE_PARAM("chromatic_aberration", "physical_link", ca_effect->physical_link);
	SAVE_PARAM("lens_distortion", "enabled", lens_distortion->is_enabled); SAVE_PARAM("lens_distortion", "scale", lens_distortion->distortion_scale); SAVE_PARAM("lens_distortion", "physical_link", lens_distortion->physical_link);
	SAVE_PARAM("vignetting", "enabled", vignetting->is_enabled); SAVE_PARAM("vignetting", "physical_link", vignetting->physical_link); SAVE_PARAM("vignetting", "intensity", vignetting->intensity); SAVE_PARAM("vignetting", "inner_radius", vignetting->inner_radius); SAVE_PARAM("vignetting", "outer_radius", vignetting->outer_radius); SAVE_PARAM("vignetting", "smoothness", vignetting->smoothness);
	SAVE_PARAM("volumetric_fog", "enabled", vol_fog->config.is_enabled); SAVE_PARAM("volumetric_fog", "density", vol_fog->config.density_base); SAVE_PARAM("volumetric_fog", "scattering", vol_fog->config.scattering); SAVE_PARAM("volumetric_fog", "absorption", vol_fog->config.absorption); SAVE_PARAM("volumetric_fog", "anisotropy", vol_fog->config.anisotropy); SAVE_PARAM("volumetric_fog", "noise_scale", vol_fog->config.noise_scale); SAVE_PARAM("volumetric_fog", "intensity", vol_fog->config.intensity); SAVE_PARAM("volumetric_fog", "fog_near", vol_fog->config.fog_near); SAVE_PARAM("volumetric_fog", "fog_far", vol_fog->config.fog_far);
	SAVE_PARAM("volumetric_fog", "grid_width", vol_fog->config.grid_width); SAVE_PARAM("volumetric_fog", "grid_height", vol_fog->config.grid_height); SAVE_PARAM("volumetric_fog", "grid_depth", vol_fog->config.grid_depth);
	SAVE_PARAM("height_fog", "enabled", hgt_fog->config.is_enabled); SAVE_PARAM("height_fog", "base_height", hgt_fog->config.base_height); SAVE_PARAM("height_fog", "falloff", hgt_fog->config.falloff); SAVE_PARAM("height_fog", "density", hgt_fog->config.density_max); SAVE_PARAM("height_fog", "intensity", hgt_fog->config.intensity); SAVE_PARAM("height_fog", "noise_scale", hgt_fog->config.noise_scale); SAVE_PARAM("height_fog", "noise_strength", hgt_fog->config.noise_strength); SAVE_PARAM("height_fog", "wind_velocity", json::array({hgt_fog->config.wind_velocity.x, hgt_fog->config.wind_velocity.y, hgt_fog->config.wind_velocity.z})); SAVE_PARAM("height_fog", "color", json::array({hgt_fog->config.fog_color.x, hgt_fog->config.fog_color.y, hgt_fog->config.fog_color.z}));
	SAVE_PARAM("distance_fog", "enabled", dst_fog->config.is_enabled); SAVE_PARAM("distance_fog", "start", dst_fog->config.fog_start); SAVE_PARAM("distance_fog", "end", dst_fog->config.fog_end); SAVE_PARAM("distance_fog", "density", dst_fog->config.density); SAVE_PARAM("distance_fog", "intensity", dst_fog->config.intensity); SAVE_PARAM("distance_fog", "color", json::array({dst_fog->config.fog_color[0], dst_fog->config.fog_color[1], dst_fog->config.fog_color[2]}));
	SAVE_PARAM("exponential_fog", "enabled", exp_fog->config.is_enabled); SAVE_PARAM("exponential_fog", "density", exp_fog->config.density); SAVE_PARAM("exponential_fog", "intensity", exp_fog->config.intensity); SAVE_PARAM("exponential_fog", "mode", exp_fog->config.mode); SAVE_PARAM("exponential_fog", "color", json::array({exp_fog->config.fog_color[0], exp_fog->config.fog_color[1], exp_fog->config.fog_color[2]}));
	#undef SAVE_PARAM
	std::ofstream file("data/post_process_settings.json");
	if (!file) return false;
	file << j.dump(2);
	return file.good();
}

bool Post_Process_Manager::loadPipelineSettings()
{
	using nlohmann::json;
	std::ifstream file("data/post_process_settings.json");
	if (!file) return false;
	json j;
	try { file >> j; } catch (...) { return false; }
	if (!j.is_object() || j.value("version", 0) != 1) return false;
	if (j.contains("order") && j["order"].is_array() && j["order"].size() == effect_order.size()) {
		std::array<bool, 13> seen{}; bool valid = true;
		for (size_t i = 0; i < effect_order.size(); ++i) { int id = j["order"][i].get<int>(); if (id < 0 || id >= 13 || seen[id]) { valid = false; break; } seen[id] = true; effect_order[i] = static_cast<EffectId>(id); }
		if (!valid) return false;
	}
	if (j.contains("enabled") && j["enabled"].is_array() && j["enabled"].size() == effect_enabled.size()) for (size_t i = 0; i < effect_enabled.size(); ++i) effect_enabled[i] = j["enabled"][i].get<bool>();
	#define LOAD_PARAM(group, field, target) do { if (j.contains(group) && j[group].is_object()) target = j[group].value(field, target); } while (false)
	LOAD_PARAM("bloom", "enabled", bloomer->is_bloom); LOAD_PARAM("bloom", "threshold", bloomer->bloom_extraction_threshold); LOAD_PARAM("bloom", "intensity", bloomer->bloom_intensity);
	LOAD_PARAM("adaptation", "target_lum", adaptation->target_lum); LOAD_PARAM("adaptation", "speed_to_light", adaptation->speed_to_light); LOAD_PARAM("adaptation", "speed_to_dark", adaptation->speed_to_dark);
	LOAD_PARAM("tone_mapping", "enabled", tone_mapper->is_enabled); { int type = static_cast<int>(tone_mapper->mapping_type); LOAD_PARAM("tone_mapping", "mapping_type", type); if (type >= 0 && type < 34) tone_mapper->mapping_type = static_cast<ToneMapping::ToneMappingType>(type); } LOAD_PARAM("tone_mapping", "exposure", tone_mapper->exposure); LOAD_PARAM("tone_mapping", "gamma", tone_mapper->gamma); LOAD_PARAM("tone_mapping", "gt_param", tone_mapper->gt_param); LOAD_PARAM("tone_mapping", "max_white", tone_mapper->max_white); LOAD_PARAM("tone_mapping", "shoulder", tone_mapper->shoulder); LOAD_PARAM("tone_mapping", "linear_strength", tone_mapper->linear_strength); LOAD_PARAM("tone_mapping", "linear_angle", tone_mapper->linear_angle); LOAD_PARAM("tone_mapping", "toe_strength", tone_mapper->toe_strength);
	LOAD_PARAM("dof", "enabled", dofer->is_dof); LOAD_PARAM("exposure", "enabled", exposurer->is_enabled);
	LOAD_PARAM("chromatic_aberration", "enabled", ca_effect->is_enabled); LOAD_PARAM("chromatic_aberration", "intensity", ca_effect->intensity); LOAD_PARAM("chromatic_aberration", "physical_link", ca_effect->physical_link);
	LOAD_PARAM("lens_distortion", "enabled", lens_distortion->is_enabled); LOAD_PARAM("lens_distortion", "scale", lens_distortion->distortion_scale); LOAD_PARAM("lens_distortion", "physical_link", lens_distortion->physical_link);
	LOAD_PARAM("vignetting", "enabled", vignetting->is_enabled); LOAD_PARAM("vignetting", "physical_link", vignetting->physical_link); LOAD_PARAM("vignetting", "intensity", vignetting->intensity); LOAD_PARAM("vignetting", "inner_radius", vignetting->inner_radius); LOAD_PARAM("vignetting", "outer_radius", vignetting->outer_radius); LOAD_PARAM("vignetting", "smoothness", vignetting->smoothness);
	LOAD_PARAM("volumetric_fog", "enabled", vol_fog->config.is_enabled); LOAD_PARAM("volumetric_fog", "density", vol_fog->config.density_base); LOAD_PARAM("volumetric_fog", "scattering", vol_fog->config.scattering); LOAD_PARAM("volumetric_fog", "absorption", vol_fog->config.absorption); LOAD_PARAM("volumetric_fog", "anisotropy", vol_fog->config.anisotropy); LOAD_PARAM("volumetric_fog", "noise_scale", vol_fog->config.noise_scale); LOAD_PARAM("volumetric_fog", "intensity", vol_fog->config.intensity); LOAD_PARAM("volumetric_fog", "fog_near", vol_fog->config.fog_near); LOAD_PARAM("volumetric_fog", "fog_far", vol_fog->config.fog_far); LOAD_PARAM("volumetric_fog", "grid_width", vol_fog->config.grid_width); LOAD_PARAM("volumetric_fog", "grid_height", vol_fog->config.grid_height); LOAD_PARAM("volumetric_fog", "grid_depth", vol_fog->config.grid_depth);
	LOAD_PARAM("height_fog", "enabled", hgt_fog->config.is_enabled); LOAD_PARAM("height_fog", "base_height", hgt_fog->config.base_height); LOAD_PARAM("height_fog", "falloff", hgt_fog->config.falloff); LOAD_PARAM("height_fog", "density", hgt_fog->config.density_max); LOAD_PARAM("height_fog", "intensity", hgt_fog->config.intensity); LOAD_PARAM("height_fog", "noise_scale", hgt_fog->config.noise_scale); LOAD_PARAM("height_fog", "noise_strength", hgt_fog->config.noise_strength); { auto color = j.value("height_fog", json::object()).value("color", json::array()); if (color.is_array() && color.size() == 3) hgt_fog->config.fog_color = DirectX::XMFLOAT3(color[0].get<float>(), color[1].get<float>(), color[2].get<float>()); auto wind = j.value("height_fog", json::object()).value("wind_velocity", json::array()); if (wind.is_array() && wind.size() == 3) hgt_fog->config.wind_velocity = DirectX::XMFLOAT3(wind[0].get<float>(), wind[1].get<float>(), wind[2].get<float>()); }
	LOAD_PARAM("distance_fog", "enabled", dst_fog->config.is_enabled); LOAD_PARAM("distance_fog", "start", dst_fog->config.fog_start); LOAD_PARAM("distance_fog", "end", dst_fog->config.fog_end); LOAD_PARAM("distance_fog", "density", dst_fog->config.density); LOAD_PARAM("distance_fog", "intensity", dst_fog->config.intensity); { auto color = j.value("distance_fog", json::object()).value("color", json::array()); if (color.is_array() && color.size() == 3) for (int i = 0; i < 3; ++i) dst_fog->config.fog_color[i] = color[i].get<float>(); }
	LOAD_PARAM("exponential_fog", "enabled", exp_fog->config.is_enabled); LOAD_PARAM("exponential_fog", "density", exp_fog->config.density); LOAD_PARAM("exponential_fog", "intensity", exp_fog->config.intensity); LOAD_PARAM("exponential_fog", "mode", exp_fog->config.mode); { auto color = j.value("exponential_fog", json::object()).value("color", json::array()); if (color.is_array() && color.size() == 3) for (int i = 0; i < 3; ++i) exp_fog->config.fog_color[i] = color[i].get<float>(); }
	#undef LOAD_PARAM
	return true;
}

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
