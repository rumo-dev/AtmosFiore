#include "resource_manager.h"
#include "Engine/utilities/misc.h"
void Resource_Manager::load_all() {
	log_printf("リソースマネージャー: 全リソースの読み込み開始\n", LogLevel::Info);
	load_shaders();
	log_printf("リソースマネージャー: 全リソースの読み込み終了\n", LogLevel::Info);
}
void Resource_Manager::load_shaders() {
	log_printf("シェーダーの読み込み開始\n", LogLevel::Info);
	shader_manager.SetShaderFolders(
		L"./source/Engine/Graphics/Shader/",
		L"./data/shaders/cso/"
	);
	//シェーダーがない場合
	shader_manager.load<Pixel_Shader>(
		"ERROR",
		"error_ps"
	);
	shader_manager.load<Pixel_Shader>(
		"GLTF_PS",
		"gltf_model_ps"
	);

	shader_manager.load<Vertex_Shader>(
		"GLTF_VS",
		"gltf_model_vs"
	);
	shader_manager.load<Vertex_Shader>(
		"POINT_SHADOW_VS",
		"point_shadow_vs"
	);
	shader_manager.load<Pixel_Shader>(
		"POINT_SHADOW_PS",
		"point_shadow_ps"
	);

	shader_manager.load<Pixel_Shader>(
		"FOG_PS",
		"fog_ps"
	);

	//gltf
	shader_manager.load<Pixel_Shader>(
		"GLTF_DEFERRED_GEOMETRY_PS",
		"gltf_deferred_geometry_ps"
	);
	shader_manager.load<Pixel_Shader>(
		"GLTF_FORWARD_OPAQUE_PS",
		"gltf_forward_opaque_ps"
	);
	shader_manager.load<Pixel_Shader>(
		"GLTF_FORWARD_TRANSPARENCY_PS",
		"gltf_forward_transparency_ps"
	);
	//ディファードライティング
	shader_manager.load<Pixel_Shader>(
		"DEFERRED_LIGHTING_PS",
		"deferred_lighting_ps"
	);
	//ssao
	shader_manager.load<Pixel_Shader>(
		"SSAO_PS",
		"ssao_ps"
	);
	//shadow
	shader_manager.load<Pixel_Shader>(
		"SHADOW_PS",
		"shadow_ps"
	);

	shader_manager.load<Pixel_Shader>(
		"ADAPTATION_PS",
		"adaptation_ps"
	);
	shader_manager.load<Compute_Shader>(
		"ADAPTATION_CS",
		"adaptation_cs"
	);
	shader_manager.load<Pixel_Shader>(
		"TONE_MAPPING_PS",
		"tone_mapping_ps"
	);

	log_printf("シェーダーの読み込み終了\n", LogLevel::Info);


	load_models();
}

void Resource_Manager::load_models() {
	log_printf("モデルの読み込み開始\n", LogLevel::Info);
	model_manager.load(
		"DamagedHelmet",
		"./data/model/glTF/DamagedHelmet/DamagedHelmet.gltf"
	);
	//model_manager.load(
	//	"Sponza",
	//	"./data/model/Sponza/Sponza.gltf"
	//);
	//model_manager.load(
	//	"BMW_M4_DTM",
	//	"./data/model/bmw_m4_dtm.glb"
	//);
	model_manager.load(
		"Library",
		//"./data/model/Library/library.gltf"
		"./data/model/Library/library_OneMesh.gltf"
	);
	//model_manager.load(
	//	"Scene",
	//	"./data/model/Scene/Scene.gltf"
	//);
	//model_manager.load(
	//	"Cafe",
	//	"./data/model/cafe/scene.gltf"
	//);
	model_manager.load(
		"Cafe_Anime",
		"./data/model/cafe_anime/scene.gltf"
	);

	//model_manager.load(
	//	"Warehouse_FBX_Model_Free",
	//	"./data/model/warehouse_fbx_model_free/scene.gltf"
	//);
	//model_manager.load(
	//	"Free_Atlanta_Corperate_Office_Building",
	//	"./data/model/free__atlanta_corperate_office_building/scene.gltf"
	//);
	//model_manager.load(
	//	"Old_Church_Modeling_Interior_Scene",
	//	"./data/model/old_church_modeling_-_interior_scene/scene.gltf"
	//);

	log_printf("モデルの読み込み終了\n", LogLevel::Info);
}
