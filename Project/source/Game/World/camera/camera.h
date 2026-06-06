#pragma once
#include <directxmath.h>

/**
 * @brief カメラの純粋なデータ構造体
 */
struct Camera {
	DirectX::XMVECTOR position{ 0.0f, 5.0f, -10.0f, 1.0f }; // 位置
	DirectX::XMVECTOR target{ 0.0f, 0.0f, 0.0f, 1.0f };     // 注視点
	DirectX::XMVECTOR up{ 0.0f, 1.0f, 0.0f, 0.0f };         // 上方向ベクトル

	float fov{ DirectX::XMConvertToRadians(45.0f) };        // 視野角
	float aspect_ratio{ 16.0f / 9.0f };                     // アスペクト比
	float near_z{ 0.1f };                                   // 近クリップ面
	float far_z{ 1000.0f };                                 // 遠クリップ面

	DirectX::XMMATRIX view{ DirectX::XMMatrixIdentity() };       // ビュー行列
	DirectX::XMMATRIX projection{ DirectX::XMMatrixIdentity() }; // プロジェクション行列

	// 現在のパラメータから行列を再計算するヘルパー関数
	void identity() {
		view = DirectX::XMMatrixLookAtLH(position, target, up);
		projection = DirectX::XMMatrixPerspectiveFovLH(fov, aspect_ratio, near_z, far_z);
	}
};