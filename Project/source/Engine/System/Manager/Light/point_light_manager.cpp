#include "point_light_manager.h"
#include "Engine/System/graphics_core.h"
#include "Game/World/camera/camera_manager.h"
#include "Engine/Utilities/math_util.h"


void PointLightManager::initialize(ID3D11Device* device)
{
	create_structured_buffer(device);
	create_light_count_buffer(device);
	_debug_sphere = new Debug_Sphere(device, 16, 16);


}
void PointLightManager::add_light(const PointLight& light)
{
	_lights.push_back(light);
}
void PointLightManager::add_light(dx::XMFLOAT3 position, float radius, float intensity, dx::XMFLOAT4 diffuseColor)
{
	PointLight light;
	light.position = position;
	light.radius = radius;
	light.intensity = intensity;
	light.diffuseColor = diffuseColor;
	_lights.push_back(light);
}

std::vector<PointLight>& PointLightManager::get_lights()
{
	return _lights;
}

void PointLightManager::remove_light(int index)
{
	if (index >= 0 && index < static_cast<int>(_lights.size()))
	{
		_lights.erase(_lights.begin() + index);
	}
}

void PointLightManager::update()
{
	for (auto& l : _lights)
		l.update();
}

void PointLightManager::draw_imgui()
{
	ImGui::Text("Point Lights: %d / %d (Visible / Total)", _visible_count, static_cast<int>(_lights.size()));
	ImGui::Separator();

	// ライト追加ボタン
	if (ImGui::Button("Add Point Light"))
	{
		add_light({ 0, 5, 0 }, 10.0f, 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
	}

	ImGui::Separator();

	int index = 0;
	for (auto& l : _lights)
	{
		std::string label = "PointLight " + std::to_string(index);
		std::string removeButtonLabel = "Remove##" + std::to_string(index);

		ImGui::PushID(index);
		if (ImGui::CollapsingHeader(label.c_str()))
		{
			l.draw_imgui(label.c_str());
		}
		ImGui::SameLine();
		if (ImGui::Button(removeButtonLabel.c_str()))
		{
			remove_light(index);
			ImGui::PopID();
			break;
		}
		ImGui::PopID();
		index++;
	}
}

void PointLightManager::upload_to_gpu(ID3D11DeviceContext* ctx)
{
	if (_lights.empty())
	{
		_visible_count = 0;
		UINT numLights = 0;
		ctx->UpdateSubresource(_cbLightCount.Get(), 0, nullptr, &numLights, 0, 0);
		ctx->PSSetConstantBuffers(4, 1, _cbLightCount.GetAddressOf());
		return;
	}

	bool enable_culling = false;
	Frustum camera_frustum{};
	auto camera_base = Camera_Manager::instance().get_active_camera();
	if (camera_base)
	{
		const Camera& camera = camera_base->get_camera();
		camera_frustum = ExtractFrustum(camera.view, camera.projection);
		enable_culling = true;
	}

	std::vector<PointLight::PointLight_GPU> gpuLights;
	gpuLights.reserve(_lights.size());

	for (auto& l : _lights)
	{
		if (enable_culling)
		{
			DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&l.position);
			if (!SphereIntersectsFrustum(camera_frustum, pos, l.radius))
			{
				continue;
			}
		}

		PointLight::PointLight_GPU g{};
		g.position = l.position;
		g.radius = l.radius;
		g.color = { l.diffuseColor.x, l.diffuseColor.y, l.diffuseColor.z };
		g.intensity = l.intensity;
		gpuLights.push_back(g);
	}

	_visible_count = static_cast<int>(gpuLights.size());

	if (gpuLights.empty())
	{
		UINT numLights = 0;
		ctx->UpdateSubresource(_cbLightCount.Get(), 0, nullptr, &numLights, 0, 0);
		ctx->PSSetConstantBuffers(4, 1, _cbLightCount.GetAddressOf());
		return;
	}

	if (gpuLights.size() > 1024)
	{
		gpuLights.resize(1024);
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	ctx->Map(_sbLights.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, gpuLights.data(), sizeof(PointLight::PointLight_GPU) * gpuLights.size());
	ctx->Unmap(_sbLights.Get(), 0);

	UINT numLights = static_cast<UINT>(gpuLights.size());
	ctx->UpdateSubresource(_cbLightCount.Get(), 0, nullptr, &numLights, 0, 0);
	ctx->PSSetShaderResources(7, 1, _sbLightsSRV.GetAddressOf());
	ctx->PSSetConstantBuffers(4, 1, _cbLightCount.GetAddressOf());
}

void PointLightManager::debug_render()
{
	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	Render_State::instance().set_3d_render_states(immediate_context, Rasterizer_State::Wireframe_CCW);

	bool enable_culling = false;
	Frustum camera_frustum{};
	auto camera_base = Camera_Manager::instance().get_active_camera();
	if (camera_base)
	{
		const Camera& camera = camera_base->get_camera();
		camera_frustum = ExtractFrustum(camera.view, camera.projection);
		enable_culling = true;
	}

	for (auto& l : _lights)
	{
		if (enable_culling)
		{
			DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&l.position);
			if (!SphereIntersectsFrustum(camera_frustum, pos, l.radius))
			{
				continue;
			}
		}

		// 1. スケール行列（半径を球の大きさに）
		DirectX::XMMATRIX S = DirectX::XMMatrixScaling(0.1f, 0.1f, 0.1f);
		DirectX::XMMATRIX S_R = DirectX::XMMatrixScaling(l.radius, l.radius, l.radius);

		// 2. 平行移動行列（ライトの位置へ）
		DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(l.position.x, l.position.y, l.position.z);


		// 3. ワールド行列
		DirectX::XMMATRIX world = S * T;
		DirectX::XMMATRIX world_R = S_R * T;



		// 4. XMMATRIX → XMFLOAT4X4 へ変換
		DirectX::XMFLOAT4X4 worldMatrix;
		DirectX::XMStoreFloat4x4(&worldMatrix, world);


		// 5. デバッグ球の描画
		_debug_sphere->Render(
			Graphics_Core::instance().get_device_context(),
			worldMatrix,
			{ l.diffuseColor.x, l.diffuseColor.y, l.diffuseColor.z, 1.0f }
		);
		//DirectX::XMStoreFloat4x4(&worldMatrix, world_R);
		//_debug_sphere->Render(
		//	Graphics_Core::instance().get_device_context(),
		//	worldMatrix,
		//	{ l.diffuseColor.x, l.diffuseColor.y, l.diffuseColor.z, 0.3f }
		//);
	}



}


void PointLightManager::create_structured_buffer(ID3D11Device* device)
{
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(PointLight::PointLight_GPU) * 1024; // 最大1024個
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = sizeof(PointLight::PointLight_GPU);

	device->CreateBuffer(&desc, nullptr, _sbLights.GetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.ElementWidth = sizeof(PointLight::PointLight_GPU);
	srvDesc.Buffer.NumElements = 1024;

	device->CreateShaderResourceView(_sbLights.Get(), &srvDesc, _sbLightsSRV.GetAddressOf());
}

void PointLightManager::create_light_count_buffer(ID3D11Device* device)
{
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = 16; // uint + padding[3]
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	device->CreateBuffer(&desc, nullptr, _cbLightCount.GetAddressOf());
}

