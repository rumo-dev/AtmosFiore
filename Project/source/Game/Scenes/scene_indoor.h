#pragma once
#include "scene_base.h"
#include "Engine/Graphics/UI/sprite/sprite.h"
#include "Engine/system/graphics_core.h"

#include "Engine/Graphics/UI/text/text.h"
#include "Engine/System/Manager/resource_manager.h"
#include "Engine/System/Manager/PostProcess/post_process_manager.h"

#include "Engine/Graphics/Model/gltf_model.h"
#include "Game/World/Light/directional_light.h"
#include "Engine/utilities/dx_common.h"

#include "Engine/utilities/color_util.h"
#include "Engine/Graphics/IBL/ibl.h"
#include "Game/Effect/VoxelGI/VoxelGI.h"
#include "Game/Effect/DDGI/DDGI.h"
#include "Game/World/Player/player.h"
#include <array>

class ThirdPersonCamera;

// ゲームシーン
class Scene_Indoor : public Scene_Base
{
public:
	Scene_Indoor() {}
	~Scene_Indoor() override {}

	// 初期化
	void initialize() override;

	// 終了化
	void finalize() override;

	// 更新処理
	void update(float elapsedTime) override;

	// 描画処理
	void render(float elapsedTime) override;

	void render_shadowmap(float elapsedTime);

	void render_defferd(float elapsedTime);

	void render_forward(float elapsedTime);

	void render_UI(float elapsedTime);

	void render_debug(float elapsedTime);

	// GUI描画
	void draw_gui() override;

private:
	static constexpr unsigned int ReflectionProbeCount = 3;
	struct ReflectionProbe
	{
		DirectX::XMFLOAT4 position{};
		DirectX::XMFLOAT4 minimum{};
		DirectX::XMFLOAT4 maximum{};
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> faces[6];
		bool dirty = true;
		bool ready = false;
		UINT next_face = 0;
	};
	struct ReflectionProbeConstants
	{
		DirectX::XMFLOAT4 positions[ReflectionProbeCount]{};
		DirectX::XMFLOAT4 minima[ReflectionProbeCount]{};
		DirectX::XMFLOAT4 maxima[ReflectionProbeCount]{};
		DirectX::XMFLOAT4 settings{};
	};
	bool initialize_reflection_probes(ID3D11Device* device);
	void capture_reflection_probe(ID3D11DeviceContext* context);
	void bind_reflection_probes(ID3D11DeviceContext* context);
	void draw_reflection_probe_gui();
	std::array<ReflectionProbe, ReflectionProbeCount> _reflection_probes;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> _reflection_probe_depth;
	Microsoft::WRL::ComPtr<ID3D11Buffer> _reflection_probe_constants;
	bool _reflection_probes_available = false;
	bool _reflection_probes_enabled = true;
	float _reflection_probe_edge_fade = 1.5f;
	float _reflection_probe_strength = 0.65f;
	enum class Sprite_Type
	{
		Default,
		Texel,
		Num,
	};
	std::unique_ptr<Gltf_Model> _gltf_model;
	std::unique_ptr<VoxelGI> _voxel_gi;
	std::unique_ptr<DDGI> _ddgi;
	DirectX::XMFLOAT4X4 _indoor_world{};
	DirectionalLight _directional_light;

	/// プレイヤー
	Player _player;
	std::shared_ptr<ThirdPersonCamera> _third_person_camera;
};
