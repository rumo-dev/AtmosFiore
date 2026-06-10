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
	DirectX::XMMATRIX inv_view{ DirectX::XMMatrixIdentity() };    // ビュー行列の逆行列	
	DirectX::XMMATRIX inv_projection{ DirectX::XMMatrixIdentity() }; // プロジェクション行列の逆行列
	DirectX::XMMATRIX view_projection{ DirectX::XMMatrixIdentity() }; // ビュー・プロジェクション行列
	DirectX::XMMATRIX inv_view_projection{ DirectX::XMMatrixIdentity() }; // ビュー・プロジェクション行列の逆行列


	// 現在のパラメータから行列を再計算するヘルパー関数
	void identity() {
		view = DirectX::XMMatrixLookAtLH(position, target, up);
		projection = DirectX::XMMatrixPerspectiveFovLH(fov, aspect_ratio, near_z, far_z);
		inv_view = DirectX::XMMatrixInverse(nullptr, view);
		inv_projection = DirectX::XMMatrixInverse(nullptr, projection);
		view_projection = view * projection;
		inv_view_projection = DirectX::XMMatrixInverse(nullptr, view_projection);
	}
};
struct Camera_Constants
{

	DirectX::XMFLOAT4 camera_position;              //カメラのワールド空間位置;
	DirectX::XMFLOAT4X4 view_projection;         //ビュー・プロジェクション変換行列;
	DirectX::XMFLOAT4X4 inv_view_projection;         //ビュー・プロジェクション変換行列;
	DirectX::XMFLOAT4X4 view;//ビュー変換行列;
	DirectX::XMFLOAT4X4 projection;//プロジェクション変換行列;
	DirectX::XMFLOAT4X4 inv_view;//ビュー変換行列;
	DirectX::XMFLOAT4X4 inv_projection;//プロジェクション変換行列;

	DirectX::XMFLOAT4X4 light_view_projection;// ライトのビュー・プロジェクション行列（シャドウマッピング用）;
};