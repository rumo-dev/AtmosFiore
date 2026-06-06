#pragma once
#include "Engine/utilities/dx_common.h"
#include "light_base.h"

/**
 * @brief スポットライトクラス
 *
 * 指定方向に円錐状に光を照らすライト
 */
class SpotLight : public Light_Base
{
public:
	/**
	 * @brief コンストラクタ
	 */
	SpotLight()
	{
		position = { 0, 0, 0 };
		direction = { 0, -1, 0 };
		radius = 10.0f;
		intensity = 1.0f;
		innerAngle = 0.5f;
		outerAngle = 0.7f;
		diffuseColor = { 1.0f, 1.0f, 1.0f , 1.0f };
	}

	/**
	 * @brief GPU転送用構造体
	 */
	struct SpotLight_GPU
	{
		dx::XMFLOAT3 position;
		float radius;

		dx::XMFLOAT3 direction;
		float intensity;

		dx::XMFLOAT3 color;
		float innerAngle;

		float outerAngle;
		float padding[3];
	};

	// 位置・方向・半径・強度・角度
	dx::XMFLOAT3 position;
	dx::XMFLOAT3 direction;
	float radius;

	float intensity;
	float innerAngle;
	float outerAngle;
	float padding[1] = {};

	/**
	 * @brief 更新処理
	 */
	void update() override
	{
		// 必要ならアニメーションやカメラ依存処理を書く
	}

	/**
	 * @brief ImGui描画
	 * @param label ラベル名
	 */
	void draw_imgui(const char* label = "SpotLight") override
	{
		if (ImGui::TreeNode(label))
		{
			ImGui::ColorEdit3("Diffuse", (float*)&diffuseColor);

			ImGui::DragFloat3("Position", (float*)&position, 0.1f);
			ImGui::DragFloat3("Direction", (float*)&direction, 0.1f);
			ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 200.0f);
			ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("Inner Angle", &innerAngle, 0.01f, 0.0f, 3.14f);
			ImGui::DragFloat("Outer Angle", &outerAngle, 0.01f, 0.0f, 3.14f);

			ImGui::TreePop();
		}
	}
};
