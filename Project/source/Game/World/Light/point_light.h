#pragma once
#include "Engine/utilities/dx_common.h"
#include "light_base.h"

inline constexpr uint32_t POINT_LIGHT_SHADOW_ATLAS_GRID = 4;
inline constexpr uint32_t POINT_LIGHT_SHADOW_ATLAS_CAPACITY =
	POINT_LIGHT_SHADOW_ATLAS_GRID * POINT_LIGHT_SHADOW_ATLAS_GRID;

class PointLight : public LightBase
{
public:
	PointLight()
	{
		position = { 0, 0, 0 };
		radius = 10.0f;
	intensity = 10000.0f; // cd; calibrated for the renderer's metric-scale scenes
		diffuseColor = { 1.0f, 1.0f, 1.0f , 1.0f };
		cast_shadows = false;
	}

	struct PointLightGpu
	{
		dx::XMFLOAT3 position;
		float radius;

		dx::XMFLOAT3 color;
		float intensity;
		int32_t shadow_tile;
	uint32_t padding[3];
	};
	// 位置・半径・強度
	dx::XMFLOAT3 position;
	float radius;

	float intensity; // luminous intensity [cd]
	float padding[3] = {};
	bool cast_shadows = false;
	int32_t shadow_tile = -1; // frame-local slot in the shared dual-paraboloid atlas

	void update() override
	{
		// 必要ならアニメーションやカメラ依存処理を書く
	}

	void draw_imgui(const char* label = "PointLight") override
	{
		if (ImGui::TreeNode(label))
		{
			ImGui::ColorEdit3("Diffuse", (float*)&diffuseColor);
			ImGui::Checkbox("Show Wireframe", &debug_wireframe);
			ImGui::Checkbox("Cast Shadows", &cast_shadows);

			ImGui::DragFloat3("Position", (float*)&position, 0.1f);
			ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 200.0f);
			ImGui::DragFloat("Luminous Intensity (cd)", &intensity, 100.0f, 0.0f, 100000.0f, "%.0f cd");

			ImGui::TreePop();
		}
	}
};
