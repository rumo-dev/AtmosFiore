// BLOOM
#include "shadow.h"
#include"Engine/System/Manager/resource_manager.h"
#include"Engine/Graphics/geometry_buffer/geometry_buffer.h"
#include "Game/World/camera/camera_manager.h"

#include <vector>
#include <algorithm>
#include "Engine/system/render_state.h"
#include "Engine/System/graphics_core.h"
shadow::shadow(ID3D11Device* device, uint32_t width, uint32_t height)
{

	// SHADOW
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
		height, DXGI_FORMAT_R16G16B16A16_FLOAT,
		false

	);




}

void shadow::make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map)
{
	// Store current states
	ID3D11ShaderResourceView* null_shader_resource_view{};
	ID3D11ShaderResourceView* cached_shader_resource_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
	//immediate_context->PSGetShaderResources(0, downsampled_count, cached_shader_resource_views);

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  cached_depth_stencil_state;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState>  cached_rasterizer_state;
	Microsoft::WRL::ComPtr<ID3D11BlendState>  cached_blend_state;
	FLOAT blend_factor[4];
	UINT sample_mask;
	immediate_context->OMGetDepthStencilState(cached_depth_stencil_state.GetAddressOf(), 0);
	immediate_context->RSGetState(cached_rasterizer_state.GetAddressOf());
	immediate_context->OMGetBlendState(cached_blend_state.GetAddressOf(), blend_factor, &sample_mask);

	Microsoft::WRL::ComPtr<ID3D11Buffer>  cached_constant_buffer;
	immediate_context->PSGetConstantBuffers(8, 1, cached_constant_buffer.GetAddressOf());

	// Bind states
	Render_State::instance().set_2d_render_states(immediate_context);


	// Final composite
	shaded->Clear(immediate_context, 1, 1, 1, 1);
	shaded->Activate(immediate_context);
	{
		ID3D11ShaderResourceView* shader_resource_views[GBUFFER_COUNT + 2]{};



		// GBUFFER の SRV を後ろにコピー
		Graphics_Core::instance().get_geometry_buffer()->
			GetShaderResourceViews(
				shader_resource_views,
				GBUFFER_COUNT,
				0
			);
		shader_resource_views[4] = color_map;
		shader_resource_views[5] = directional_shadow_map->get_depth_map();


		bit_block_transfer->Blit(immediate_context, shader_resource_views, 0, 6, Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("SHADOW_PS"));
	}


	shaded->Deactivate(immediate_context);
	// Restore states
	immediate_context->PSSetConstantBuffers(8, 1, cached_constant_buffer.GetAddressOf());

	immediate_context->OMSetDepthStencilState(cached_depth_stencil_state.Get(), 0);
	immediate_context->RSSetState(cached_rasterizer_state.Get());
	immediate_context->OMSetBlendState(cached_blend_state.Get(), blend_factor, sample_mask);

	//immediate_context->PSSetShaderResources(0, downsampled_count, cached_shader_resource_views);
	for (ID3D11ShaderResourceView* cached_shader_resource_view : cached_shader_resource_views)
	{
		if (cached_shader_resource_view) cached_shader_resource_view->Release();
	}




}

void shadow::make_directional_shadow_begin() {
	using namespace DirectX;

	Graphics_Core::Scene_Constants data{};
	XMStoreFloat4(&data.camera_position, Camera_Manager::instance().get_active_camera()->get_camera().position);

	const float aspect_ratio = directional_shadow_map->viewport.Width / directional_shadow_map->viewport.Height;
	XMVECTOR F{ XMLoadFloat4(&light_view_focus) };
	DirectX::XMFLOAT4 directional_light_direction = Graphics_Core::instance().get_directional_light_direction();
	XMVECTOR E{ F - XMVector3Normalize(XMLoadFloat4(&directional_light_direction)) * light_view_distance };
	XMVECTOR U{ XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) };
	XMMATRIX V{ XMMatrixLookAtLH(E, F, U) };
	XMMATRIX P{ XMMatrixOrthographicLH(light_view_size * aspect_ratio, light_view_size, light_view_near_z, light_view_far_z) };

	DirectX::XMStoreFloat4x4(&data.view_projection, V * P);
	// store for culling usage
	last_light_view_projection = data.view_projection;
	data.light_view_projection = data.view_projection;
	Graphics_Core::instance().get_device_context()->UpdateSubresource(Graphics_Core::instance().get_constant_buffer(Graphics_Core::Conastant_Buffer_Type::Scene), 0, 0, &data, 0, 0);
	Graphics_Core::instance().get_device_context()->VSSetConstantBuffers(1, 1, Graphics_Core::instance().get_constant_buffer_address(Graphics_Core::Conastant_Buffer_Type::Scene));

	active_shadow_map = directional_shadow_map.get();
	active_shadow_map->clear(Graphics_Core::instance().get_device_context(), 1.0f);
	active_shadow_map->activate(Graphics_Core::instance().get_device_context());
}
void shadow::make_shadow_begin(PointShadowFace face) {
	using namespace DirectX;

	ID3D11DeviceContext* context = Graphics_Core::instance().get_device_context();
	auto& lights = Graphics_Core::instance().get_point_light_manager().get_lights();

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

	context->UpdateSubresource(point_shadow_constant_buffer.Get(), 0, 0, &constants, 0, 0);
	context->VSSetConstantBuffers(5, 1, point_shadow_constant_buffer.GetAddressOf());
	context->PSSetConstantBuffers(5, 1, point_shadow_constant_buffer.GetAddressOf());

	// prepare per-face view-projections for point light (cube faces)
	if (!lights.empty())
	{
		const auto& light = lights.front();
		XMVECTOR pos = XMLoadFloat3(&light.position);
		// +X, -X, +Y, -Y, +Z, -Z
		XMVECTOR targets[6] = {
			XMVectorAdd(pos, XMVectorSet(1,0,0,0)), XMVectorAdd(pos, XMVectorSet(-1,0,0,0)),
			XMVectorAdd(pos, XMVectorSet(0,1,0,0)), XMVectorAdd(pos, XMVectorSet(0,-1,0,0)),
			XMVectorAdd(pos, XMVectorSet(0,0,1,0)), XMVectorAdd(pos, XMVectorSet(0,0,-1,0))
		};
		XMVECTOR ups[6] = {
			XMVectorSet(0,1,0,0), XMVectorSet(0,1,0,0), XMVectorSet(0,0,-1,0), XMVectorSet(0,0,1,0), XMVectorSet(0,1,0,0), XMVectorSet(0,1,0,0)
		};
		float aspect = 1.0f; // square faces
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

	active_shadow_map = face == PointShadowFace::Front ? point_shadow_front.get() : point_shadow_back.get();
	active_shadow_map->clear(context, 1.0f);
	active_shadow_map->activate(context);
}
void shadow::make_shadow_end() {
	if (active_shadow_map)
	{
		active_shadow_map->deactivate(Graphics_Core::instance().get_device_context());
		active_shadow_map = nullptr;
	}
	// keep current_point_face_group as last group
}
