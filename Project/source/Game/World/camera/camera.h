#pragma once
#include <directxmath.h>
#include "Engine/Graphics/UI/DebugMenu/CustomWidgets.h"
/**
 * @brief カメラの純粋なデータ構造体
 */
struct Camera {
	// --- 既存のパラメータ ---
	DirectX::XMVECTOR position{ 0.0f, 5.0f, -10.0f, 1.0f };
	DirectX::XMVECTOR target{ 0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMVECTOR up{ 0.0f, 1.0f, 0.0f, 0.0f };

	float fov{ DirectX::XMConvertToRadians(45.0f) };
	float aspect_ratio{ 16.0f / 9.0f };
	float near_z{ 0.1f };
	float far_z{ 10000.f };

	DirectX::XMMATRIX view{ DirectX::XMMatrixIdentity() };
	DirectX::XMMATRIX projection{ DirectX::XMMatrixIdentity() };
	DirectX::XMMATRIX inv_view{ DirectX::XMMatrixIdentity() };
	DirectX::XMMATRIX inv_projection{ DirectX::XMMatrixIdentity() };
	DirectX::XMMATRIX view_projection{ DirectX::XMMatrixIdentity() };
	DirectX::XMMATRIX inv_view_projection{ DirectX::XMMatrixIdentity() };

	float shutterDenominator = 5.0f;
	float FNumber = 2.8f;
	float FocalLength = 50.0f;
	float SensorSize = 35.f;
	float FocusDist = 5.f;
	float MaxBlurRadius = 10.f;
	bool isReversed_Z = false;

	float TStop = 2.8f;
	float ShutterSpeed = 1.0f / shutterDenominator;
	float ISO = 800.0f;
	float EV_Compensation = 0.0f;

	// --- 手振れ用パラメータ (ランダムウォーク) ---
	bool enableCameraShake = false;
	float shakeIntensity = 0.1f; // ブレ幅
	float shakeFrequency = 0.1f;  // 更新頻度
	DirectX::XMVECTOR shakeOffset{ 0.0f, 0.0f, 0.0f, 0.0f };

private:
	DirectX::XMVECTOR currentShakeTarget{ 0.0f, 0.0f, 0.0f, 0.0f };
	DirectX::XMVECTOR previousShakeTarget{ 0.0f, 0.0f, 0.0f, 0.0f };
	float shakeTimer = 1.0f;
	std::mt19937 rng{ std::random_device{}() }; // 高精度な乱数エンジン

public:
	// ランダムウォークによる揺れの計算
	void updateShake(float deltaTime) {
		if (!enableCameraShake) {
			shakeOffset = DirectX::XMVectorZero();
			return;
		}

		shakeTimer += deltaTime * shakeFrequency;

		if (shakeTimer >= 1.0f) {
			shakeTimer = 0.0f;
			previousShakeTarget = currentShakeTarget;

			std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
			currentShakeTarget = DirectX::XMVectorSet(dist(rng) * shakeIntensity, dist(rng) * shakeIntensity, 0.0f, 0.0f);
		}

		// イージング（滑らかな補間）
		float t = shakeTimer;
		float smoothT = t * t * (3.0f - 2.0f * t);
		shakeOffset = DirectX::XMVectorLerp(previousShakeTarget, currentShakeTarget, smoothT);
	}

	// 行列計算
	void identity() {
		// 注視点を揺らすことで「視線が泳ぐ手持ち感」を演出
		DirectX::XMVECTOR finalTarget = DirectX::XMVectorAdd(target, shakeOffset);
		view = DirectX::XMMatrixLookAtLH(position, finalTarget, up);

		if (isReversed_Z) {
			projection = DirectX::XMMatrixPerspectiveFovLH(fov, aspect_ratio, far_z, near_z);
		}
		else {
			projection = DirectX::XMMatrixPerspectiveFovLH(fov, aspect_ratio, near_z, far_z);
		}
		inv_view = DirectX::XMMatrixInverse(nullptr, view);
		inv_projection = DirectX::XMMatrixInverse(nullptr, projection);
		view_projection = view * projection;
		inv_view_projection = DirectX::XMMatrixInverse(nullptr, view_projection);
	}
	void DrawCameraSettingsUI() {

		CustomUI::Checkbox("Reversed-Z", &isReversed_Z);
		ImGui::Text("Clipping Planes");

		// near_z(0.001~100000) はレンジが非常に広いため LogScale を使用
		// Near < Far は NearFarControl 内部で常に保証される
		CustomUI::NearFarControl("Clip Planes",
			&near_z, &far_z,
			0.001f, 100000.0f,
			"%.3f",
			CustomUI::NearFarFlags_ShowDepthViz | CustomUI::NearFarFlags_LogScale);

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
		ImGui::Separator();
		ImGui::Text("Exposure (T-Stop)");
		CustomUI::SliderFloat("T-Stop", &TStop, 1.0f, 22.0f, "T%.1f");


		if (CustomUI::SliderFloat("Shutter Speed", &shutterDenominator, 1.0f, 8000.0f, "1/%.0f s", ImGuiSliderFlags_Logarithmic)) {

			ShutterSpeed = (1.0f / shutterDenominator);
		}
		CustomUI::SliderFloat("ISO", &ISO, 100.0f, 6400.0f, "ISO %.0f",
			ImGuiSliderFlags_Logarithmic);
		CustomUI::SliderFloat("EV Compensation", &EV_Compensation, -3.0f, 3.0f, "%.1f EV");

		ImGui::Separator();
		ImGui::Text("Camera Shake Settings");
		CustomUI::Checkbox("Enable Camera Shake", &enableCameraShake);
		if (enableCameraShake) {
			CustomUI::SliderFloat("Shake Intensity", &shakeIntensity, 0.0f, 1.0f);
			CustomUI::SliderFloat("Shake Frequency", &shakeFrequency, 0.0f, 30.0f);
		}
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

	float TStop;           // T値
	float ShutterSpeed;    // シャッタースピード [秒]
	float ISO;             // ISO感度
	float EV_Compensation; // 露出補正 [EV]

	int isReserved;
	DirectX::XMFLOAT3 Padding;


};
static_assert(sizeof(Camera_Constants) % 16 == 0, "CameraConfig must be 16-byte aligned");