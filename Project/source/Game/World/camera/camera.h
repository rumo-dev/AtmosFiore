#pragma once
#include <directxmath.h>
#include "Engine/Graphics/UI/DebugMenu/CustomWidgets.h"
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
	float far_z{ 10000.f };                                 // 遠クリップ面

	DirectX::XMMATRIX view{ DirectX::XMMatrixIdentity() };       // ビュー行列
	DirectX::XMMATRIX projection{ DirectX::XMMatrixIdentity() }; // プロジェクション行列
	DirectX::XMMATRIX inv_view{ DirectX::XMMatrixIdentity() };    // ビュー行列の逆行列	
	DirectX::XMMATRIX inv_projection{ DirectX::XMMatrixIdentity() }; // プロジェクション行列の逆行列
	DirectX::XMMATRIX view_projection{ DirectX::XMMatrixIdentity() }; // ビュー・プロジェクション行列
	DirectX::XMMATRIX inv_view_projection{ DirectX::XMMatrixIdentity() }; // ビュー・プロジェクション行列の逆行列

	float FNumber = 2.8f;      // 絞り値 (例: 1.4, 2.8)
	float FocalLength = 50.0f;  // 焦点距離 (mm)
	float SensorSize = 35.f;   // センサーサイズ (mm)
	float FocusDist = 5.f;    // 焦点が合っている距離 (m)
	float MaxBlurRadius = 10.f;// 最大ボケ半径 (ピクセル単位)

	// 現在のパラメータから行列を再計算するヘルパー関数
	void identity() {
		view = DirectX::XMMatrixLookAtLH(position, target, up);
		projection = DirectX::XMMatrixPerspectiveFovLH(fov, aspect_ratio, near_z, far_z);
		inv_view = DirectX::XMMatrixInverse(nullptr, view);
		inv_projection = DirectX::XMMatrixInverse(nullptr, projection);
		view_projection = view * projection;
		inv_view_projection = DirectX::XMMatrixInverse(nullptr, view_projection);
	}
	void DrawCameraSettingsUI() {

		ImGui::Text("Camera Parameters");


		// F値は1.4～22程度が一般的
		CustomUI::SliderFloat("F-Number (Aperture)", &FNumber, 1.0f, 22.0f, "f/%.1f");

		// 焦点距離 (mm)
		CustomUI::SliderFloat("Focal Length (mm)", &FocalLength, 12.0f, 200.0f, "%.0f mm");

		// センサーサイズ (35mmフルサイズなど)
		CustomUI::SliderFloat("Sensor Size (mm)", &SensorSize, 20.0f, 50.0f, "%.1f mm");

		CustomUI::Separator();

		ImGui::Text("Focus Settings");
		// ピント距離 (m)
		CustomUI::DragFloat("Focus Distance", &FocusDist, 0.1f, 0.1f, 100.0f, "%.2f m");

		// ブラーの強さ
		CustomUI::SliderFloat("Max Blur Radius", &MaxBlurRadius, 0.0f, 50.0f);

	}
};
struct alignas(16) Camera_Constants
{

	DirectX::XMFLOAT4 camera_position;              //カメラのワールド空間位置;

	DirectX::XMFLOAT4X4 view_projection;         //ビュー・プロジェクション変換行列;

	DirectX::XMFLOAT4X4 inv_view_projection;         //ビュー・プロジェクション変換行列;

	DirectX::XMFLOAT4X4 view;//ビュー変換行列;

	DirectX::XMFLOAT4X4 projection;//プロジェクション変換行列;

	DirectX::XMFLOAT4X4 inv_view;//ビュー変換行列;

	DirectX::XMFLOAT4X4 inv_projection;//プロジェクション変換行列;

	DirectX::XMFLOAT4X4 light_view_projection;// ライトのビュー・プロジェクション行列（シャドウマッピング用）;

	float fov;
	float near_z;
	float far_z;
	float FNumber;      // 絞り値 (例: 1.4, 2.8)
	float FocalLength;  // 焦点距離 (mm)
	float SensorSize;   // センサーサイズ (mm)
	float FocusDist;    // 焦点が合っている距離 (m)

	float MaxBlurRadius;// 最大ボケ半径 (ピクセル単位)
	DirectX::XMFLOAT3 Padding;


};
static_assert(sizeof(Camera_Constants) % 16 == 0, "CameraConfig must be 16-byte aligned");