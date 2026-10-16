#include "area_light_manager.h"
#include "Engine/System/graphics_core.h"
#include "Game/World/camera/camera_manager.h"
#include "Engine/Utilities/math_util.h"
#include "Engine/Graphics/Shader/ltc_matrix.hpp"

void AreaLightManager::initialize(ID3D11Device* device)
{
	create_structured_buffer(device);
	create_light_count_buffer(device);
	create_ltc_lut(device);
	_debug_box = new Debug_Cube(device);
	_debug_disk = new Debug_Disk(device, 32);
	_debug_sphere = new Debug_Sphere(device, 16, 16);
}

void AreaLightManager::add_light(const AreaLight& light)
{
	_lights.push_back(light);
}

void AreaLightManager::add_light(dx::XMFLOAT3 position, dx::XMFLOAT3 direction, dx::XMFLOAT3 right, float width, float height, float intensity, dx::XMFLOAT4 diffuseColor)
{
	AreaLight light;
	light.position = position;
	light.direction = direction;
	light.right = right;
	light.width = width;
	light.height = height;
	light.intensity = intensity;
	light.diffuseColor = diffuseColor;
	_lights.push_back(light);
}

std::vector<AreaLight>& AreaLightManager::get_lights()
{
	return _lights;
}

void AreaLightManager::remove_light(int index)
{
	if (index >= 0 && index < static_cast<int>(_lights.size()))
	{
		_lights.erase(_lights.begin() + index);
	}
}

void AreaLightManager::update()
{
	for (auto& l : _lights)
		l.update();
}

void AreaLightManager::draw_imgui()
{
	ImGui::Text("Area Lights: %d / %d (Visible / Total)", _visible_count, static_cast<int>(_lights.size()));
	ImGui::Separator();

	// ライト追加ボタン
	if (ImGui::Button("Add Area Light"))
	{
		add_light({ 0, 5, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, 2.0f, 2.0f, 3500.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
	}

	ImGui::Separator();

	int index = 0;
	for (auto& l : _lights)
	{
		std::string label = "AreaLight " + std::to_string(index);
		std::string removeButtonLabel = "Remove##" + std::to_string(index);

		ImGui::PushID(index);
		if (ImGui::CollapsingHeader(label.c_str()))
		{
			l.draw_imgui("Parameters");
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

void AreaLightManager::upload_to_gpu(ID3D11DeviceContext* ctx)
{
	if (_lights.empty())
	{
		_visible_count = 0;
		UINT numLights = 0;
		ctx->UpdateSubresource(_cbLightCount.Get(), 0, nullptr, &numLights, 0, 0);
		ctx->PSSetConstantBuffers(7, 1, _cbLightCount.GetAddressOf());
		return;
	}

	bool enable_culling = false;
	Frustum camera_frustum{};
	auto camera_base = CameraManager::instance().get_active_camera();
	if (camera_base)
	{
		const Camera& camera = camera_base->get_camera();
		camera_frustum = ExtractFrustum(camera.view, camera.projection);
		enable_culling = true;
	}

	std::vector<AreaLight::AreaLightGpu> gpuLights;
	gpuLights.reserve(_lights.size());

	for (auto& l : _lights)
	{
		if (enable_culling)
		{
			DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&l.position);
			float maxRadius = max(max(l.width, l.height), l.radius);
			maxRadius = max(maxRadius, l.range);
			if (!SphereIntersectsFrustum(camera_frustum, pos, maxRadius))
			{
				continue; // Culled!
			}
		}

		AreaLight::AreaLightGpu g{};
		g.position = l.position;
		g.width = l.width;
		g.direction = l.direction;
		g.height = l.height;
		g.right = l.right;
		g.radius = l.radius;
		g.shape = static_cast<uint32_t>(l.shape);
		g.intensity = l.intensity;
		g.range = l.range;
		g.color = { l.diffuseColor.x, l.diffuseColor.y, l.diffuseColor.z };
		gpuLights.push_back(g);
	}

	_visible_count = static_cast<int>(gpuLights.size());

	if (gpuLights.empty())
	{
		UINT numLights = 0;
		ctx->UpdateSubresource(_cbLightCount.Get(), 0, nullptr, &numLights, 0, 0);
		ctx->PSSetConstantBuffers(7, 1, _cbLightCount.GetAddressOf());
		return;
	}

	if (gpuLights.size() > 1024)
	{
		gpuLights.resize(1024);
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	ctx->Map(_sbLights.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, gpuLights.data(), sizeof(AreaLight::AreaLightGpu) * gpuLights.size());
	ctx->Unmap(_sbLights.Get(), 0);

	UINT numLights = static_cast<UINT>(gpuLights.size());
	ctx->UpdateSubresource(_cbLightCount.Get(), 0, nullptr, &numLights, 0, 0);
	ctx->PSSetShaderResources(10, 1, _sbLightsSRV.GetAddressOf());
	ctx->PSSetShaderResources(11, 1, _ltcMatrixLutSRV.GetAddressOf());
	ctx->PSSetShaderResources(12, 1, _ltcAmplitudeLutSRV.GetAddressOf());
	ctx->PSSetConstantBuffers(7, 1, _cbLightCount.GetAddressOf());
}

void AreaLightManager::create_ltc_lut(ID3D11Device* device)
{
	constexpr UINT size = 64;
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = size; desc.Height = size; desc.MipLevels = 1; desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA data{};
	data.SysMemPitch = size * 4 * sizeof(float);
	Microsoft::WRL::ComPtr<ID3D11Texture2D> matrixTexture;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> amplitudeTexture;
	data.pSysMem = LTC1;
	if (SUCCEEDED(device->CreateTexture2D(&desc, &data, matrixTexture.GetAddressOf())))
		device->CreateShaderResourceView(matrixTexture.Get(), nullptr, _ltcMatrixLutSRV.GetAddressOf());
	data.pSysMem = LTC2;
	if (SUCCEEDED(device->CreateTexture2D(&desc, &data, amplitudeTexture.GetAddressOf())))
		device->CreateShaderResourceView(amplitudeTexture.Get(), nullptr, _ltcAmplitudeLutSRV.GetAddressOf());
}

void AreaLightManager::debug_render()
{
	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	Render_State::instance().set_3d_render_states(immediate_context, Rasterizer_State::Wireframe_CCW);
	// Scene depthで遮蔽し、可視化形状が深度バッファを書き換えないようにする。
	const auto camera = CameraManager::instance().get_active_camera();
	const bool reversedZ = camera && camera->get_camera().isReversed_Z;
	Render_State::instance().set_depth_stencil_state(
		immediate_context,
		reversedZ ? Depth_State::Reversed_Z_Test_Enable_Write_Disable : Depth_State::Test_Enable_Write_Disable);

	bool enable_culling = false;
	Frustum camera_frustum{};
	auto camera_base = CameraManager::instance().get_active_camera();
	if (camera_base)
	{
		const Camera& camera = camera_base->get_camera();
		camera_frustum = ExtractFrustum(camera.view, camera.projection);
		enable_culling = true;
	}

	for (auto& l : _lights)
	{
		if (!l.debug_wireframe && !l.debug_show_range) continue;
		if (enable_culling)
		{
			DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&l.position);
			float sourceRadius = max(max(l.width, l.height), l.radius);
			float maxRadius = l.debug_show_range ? max(l.range, sourceRadius) : sourceRadius;
			if (!SphereIntersectsFrustum(camera_frustum, pos, maxRadius))
			{
				continue; // Culled, do not render debug shape
			}
		}

		// directionは照射方向なので反転して面法線化（シェーダーと同じ）
		dx::XMVECTOR dir = dx::XMLoadFloat3(&l.direction);
		dir = dx::XMVector3Normalize(dir);
		dx::XMVECTOR normal = dx::XMVectorNegate(dir); // 面法線（照射方向）

		dx::XMVECTOR right = dx::XMLoadFloat3(&l.right);
		right = dx::XMVector3Normalize(right);

		// rightを面へ射影（シェーダーと同じ）
		dx::XMVECTOR dot = dx::XMVector3Dot(right, normal);
		dx::XMVECTOR rightProjected = dx::XMVectorSubtract(right, dx::XMVectorMultiply(normal, dot));
		rightProjected = dx::XMVector3Normalize(rightProjected);

		dx::XMVECTOR up = dx::XMVector3Cross(rightProjected, normal);

		// スケール行列
		// Rectangle mesh is already centered on [-0.5, 0.5], so width/height are full dimensions.
		DirectX::XMMATRIX S = DirectX::XMMatrixScaling(l.width, l.height, 0.1f);

		// 回転行列（方向ベクトルに合わせる）
		dx::XMFLOAT3X3 rotation3x3;
		dx::XMFLOAT3 rightFloat, upFloat, normalFloat;
		DirectX::XMStoreFloat3(&rightFloat, rightProjected);
		DirectX::XMStoreFloat3(&upFloat, up);
		DirectX::XMStoreFloat3(&normalFloat, normal);
		rotation3x3._11 = rightFloat.x; rotation3x3._12 = rightFloat.y; rotation3x3._13 = rightFloat.z;
		rotation3x3._21 = upFloat.x;    rotation3x3._22 = upFloat.y;    rotation3x3._23 = upFloat.z;
		rotation3x3._31 = normalFloat.x; rotation3x3._32 = normalFloat.y; rotation3x3._33 = normalFloat.z;
		dx::XMMATRIX rotation = DirectX::XMLoadFloat3x3(&rotation3x3);

		// 平行移動行列（ライトの位置へ）
		DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(l.position.x, l.position.y, l.position.z);

		// ワールド行列
		DirectX::XMMATRIX world = S * rotation * T;

		// XMMATRIX → XMFLOAT4X4 へ変換
		DirectX::XMFLOAT4X4 worldMatrix;
		DirectX::XMStoreFloat4x4(&worldMatrix, world);

		// 光源そのものの形状表示
		if (l.debug_wireframe && l.shape == AreaLightShape::Rectangle)
		{
			_debug_box->Render(
				Graphics_Core::instance().get_device_context(),
				worldMatrix,
				{ l.diffuseColor.x, l.diffuseColor.y, l.diffuseColor.z, 1.0f }
			);
		}
		else if (l.debug_wireframe && l.shape == AreaLightShape::Disk)
		{
			// 円盤用のスケール行列（半径を適用）
			DirectX::XMMATRIX S_disk = DirectX::XMMatrixScaling(l.radius * 2.0f, l.radius * 2.0f, 0.1f);
			DirectX::XMMATRIX world_disk = S_disk * rotation * T;
			DirectX::XMStoreFloat4x4(&worldMatrix, world_disk);

			_debug_disk->Render(
				Graphics_Core::instance().get_device_context(),
				worldMatrix,
				{ l.diffuseColor.x, l.diffuseColor.y, l.diffuseColor.z, 1.0f }
			);
		}
		else if (l.debug_wireframe && l.shape == AreaLightShape::Sphere)
		{
			// 球形用のスケール行列（半径を適用）
			DirectX::XMMATRIX S_sphere = DirectX::XMMatrixScaling(l.radius * 2.0f, l.radius * 2.0f, l.radius * 2.0f);
			DirectX::XMMATRIX world_sphere = S_sphere * T; // 球形は回転不要
			DirectX::XMStoreFloat4x4(&worldMatrix, world_sphere);

			_debug_sphere->Render(
				Graphics_Core::instance().get_device_context(),
				worldMatrix,
				{ l.diffuseColor.x, l.diffuseColor.y, l.diffuseColor.z, 1.0f }
			);
		}

		// ライトの距離減衰範囲（シェーダーのRangeと同じ半径）
		if (l.debug_show_range)
		{
			DirectX::XMMATRIX rangeScale = DirectX::XMMatrixScaling(l.range * 2.0f, l.range * 2.0f, l.range * 2.0f);
			DirectX::XMMATRIX rangeWorld = rangeScale * T;
			DirectX::XMStoreFloat4x4(&worldMatrix, rangeWorld);
			_debug_sphere->Render(
				immediate_context,
				worldMatrix,
				{ l.diffuseColor.x, l.diffuseColor.y, l.diffuseColor.z, 1.0f }
			);
		}
	}
}

void AreaLightManager::create_structured_buffer(ID3D11Device* device)
{
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(AreaLight::AreaLightGpu) * 1024; // 最大1024個
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = sizeof(AreaLight::AreaLightGpu);

	device->CreateBuffer(&desc, nullptr, _sbLights.GetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.ElementWidth = sizeof(AreaLight::AreaLightGpu);
	srvDesc.Buffer.NumElements = 1024;

	device->CreateShaderResourceView(_sbLights.Get(), &srvDesc, _sbLightsSRV.GetAddressOf());
}

void AreaLightManager::create_light_count_buffer(ID3D11Device* device)
{
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = 16; // uint + padding[3]
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	device->CreateBuffer(&desc, nullptr, _cbLightCount.GetAddressOf());
}
