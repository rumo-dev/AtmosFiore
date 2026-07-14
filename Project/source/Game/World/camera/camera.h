#pragma once
#include <algorithm>
#include <cmath>
#include <directxmath.h>
#include "Engine/Graphics/UI/DebugMenu/CustomWidgets.h"


#include "Engine/Audio/AudioSystem.h"

/**
 * @brief カメラの純粋なデータ構造体
 */
struct Camera {
	// --- 既存のパラメータ ---
	DirectX::XMVECTOR position{ 0.0f, 5.0f, -10.0f, 1.0f };
	DirectX::XMVECTOR target{ 0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMVECTOR up{ 0.0f, 1.0f, 0.0f, 0.0f };

	// Projection data derived from lens, sensor, and output size.
	float fov{ DirectX::XMConvertToRadians(27.0f) };
	float aspect_ratio{ 16.0f / 9.0f };
	float near_z{ 0.1f };
	float far_z{ 10000.f };

	DirectX::XMMATRIX view{ DirectX::XMMatrixIdentity() };
	DirectX::XMMATRIX projection{ DirectX::XMMatrixIdentity() };
	DirectX::XMMATRIX inv_view{ DirectX::XMMatrixIdentity() };
	DirectX::XMMATRIX inv_projection{ DirectX::XMMatrixIdentity() };
	DirectX::XMMATRIX view_projection{ DirectX::XMMatrixIdentity() };
	DirectX::XMMATRIX inv_view_projection{ DirectX::XMMatrixIdentity() };
	DirectX::XMVECTOR prevPosition{ 0.0f, 5.0f, -10.0f, 1.0f };

	// Parameters controlled by a photographer.
	float shutterDenominator = 5.0f;
	float FNumber = 7.9f;
	float FocalLength = 50.0f;
	float SensorSize = 35.5f; // Sensor width [mm]
	float FocusDist = 8.8f;
	bool isReversed_Z = false;

	// Values derived automatically from camera and render settings.
	float MaxBlurRadius = 21.2f;
	float TStop = 7.9f;
	float ShutterSpeed = 1.0f / shutterDenominator;
	float ISO = 617.0f;
	float EV_Compensation = -0.8f;

	// Automatic mode can be enabled globally and refined per camera control.
	bool autoSettingsEnabled = false;
	bool autoFNumber = true;
	bool autoFocalLength = true;
	bool autoSensorSize = true;
	bool autoFocusDistance = true;
	bool autoShutterSpeed = true;
	bool autoISO = true;
	bool autoEVCompensation = true;
	float autoExposureTargetEV100 = 2.2f; // Matches the previous indoor exposure.

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
	void apply_auto_settings(float delta_time) {
		if (!autoSettingsEnabled) return;

		const float focus_distance = std::fmax(
			DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(target, position))), 0.1f);
		const float camera_speed = delta_time > 0.0f
			? DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(position, prevPosition))) / delta_time
			: 0.0f;

		if (autoFocusDistance) FocusDist = focus_distance;
		if (autoFocalLength) FocalLength = std::clamp(35.0f + focus_distance * 2.0f, 35.0f, 85.0f);
		if (autoSensorSize) SensorSize = 36.0f; // Full-frame reference width.
		if (autoFNumber) FNumber = std::clamp(2.8f + std::log2(focus_distance + 1.0f), 2.8f, 11.0f);
		if (autoShutterSpeed) shutterDenominator = std::clamp(5.0f + camera_speed * 120.0f, 1.0f, 1000.0f);

		// Keep the scene's indoor EV100 reference. If ISO reaches its limit,
		// favor a slower shutter over an unnecessarily dark result.
		if (autoISO) {
			const float ev100 = std::log2((FNumber * FNumber) * shutterDenominator);
			ISO = std::clamp(100.0f * std::exp2(ev100 - autoExposureTargetEV100), 100.0f, 6400.0f);
			if (autoShutterSpeed && ISO >= 6400.0f) {
				shutterDenominator = std::clamp(
					std::exp2(autoExposureTargetEV100) * (ISO / 100.0f) / (FNumber * FNumber),
					1.0f, 8000.0f);
			}
		}
		if (autoEVCompensation) EV_Compensation = 0.0f;
	}

	// SensorSize is the sensor width. Keep horizontal framing stable while
	// deriving the vertical FoV required by XMMatrixPerspectiveFovLH.
	void update_derived_parameters(float viewport_aspect_ratio, float viewport_height) {
		aspect_ratio = std::fmax(viewport_aspect_ratio, 0.001f);

		const float focal_length = std::fmax(FocalLength, 0.001f);
		const float sensor_width = std::fmax(SensorSize, 0.001f);
		const float sensor_height = sensor_width / aspect_ratio;
		fov = 2.0f * std::atan(sensor_height / (2.0f * focal_length));

		// No lens transmission loss is modeled, so T-stop equals F-number.
		TStop = std::fmax(FNumber, 0.001f);
		ShutterSpeed = 1.0f / std::fmax(shutterDenominator, 1.0f);

		// This is a resolution-dependent rendering cap, not a camera control.
		MaxBlurRadius = std::clamp(viewport_height * 0.02f, 1.0f, 64.0f);
	}

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
	void UpdateAudioListener(float deltaTime) {
		// 1. 現在の位置と前フレームの位置を格納
		DirectX::XMFLOAT4 curPosF, prevPosF;
		DirectX::XMStoreFloat4(&curPosF, position);
		DirectX::XMStoreFloat4(&prevPosF, prevPosition);

		// 2. 速度の計算（ドップラー効果用）
		float vx = 0.0f, vy = 0.0f, vz = 0.0f;
		if (deltaTime > 0.0f) {
			vx = (curPosF.x - prevPosF.x) / deltaTime;
			vy = (curPosF.y - prevPosF.y) / deltaTime;
			vz = (curPosF.z - prevPosF.z) / deltaTime;
		}
		AudioSystem::instance().SetListenerVelocity(vx, vy, vz);

		// 3. 位置の更新
		AudioSystem::instance().SetListenerPosition(curPosF.x, curPosF.y, curPosF.z);

		// 4. カメラの逆行列から「前方向」と「上方向」を抽出して設定
		DirectX::XMFLOAT4X4 invViewF;
		DirectX::XMStoreFloat4x4(&invViewF, inv_view);

		AudioSystem::instance().SetListenerOrientation(
			invViewF._31, invViewF._32, invViewF._33,  // 前方向 (OrientFront)
			invViewF._21, invViewF._22, invViewF._23   // 上方向 (OrientTop)
		);

		// 5. 次フレームの速度計算のために現在の位置を保存
		prevPosition = position;
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
		ImGui::Text("Automatic Camera Settings");
		CustomUI::Checkbox("Enable Auto Settings", &autoSettingsEnabled);
		ImGui::TextDisabled("Auto values update every frame; turn off an item to edit it manually.");
		ImGui::BeginDisabled(!autoSettingsEnabled || !autoISO);
		CustomUI::SliderFloat("Auto Exposure Target (EV100)", &autoExposureTargetEV100, -2.0f, 8.0f, "%.1f EV");
		ImGui::EndDisabled();

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
		ImGui::BeginDisabled(autoSettingsEnabled && autoFNumber);
		CustomUI::SliderFloat("F-Number (Aperture)", &FNumber, 1.0f, 22.0f, "f/%.1f");
		ImGui::EndDisabled(); ImGui::SameLine(); CustomUI::Checkbox("Auto Aperture", &autoFNumber);

		// 焦点距離 (mm)
		ImGui::BeginDisabled(autoSettingsEnabled && autoFocalLength);
		CustomUI::SliderFloat("Focal Length (mm)", &FocalLength, 12.0f, 200.0f, "%.0f mm");
		ImGui::EndDisabled(); ImGui::SameLine(); CustomUI::Checkbox("Auto Focal Length", &autoFocalLength);

		// センサーサイズ (35mmフルサイズなど)
		ImGui::BeginDisabled(autoSettingsEnabled && autoSensorSize);
		CustomUI::SliderFloat("Sensor Width (mm)", &SensorSize, 20.0f, 50.0f, "%.1f mm");
		ImGui::EndDisabled(); ImGui::SameLine(); CustomUI::Checkbox("Auto Sensor Width", &autoSensorSize);
		ImGui::TextDisabled("Vertical FoV: %.1f deg (calculated)", DirectX::XMConvertToDegrees(fov));

		CustomUI::Separator();

		ImGui::Text("Focus Settings");
		// ピント距離 (m)
		ImGui::BeginDisabled(autoSettingsEnabled && autoFocusDistance);
		CustomUI::DragFloat("Focus Distance", &FocusDist, 0.1f, 0.1f, 100.0f, "%.2f m");
		ImGui::EndDisabled(); ImGui::SameLine(); CustomUI::Checkbox("Auto Focus", &autoFocusDistance);

		// ブラーの強さ
		ImGui::TextDisabled("Max Blur Radius: %.1f px (calculated from resolution)", MaxBlurRadius);
		ImGui::Separator();
		ImGui::Text("Exposure");
		ImGui::TextDisabled("T-Stop: T%.1f (calculated from F-Number)", TStop);


		ImGui::BeginDisabled(autoSettingsEnabled && autoShutterSpeed);
		CustomUI::SliderFloat("Shutter Speed", &shutterDenominator, 1.0f, 8000.0f, "1/%.0f s", ImGuiSliderFlags_Logarithmic);
		ImGui::EndDisabled(); ImGui::SameLine(); CustomUI::Checkbox("Auto Shutter", &autoShutterSpeed);
		ImGui::BeginDisabled(autoSettingsEnabled && autoISO);
		CustomUI::SliderFloat("ISO", &ISO, 100.0f, 6400.0f, "ISO %.0f",
			ImGuiSliderFlags_Logarithmic);
		ImGui::EndDisabled(); ImGui::SameLine(); CustomUI::Checkbox("Auto ISO", &autoISO);
		ImGui::BeginDisabled(autoSettingsEnabled && autoEVCompensation);
		CustomUI::SliderFloat("EV Compensation", &EV_Compensation, -3.0f, 3.0f, "%.1f EV");
		ImGui::EndDisabled(); ImGui::SameLine(); CustomUI::Checkbox("Auto EV Compensation", &autoEVCompensation);

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
