#include "spot_light_manager.h"
#include "Engine/System/graphics_core.h"
#include "Game/World/camera/camera_manager.h"
#include "Engine/Utilities/math_util.h"

void SpotLightManager::initialize(ID3D11Device* device)
{
	create_structured_buffer(device);
	create_light_count_buffer(device);
	_debug_cone = new Debug_Cone(device, 16);
}

void SpotLightManager::add_light(const SpotLight& light)
{
	_lights.push_back(light);
}

void SpotLightManager::add_light(dx::XMFLOAT3 position, dx::XMFLOAT3 direction, float radius, float intensity, float innerAngle, float outerAngle, dx::XMFLOAT4 diffuseColor)
{
	SpotLight light;
	light.position = position;
	light.direction = direction;
	light.radius = radius;
	light.intensity = intensity;
	light.innerAngle = innerAngle;
	light.outerAngle = outerAngle;
	light.diffuseColor = diffuseColor;
	_lights.push_back(light);
}

std::vector<SpotLight>& SpotLightManager::get_lights()
{
	return _lights;
}

void SpotLightManager::remove_light(int index)
{
	if (index >= 0 && index < static_cast<int>(_lights.size()))
	{
		_lights.erase(_lights.begin() + index);
	}
}

void SpotLightManager::update()
{
	for (auto& l : _lights)
		l.update();
}

void SpotLightManager::draw_imgui()
{
	ImGui::Text("Spot Lights: %d / %d (Visible / Total)", _visible_count, static_cast<int>(_lights.size()));
	ImGui::Separator();

	// ライト追加ボタン
	if (ImGui::Button("Add Spot Light"))
	{
		add_light({ 0, 5, 0 }, { 0, -1, 0 }, 10.0f, 1.0f, 0.5f, 0.7f, { 1.0f, 1.0f, 1.0f, 1.0f });
	}

	ImGui::Separator();

	int index = 0;
	for (auto& l : _lights)
	{
		std::string label = "SpotLight " + std::to_string(index);
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

void SpotLightManager::upload_to_gpu(ID3D11DeviceContext* ctx)
{
	if (_lights.empty())
	{
		_visible_count = 0;
		UINT numLights = 0;
		ctx->UpdateSubresource(_cbLightCount.Get(), 0, nullptr, &numLights, 0, 0);
		ctx->PSSetConstantBuffers(5, 1, _cbLightCount.GetAddressOf());
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

	std::vector<SpotLight::SpotLight_GPU> gpuLights;
	gpuLights.reserve(_lights.size());

	for (auto& l : _lights)
	{
		if (enable_culling)
		{
			DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&l.position);
			if (!SphereIntersectsFrustum(camera_frustum, pos, l.radius))
			{
				continue; // Culled!
			}
		}

		SpotLight::SpotLight_GPU g{};
		g.position = l.position;
		g.radius = l.radius;
		g.direction = l.direction;
		g.intensity = l.intensity;
		g.color = { l.diffuseColor.x, l.diffuseColor.y, l.diffuseColor.z };
		g.innerAngle = l.innerAngle;
		g.outerAngle = l.outerAngle;
		gpuLights.push_back(g);
	}

	_visible_count = static_cast<int>(gpuLights.size());

	if (gpuLights.empty())
	{
		UINT numLights = 0;
		ctx->UpdateSubresource(_cbLightCount.Get(), 0, nullptr, &numLights, 0, 0);
		ctx->PSSetConstantBuffers(5, 1, _cbLightCount.GetAddressOf());
		return;
	}

	if (gpuLights.size() > 1024)
	{
		gpuLights.resize(1024);
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	ctx->Map(_sbLights.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, gpuLights.data(), sizeof(SpotLight::SpotLight_GPU) * gpuLights.size());
	ctx->Unmap(_sbLights.Get(), 0);

	UINT numLights = static_cast<UINT>(gpuLights.size());
	ctx->UpdateSubresource(_cbLightCount.Get(), 0, nullptr, &numLights, 0, 0);
	ctx->PSSetShaderResources(8, 1, _sbLightsSRV.GetAddressOf());
	ctx->PSSetConstantBuffers(6, 1, _cbLightCount.GetAddressOf());
}

void SpotLightManager::debug_render()
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
				continue; // Culled, do not render debug shape
			}
		}

		// 方向を正規化
		dx::XMVECTOR dir = dx::XMLoadFloat3(&l.direction);
		dir = dx::XMVector3Normalize(dir);

		// コーンのデフォルト方向（Y軸負方向）
		dx::XMVECTOR coneDir = dx::XMVectorSet(0, 1, 0, 0);

		// 回転軸と角度を計算
		dx::XMVECTOR rotationAxis = dx::XMVector3Cross(coneDir, dir);
		float dotProduct = dx::XMVectorGetX(dx::XMVector3Dot(coneDir, dir));
		float rotationAngle = acosf(dotProduct);

		dx::XMMATRIX rotation;
		if (fabsf(dotProduct + 1.0f) < 0.001f)
		{
			// 逆方向（180度回転）
			rotation = dx::XMMatrixRotationX(DirectX::XM_PI);
		}
		else if (fabsf(dotProduct - 1.0f) < 0.001f)
		{
			// 同一方向（回転不要）
			rotation = dx::XMMatrixIdentity();
		}
		else
		{
			rotationAxis = dx::XMVector3Normalize(rotationAxis);
			rotation = dx::XMMatrixRotationAxis(rotationAxis, rotationAngle);
		}

		// スケール行列（半径を円柱の大きさに）
		float coneHeight = l.radius;
		float coneRadius = l.radius * tan(l.outerAngle);
		DirectX::XMMATRIX S = DirectX::XMMatrixScaling(coneRadius, coneHeight, coneRadius);

		// 平行移動行列（ライトの位置へ）
		DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(l.position.x, l.position.y, l.position.z);

		// ワールド行列
		DirectX::XMMATRIX world = S * rotation * T;

		// XMMATRIX → XMFLOAT4X4 へ変換
		DirectX::XMFLOAT4X4 worldMatrix;
		DirectX::XMStoreFloat4x4(&worldMatrix, world);

		// デバッグ円柱の描画（円錐の代用）
		_debug_cone->Render(
			Graphics_Core::instance().get_device_context(),
			worldMatrix,
			{ l.diffuseColor.x, l.diffuseColor.y, l.diffuseColor.z, 1.0f }
		);
	}
}

void SpotLightManager::create_structured_buffer(ID3D11Device* device)
{
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(SpotLight::SpotLight_GPU) * 1024; // 最大1024個
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = sizeof(SpotLight::SpotLight_GPU);

	device->CreateBuffer(&desc, nullptr, _sbLights.GetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.ElementWidth = sizeof(SpotLight::SpotLight_GPU);
	srvDesc.Buffer.NumElements = 1024;

	device->CreateShaderResourceView(_sbLights.Get(), &srvDesc, _sbLightsSRV.GetAddressOf());
}

void SpotLightManager::create_light_count_buffer(ID3D11Device* device)
{
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = 16; // uint + padding[3]
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	device->CreateBuffer(&desc, nullptr, _cbLightCount.GetAddressOf());
}
