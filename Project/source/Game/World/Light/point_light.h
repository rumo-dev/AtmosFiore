#pragma once
#include "Engine/utilities/dx_common.h"
#include "light_base.h"

class PointLight : public Light_Base
{
public:
	PointLight()
	{
		position = { 0, 0, 0 };
		radius = 10.0f;
		intensity = 1.0f;
		diffuseColor = { 1.0f, 1.0f, 1.0f , 1.0f };
	}

	struct PointLight_GPU
	{
		dx::XMFLOAT3 position;
		float radius;

		dx::XMFLOAT3 color;
		float intensity;
	};
	// 位置・半径・強度
	dx::XMFLOAT3 position;
	float radius;

	float intensity;
	float padding[3] = {};

	void update() override
	{
		// 必要ならアニメーションやカメラ依存処理を書く
	}

	void draw_imgui(const char* label = "PointLight") override
	{
		if (ImGui::TreeNode(label))
		{
			ImGui::ColorEdit3("Diffuse", (float*)&diffuseColor);

			ImGui::DragFloat3("Position", (float*)&position, 0.1f);
			ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 200.0f);
			ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.0f, 10.0f);

			ImGui::TreePop();
		}
	}
};
