#pragma once

#include "camera_manager.h"
#include "spectator_camera.h"
#include <algorithm>
#include <cmath>
#include <memory>

struct CameraLightPlacement
{
	dx::XMFLOAT3 position{};
	dx::XMFLOAT3 forward{ 0.0f, 0.0f, 1.0f };
	dx::XMFLOAT3 right{ 1.0f, 0.0f, 0.0f };
};

inline CameraLightPlacement GetCameraLightPlacement()
{
	const Camera& camera = CameraManager::instance().get_active_camera()->get_camera();
	CameraLightPlacement placement;
	dx::XMStoreFloat3(&placement.position, camera.position);
	dx::XMVECTOR forward = dx::XMVectorSubtract(camera.target, camera.position);
	if (dx::XMVectorGetX(dx::XMVector3LengthSq(forward)) < 1.0e-6f)
		forward = dx::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	forward = dx::XMVector3Normalize(forward);
	dx::XMStoreFloat3(&placement.forward, forward);
	dx::XMVECTOR right = dx::XMVector3Cross(
		dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), forward);
	if (dx::XMVectorGetX(dx::XMVector3LengthSq(right)) < 1.0e-6f)
		right = dx::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	dx::XMStoreFloat3(&placement.right, dx::XMVector3Normalize(right));
	return placement;
}

inline bool GoToLightPosition(const dx::XMFLOAT3& position,
	const dx::XMFLOAT3* look_direction = nullptr)
{
	auto& manager = CameraManager::instance();
	const CameraLightPlacement placement = GetCameraLightPlacement();
	dx::XMVECTOR forward = look_direction
		? dx::XMLoadFloat3(look_direction)
		: dx::XMLoadFloat3(&placement.forward);
	if (dx::XMVectorGetX(dx::XMVector3LengthSq(forward)) < 1.0e-6f)
		forward = dx::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	forward = dx::XMVector3Normalize(forward);

	auto spectator = std::dynamic_pointer_cast<SpectatorCamera>(manager.get_active_camera());
	if (!spectator && manager.switch_camera("Spectator"))
		spectator = std::dynamic_pointer_cast<SpectatorCamera>(manager.get_active_camera());
	if (!spectator) {
		spectator = std::make_shared<SpectatorCamera>();
		manager.register_camera("Light Inspector", spectator);
		manager.switch_camera("Light Inspector");
	}

	const float yaw = dx::XMConvertToDegrees(atan2f(
		dx::XMVectorGetZ(forward), dx::XMVectorGetX(forward)));
	const float pitch = (std::clamp)(dx::XMConvertToDegrees(asinf((std::clamp)(
		dx::XMVectorGetY(forward), -1.0f, 1.0f))), -89.0f, 89.0f);
	const float yaw_radians = dx::XMConvertToRadians(yaw);
	const float pitch_radians = dx::XMConvertToRadians(pitch);
	forward = dx::XMVectorSet(cosf(pitch_radians) * cosf(yaw_radians),
		sinf(pitch_radians), cosf(pitch_radians) * sinf(yaw_radians), 0.0f);
	const dx::XMVECTOR eye = dx::XMVectorSet(position.x, position.y, position.z, 1.0f);
	spectator->set_rotation(yaw, pitch, eye, dx::XMVectorAdd(eye, forward));
	spectator->get_camera().identity();
	return true;
}
