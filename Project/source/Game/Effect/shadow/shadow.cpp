// BLOOM
#include "shadow.h"
#include "Engine/System/Manager/resource_manager.h"
#include "Engine/Graphics/geometry_buffer/geometry_buffer.h"
#include "Game/World/camera/camera_manager.h"

#include <vector>
#include <algorithm>
#include "Engine/system/render_state.h"
#include "Engine/System/graphics_core.h"

#include "tracy_util.h"

shadow::shadow(ID3D11Device* device, uint32_t width, uint32_t height)
{
	TRACY_CPU_ZONE_C("shadow::shadow", TracyCategory::System);

	// SHADOW MAPS
	point_shadow_front = std::make_unique<shadow_map>(device, shadowmap_width, shadowmap_height);
	point_shadow_back = std::make_unique<shadow_map>(device, shadowmap_width, shadowmap_height);
	directional_shadow_map = std::make_unique<shadow_map>(device, shadowmap_width, shadowmap_height);

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(Point_Shadow_Constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	HRESULT hr = device->CreateBuffer(&buffer_desc, nullptr, point_shadow_constant_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	bit_block_transfer = std::make_unique<FullscreenQuad>(device);

	shaded = std::make_unique<Framebuffer>(
		device,
		width,
		height,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		false
	);
}

// 最終的なシャドウ合成 (コンポジット) パス
void shadow::make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map)
{
	TRACY_CPU_ZONE_C("shadow::make", TracyCategory::Shadow); // 合成パス
	TRACY_GPU_ZONE_C("shadow::make", TracyCategory::Shadow);

	// 現在のステートをキャッシュ
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> cached_depth_stencil_state;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> cached_rasterizer_state;
	Microsoft::WRL::ComPtr<ID3D11BlendState> cached_blend_state;
	FLOAT blend_factor[4];
	UINT sample_mask;

	immediate_context->OMGetDepthStencilState(cached_depth_stencil_state.GetAddressOf(), 0);
	immediate_context->RSGetState(cached_rasterizer_state.GetAddressOf());
	immediate_context->OMGetBlendState(cached_blend_state.GetAddressOf(), blend_factor, &sample_mask);

	Microsoft::WRL::ComPtr<ID3D11Buffer> cached_constant_buffer;
	immediate_context->PSGetConstantBuffers(8, 1, cached_constant_buffer.GetAddressOf());

	// 描画ステートの設定
	Render_State::instance().set_2d_render_states(immediate_context);

	// レンダーターゲットのクリアと有効化
	{
		TRACY_CPU_ZONE_N("shadow::make::CompositePass");

		shaded->Clear(immediate_context, 1, 1, 1, 1);
		shaded->Activate(immediate_context);
		{
			ID3D11ShaderResourceView* shader_resource_views[GBUFFER_COUNT + 2]{};

			// G-Buffer の SRV をバインド用配列へコピー
			Graphics_Core::instance().get_geometry_buffer()->GetShaderResourceViews(
				shader_resource_views,
				GBUFFER_COUNT,
				0
			);
			shader_resource_views[4] = color_map;
			shader_resource_views[5] = directional_shadow_map->get_depth_map();

			// フルスクリーンクアッドで描画（シェーダーを走らせて合成）
			bit_block_transfer->Blit(
				immediate_context,
				shader_resource_views,
				0,
				6,
				Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("SHADOW_PS")
			);
		}
		shaded->Deactivate(immediate_context);
	}

	// ステートを元に戻す
	immediate_context->PSSetConstantBuffers(8, 1, cached_constant_buffer.GetAddressOf());
	immediate_context->OMSetDepthStencilState(cached_depth_stencil_state.Get(), 0);
	immediate_context->RSSetState(cached_rasterizer_state.Get());
	immediate_context->OMSetBlendState(cached_blend_state.Get(), blend_factor, sample_mask);
}

// ディレクショナルライトのシャドウマップ生成開始
void shadow::make_directional_shadow_begin()
{
	TRACY_CPU_ZONE_C("shadow::make_directional_shadow_begin", TracyCategory::Shadow);

	using namespace DirectX;

	XMFLOAT4X4 VP;

	// カメラオブジェクトの毎フレームコピーを防止するため const 参照(&) で受ける
	const Camera& cam = CameraManager::instance().get_active_camera()->get_camera();

	const float aspect_ratio =
		directional_shadow_map->viewport.Width /
		directional_shadow_map->viewport.Height;

	// カメラの位置だけを使用（向きは無視）
	XMVECTOR F = cam.position;

	XMFLOAT4 directional_light_direction =
		Graphics_Core::instance().get_directional_light_direction();

	XMVECTOR lightDir =
		XMVector3Normalize(XMLoadFloat4(&directional_light_direction));

	// ライト方向に離れた位置からカメラ位置を見る
	XMVECTOR E = F - lightDir * light_view_distance;

	XMMATRIX V = XMMatrixLookAtLH(
		E,
		F,
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
	);

	XMMATRIX P = XMMatrixOrthographicLH(
		light_view_size * aspect_ratio,
		light_view_size,
		light_view_near_z,
		light_view_far_z
	);

	XMStoreFloat4x4(&VP, V * P);

	// カリング・描画用に保存
	last_light_view_projection = VP;
	light_view_projection = VP;

	active_shadow_map = directional_shadow_map.get();

	auto* dc = Graphics_Core::instance().get_device_context();
	active_shadow_map->clear(dc, 1.0f);
	active_shadow_map->activate(dc);
}

// ポイントライトのシャドウマップ生成開始
void shadow::make_shadow_begin(PointShadowFace face)
{
	TRACY_CPU_ZONE_C("shadow::make_shadow_begin", TracyCategory::Shadow);

	using namespace DirectX;

	ID3D11DeviceContext* context = Graphics_Core::instance().get_device_context();
	const auto& lights = Graphics_Core::instance().get_point_light_manager().get_lights();

	Point_Shadow_Constants constants{};
	if (!lights.empty())
	{
		const auto& light = lights.front();
		const float shadow_far = light.radius > light_view_near_z ? light.radius : light_view_far_z;
		constants.position_radius = { light.position.x, light.position.y, light.position.z, light.radius };
		constants.params = {
			light_view_near_z,
			shadow_far,
			face == PointShadowFace::Front ? 1.0f : -1.0f,
			point_shadow_bias
		};
		constants.options = { point_shadow_strength, static_cast<float>(point_shadow_enabled), 0.0f, 0.0f };
	}

	{
		TRACY_CB_ZONE("shadow::make_shadow_begin::UpdatePointShadowCB");

		context->UpdateSubresource(point_shadow_constant_buffer.Get(), 0, 0, &constants, 0, 0);
		context->VSSetConstantBuffers(5, 1, point_shadow_constant_buffer.GetAddressOf());
		context->PSSetConstantBuffers(5, 1, point_shadow_constant_buffer.GetAddressOf());
	}

	// キューブマップの各面(6面分)のView-Projection行列を計算
	if (!lights.empty())
	{
		TRACY_CPU_ZONE_N("shadow::make_shadow_begin::CalcCubeFaceViewProj");

		const auto& light = lights.front();
		XMVECTOR pos = XMLoadFloat3(&light.position);

		// +X, -X, +Y, -Y, +Z, -Z
		XMVECTOR targets[6] = {
			XMVectorAdd(pos, XMVectorSet(1,0,0,0)),  XMVectorAdd(pos, XMVectorSet(-1,0,0,0)),
			XMVectorAdd(pos, XMVectorSet(0,1,0,0)),  XMVectorAdd(pos, XMVectorSet(0,-1,0,0)),
			XMVectorAdd(pos, XMVectorSet(0,0,1,0)),  XMVectorAdd(pos, XMVectorSet(0,0,-1,0))
		};
		XMVECTOR ups[6] = {
			XMVectorSet(0,1,0,0),  XMVectorSet(0,1,0,0),  XMVectorSet(0,0,-1,0),  XMVectorSet(0,0,1,0),  XMVectorSet(0,1,0,0),  XMVectorSet(0,1,0,0)
		};

		float aspect = 1.0f; // 正方形
		float fov = DirectX::XMConvertToRadians(90.0f);
		float nearz = light_view_near_z;
		float farz = (light_view_far_z > light.radius) ? light_view_far_z : light.radius;

		for (int i = 0; i < 6; ++i)
		{
			XMMATRIX V = XMMatrixLookAtLH(pos, targets[i], ups[i]);
			XMMATRIX P = XMMatrixPerspectiveFovLH(fov, aspect, nearz, farz);
			XMStoreFloat4x4(&point_face_viewproj[i], V * P);
		}
	}

	current_point_face_group = face;

	active_shadow_map = (face == PointShadowFace::Front) ? point_shadow_front.get() : point_shadow_back.get();
	active_shadow_map->clear(context, 1.0f);
	active_shadow_map->activate(context);
}

// シャドウマップ描画の終了処理
void shadow::make_shadow_end()
{
	// 極めて軽量な終了処理のため、あえてゾーン計測は設定せずオーバーヘッドを削減
	if (active_shadow_map)
	{
		active_shadow_map->deactivate(Graphics_Core::instance().get_device_context());
		active_shadow_map = nullptr;
	}
}