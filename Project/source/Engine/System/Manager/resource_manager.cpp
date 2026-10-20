#include "resource_manager.h"
#include "Engine/utilities/misc.h"
#include "Engine/System/graphics_core.h"
#include "Engine/Graphics/Texture/texture.h"

Resource_Manager::Resource_Manager() {
	shader_manager_.SetShaderFolders(
		L"./source/Engine/Graphics/Shader/",
		L"./data/shaders/cso/");
}

HRESULT Resource_Manager::load_texture(const std::wstring& path,
	ID3D11ShaderResourceView** view, D3D11_TEXTURE2D_DESC* desc) {
	ID3D11Device* device = Graphics_Core::instance().get_device();
	if (!view || !device) return E_INVALIDARG;
	D3D11_TEXTURE2D_DESC localDesc{};
	return load_texture_from_file(device, path.c_str(), view, desc ? desc : &localDesc);
}

HRESULT Resource_Manager::load_texture_memory(const void* data, size_t size,
	ID3D11ShaderResourceView** view) {
	ID3D11Device* device = Graphics_Core::instance().get_device();
	if (!view || !device || !data || size == 0) return E_INVALIDARG;
	return load_texture_from_memory(device, data, size, view);
}

size_t Resource_Manager::unload_unused() {
	const size_t shader_count = shader_manager_.UnloadUnused();
	const size_t model_count = model_manager_.unload_unused();
	const size_t texture_count = release_unused_textures();
	const size_t removed = shader_count + model_count + texture_count;
	log_printf("リソースマネージャー: 未使用リソースを %zu 件解放しました\n", LogLevel::Info, removed);
	return removed;
}

void Resource_Manager::unload_all() {
	shader_manager_.UnloadAll();
	model_manager_.remove_all_instances();
	model_manager_.unload_all();
	release_all_textures();
}

void Resource_Manager::load_all() {
	log_printf("リソースマネージャー: 全リソースの読み込み開始\n", LogLevel::Info);
	load_shaders();
	load_models();
	log_printf("リソースマネージャー: 全リソースの読み込み終了\n", LogLevel::Info);
}
void Resource_Manager::load_shaders() {
	if (shaders_loaded_) return;
	log_printf("シェーダーの読み込み開始\n", LogLevel::Info);
	shader_manager_.SetShaderFolders(
		L"./source/Engine/Graphics/Shader/",
		L"./data/shaders/cso/"
	);
	//シェーダーがない場合
	load_shader<Pixel_Shader>(
		"ERROR",
		"error_ps"
	);
	load_shader<Pixel_Shader>(
		"GLTF_PS",
		"gltf_model_ps"
	);

	load_shader<Vertex_Shader>(
		"GLTF_VS",
		"gltf_model_vs"
	);
	load_shader<Vertex_Shader>(
		"POINT_SHADOW_VS",
		"point_shadow_vs"
	);
	load_shader<Vertex_Shader>(
		"DIRECTIONAL_SHADOW_VS",
		"directional_shadow_vs"
	);
	load_shader<Pixel_Shader>(
		"POINT_SHADOW_PS",
		"point_shadow_ps"
	);

	load_shader<Pixel_Shader>(
		"FOG_PS",
		"fog_ps"
	);
	load_shader<Pixel_Shader>(
		"SKY_PS",
		"sky_ps"
	);

	//gltf
	load_shader<Pixel_Shader>(
		"GLTF_DEFERRED_GEOMETRY_PS",
		"gltf_deferred_geometry_ps"
	);
	load_shader<Pixel_Shader>(
		"GLTF_FORWARD_OPAQUE_PS",
		"gltf_forward_opaque_ps"
	);
	load_shader<Pixel_Shader>(
		"GLTF_FORWARD_TRANSPARENCY_PS",
		"gltf_forward_transparency_ps"
	);
	//ディファードライティング
	load_shader<Pixel_Shader>(
		"DEFERRED_LIGHTING_PS",
		"deferred_lighting_ps"
	);
	//ssao
	load_shader<Pixel_Shader>(
		"SSAO_PS",
		"ssao_ps"
	);
	load_shader<Pixel_Shader>(
		"SSAO_BILATERAL_BLUR_PS",
		"ssao_bilateral_blur_ps"
	);
	load_shader<Pixel_Shader>(
		"SSAO_UPSAMPLE_PS",
		"ssao_upsample_ps"
	);
	//shadow
	load_shader<Pixel_Shader>(
		"SHADOW_PS",
		"shadow_ps"
	);

	load_shader<Pixel_Shader>(
		"ADAPTATION_PS",
		"adaptation_ps"
	);
	load_shader<Compute_Shader>(
		"ADAPTATION_CS",
		"adaptation_cs"
	);
	load_shader<Pixel_Shader>(
		"DOF_COC",
		"coc_generation_ps.cso"
	);
	load_shader<Pixel_Shader>(
		"DOF_NEAR_SEPARATE",
		"field_separation_near_ps.cso"
	);
	load_shader<Pixel_Shader>(
		"DOF_FAR_SEPARATE",
		"field_separation_far_ps.cso"
	);
	load_shader<Pixel_Shader>(
		"DOF_FFS_HORIZONTAL",
		"ffs_horizontal_ps.cso"
	);
	load_shader<Pixel_Shader>(
		"DOF_FFS_VERTICAL",
		"ffs_vertical_ps.cso"
	);
	load_shader<Pixel_Shader>(
		"DOF_COMP",
		"composite_ps.cso"
	);

	// Register every shader referenced by a static CSO name in one place.
	// File based loading remains inside ShaderManager and is cached by source file.
	load_shader<Pixel_Shader>("BLOOM_FINAL_PS", "bloomfinal.cso");
	load_shader<Pixel_Shader>("CHROMATIC_ABERRATION_PS", "chromatic_aberration_ps.cso");
	load_shader<Pixel_Shader>("DEBUG_PRIMITIVE_PS", "debug_primitive_ps.cso");
	load_shader<Vertex_Shader>("DEBUG_PRIMITIVE_VS", "debug_primitive_vs.cso");
	load_shader<Pixel_Shader>("DISTANCE_FOG_PS", "distance_fog_ps.cso");
	load_shader<Pixel_Shader>("EXPONENTIAL_FOG_PS", "exponential_fog_ps.cso");
	load_shader<Pixel_Shader>("EXPOSURE_PS", "exposure_ps.cso");
	load_shader<Compute_Shader>("DOF_FFS_PREFIX_H_CS", "ffs_prefix_h_cs.cso");
	load_shader<Compute_Shader>("DOF_FFS_PREFIX_V_CS", "ffs_prefix_v_cs.cso");
	load_shader<Compute_Shader>("DOF_FFS_RESOLVE_CS", "ffs_resolve_cs.cso");
	load_shader<Compute_Shader>("DOF_FFS_SCATTER_CS", "ffs_scatter_cs.cso");
	load_shader<Vertex_Shader>("FULLSCREEN_QUAD_VS", "fullscreen_quad_vs.cso");
	load_shader<Pixel_Shader>("FULLSCREEN_QUAD_PS", "fullscreen_quad_ps.cso");
	load_shader<Pixel_Shader>("GAUSSIAN_BLUR_DOWNSAMPLING_PS", "gaussian_blur_downsampling_ps.cso");
	load_shader<Pixel_Shader>("GAUSSIAN_BLUR_HORIZONTAL_PS", "gaussian_blur_horizontal_ps.cso");
	load_shader<Pixel_Shader>("GAUSSIAN_BLUR_UPSAMPLING_PS", "gaussian_blur_upsampling_ps.cso");
	load_shader<Pixel_Shader>("GAUSSIAN_BLUR_VERTICAL_PS", "gaussian_blur_vertical_ps.cso");
	load_shader<Pixel_Shader>("GLOW_EXTRACTION_PS", "glow_extraction_ps.cso");
	load_shader<Pixel_Shader>("HEIGHT_FOG_PS", "height_fog_ps.cso");
	load_shader<Pixel_Shader>("LENS_DISTORTION_PS", "lens_distortion_ps.cso");
	load_shader<Pixel_Shader>("LOADING_BACKGROUND_PS", "loading_background_ps.cso");
	load_shader<Pixel_Shader>("OBJ_MODEL_PS", "obj_model_ps.cso");
	load_shader<Vertex_Shader>("OBJ_MODEL_VS", "obj_model_vs.cso");
	load_shader<Pixel_Shader>("SPRITE_PS", "sprite_ps.cso");
	load_shader<Vertex_Shader>("SPRITE_VS", "sprite_vs.cso");
	load_shader<Pixel_Shader>("TITLE_BACKGROUND_PS", "title_background_ps.cso");
	load_shader<Pixel_Shader>("TONEMAPPING_PS", "tonemapping_ps.cso");
	load_shader<Pixel_Shader>("TRANSITION_PS", "transition_ps.cso");
	load_shader<Pixel_Shader>("VIGNETTING_PS", "vignetting_ps.cso");
	load_shader<Compute_Shader>("VOLUMETRIC_FOG_ACCUMULATION_CS", "volumetric_fog_accumulation_cs.cso");
	load_shader<Compute_Shader>("VOLUMETRIC_FOG_INJECTION_CS", "volumetric_fog_injection_cs.cso");
	load_shader<Pixel_Shader>("VOLUMETRIC_FOG_PS", "volumetric_fog_ps.cso");
	shaders_loaded_ = true;
	log_printf("シェーダーの読み込み終了\n", LogLevel::Info);
}

void Resource_Manager::load_models() {
	log_printf("モデルの読み込み開始\n", LogLevel::Info);
	//エラー用ダミー
	load_model(
		"Error",
		"./data/model/Error/Scene.gltf"
	);
	//load_model(
	//	"DamagedHelmet",
	//	"./data/model/glTF/DamagedHelmet/DamagedHelmet.gltf"
	//);
	//load_model(
	//	"Sponza",
	//	"./data/model/Sponza/Sponza.gltf"
	//);
	//load_model(
	//	"BMW_M4_DTM",
	//	"./data/model/bmw_m4_dtm.glb"
	//);
	load_model(
		"Library",
		//"./data/model/Library/library.gltf"
		"./data/model/Library/library_OneMesh.gltf"
	);

	//load_model(
	//	"Scene",
	//	"./data/model/Scene/Scene.gltf"
	//);
	load_model(
		"Spider",
		"./data/model/drone.glb"
	);
	//load_model(
	//	"Spider1",
	//	"./data/model/drone2.glb"
	//);
	//load_model(
	//	"Spider2",
	//	"./data/model/drone3.glb"
	//);
	//load_model(
	//	"Spider3",
	//	"./data/model/drone4.glb"
	//);
	//load_model(
	//	"Cafe",
	//	"./data/model/cafe/scene.gltf"
	//);
	//load_model(
	//	"Cafe_Anime",
	//	"./data/model/cafe_anime/scene.gltf"
	//);
	//load_model(
	//	"City",
	//	"./data/model/City/scene.gltf"
	//);

	//load_model(
	//	"Warehouse_FBX_Model_Free",
	//	"./data/model/warehouse_fbx_model_free/scene.gltf"
	//);
	//load_model(
	//	"Free_Atlanta_Corperate_Office_Building",
	//	"./data/model/free__atlanta_corperate_office_building/scene.gltf"
	//);
	//load_model(
	//	"Old_Church_Modeling_Interior_Scene",
	//	"./data/model/old_church_modeling_-_interior_scene/scene.gltf"
	//);

	log_printf("モデルの読み込み終了\n", LogLevel::Info);
}
