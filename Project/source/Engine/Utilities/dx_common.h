#pragma once

/**
 * @file dx_common.h
 * @brief DirectX関連の共通ユーティリティ定義
 *
 * Windows/DirectXの基本ヘッダ、スマートポインタ、リソース解放、
 * 定数バッファ作成ユーティリティなどをまとめたヘッダです。
 */

 // Windows + DirectX基本
#include <windows.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// スマートポインタ（Microsoft::WRL::ComPtr）
using Microsoft::WRL::ComPtr;

// DirectXMath名前空間を短縮
namespace dx = DirectX;

/**
 * @brief COMオブジェクトの安全な解放を行うユーティリティ関数
 * @tparam T COMオブジェクト型
 * @param ptr 解放対象のポインタ（参照渡し）
 *
 * ptrがnullptrでない場合、Release()を呼び出し、ptrをnullptrにします。
 */
template <typename T>
inline void safe_release(T*& ptr) {
	if (ptr) {
		ptr->Release();
		ptr = nullptr;
	}
}

/**
 * @brief 定数バッファ用のD3D11_BUFFER_DESCを生成します。
 * @tparam T 定数バッファに格納する構造体型
 * @return D3D11_BUFFER_DESC 構造体
 *
 * sizeof(T)に応じたByteWidthで、Usage/BindFlags/CPUAccessFlagsを
 * 定数バッファ用に初期化したD3D11_BUFFER_DESCを返します。
 */
template <typename T>
D3D11_BUFFER_DESC create_constant_buffer_desc() {
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(T);
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = 0;
	return desc;
}

enum class CoordinateSystem : uint32_t
{
	RH_Y_UP = 0,
	LH_Y_UP = 1,
	RH_Z_UP = 2,
	LH_Z_UP = 3,
};
static const DirectX::XMFLOAT4X4 COORDINATE_SYSTEM_TABLE[] =
{
	// RH_Y_UP
	{ -1, 0, 0, 0,
	   0, 1, 0, 0,
	   0, 0, 1, 0,
	   0, 0, 0, 1 },

	   // LH_Y_UP
	   {  1, 0, 0, 0,
		  0, 1, 0, 0,
		  0, 0, 1, 0,
		  0, 0, 0, 1 },

		  // RH_Z_UP
		  { -1, 0, 0, 0,
			 0, 0,-1, 0,
			 0, 1, 0, 0,
			 0, 0, 0, 1 },

			 // LH_Z_UP
			 {  1, 0, 0, 0,
				0, 0, 1, 0,
				0, 1, 0, 0,
				0, 0, 0, 1 },
};
inline dx::XMFLOAT4X4 make_world_matrix(
	CoordinateSystem system,
	const dx::XMFLOAT3& scale,
	const dx::XMFLOAT3& rotation,   // pitch, yaw, roll
	const dx::XMFLOAT3& translation,
	const float scale_factor = 0.01f)
{
	auto idx = static_cast<uint32_t>(system);
	assert(idx < _countof(COORDINATE_SYSTEM_TABLE));

	dx::XMMATRIX C = dx::XMLoadFloat4x4(&COORDINATE_SYSTEM_TABLE[idx]);

	dx::XMMATRIX S = dx::XMMatrixScaling(scale.x * scale_factor, scale.y * scale_factor, scale.z * scale_factor);
	dx::XMMATRIX R = dx::XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);
	dx::XMMATRIX T = dx::XMMatrixTranslation(translation.x, translation.y, translation.z);

	dx::XMMATRIX M = C * S * R * T;

	dx::XMFLOAT4X4 world{};
	dx::XMStoreFloat4x4(&world, M);
	return world;
}
