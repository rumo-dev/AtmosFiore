#pragma once
#include "Engine/utilities/dx_common.h"
#include "light_base.h"

/**
 * @brief エリアライトの形状タイプ
 */
enum class AreaLightShape : uint32_t
{
	Rectangle = 0,	///< 矩形
	Disk = 1,		///< 円形（ディスク）
	Sphere = 2		///< 球形
};

/**
 * @brief エリアライトクラス
 *
 * 矩形・円形・球形の面光源を表現するライト
 * ソフトシャドウやよりリアルなライティングを実現する
 */
class AreaLight : public Light_Base
{
public:
	/**
	 * @brief コンストラクタ
	 */
	AreaLight()
	{
		position = { 0, 0, 0 };
		direction = { 0, -1, 0 };
		right = { 1, 0, 0 };
		width = 2.0f;
		height = 2.0f;
		radius = 1.0f;
		shape = AreaLightShape::Rectangle;
		intensity = 1.0f;
		diffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	}

	/**
	 * @brief GPU転送用構造体
	 */
	struct AreaLight_GPU
	{
		dx::XMFLOAT3 position;
		float width;

		dx::XMFLOAT3 direction;
		float height;

		dx::XMFLOAT3 right;
		float radius;

		uint32_t shape;
		float intensity;
		uint32_t padding1;
		uint32_t padding2;

		dx::XMFLOAT3 color;
		float padding3;
	};

	// 位置・方向・サイズ・強度
	dx::XMFLOAT3 position;
	dx::XMFLOAT3 direction;
	dx::XMFLOAT3 right;
	float width;
	float height;
	float radius;
	AreaLightShape shape;
	float intensity;
	float padding[2] = {};

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
	void draw_imgui(const char* label = "AreaLight") override
	{
		if (ImGui::TreeNode(label))
		{
			ImGui::ColorEdit3("Diffuse", (float*)&diffuseColor);

			ImGui::DragFloat3("Position", (float*)&position, 0.1f);
			ImGui::DragFloat3("Direction", (float*)&direction, 0.1f);
			ImGui::DragFloat3("Right", (float*)&right, 0.1f);

			// 形状選択
			const char* shapeItems[] = { "Rectangle", "Disk", "Sphere" };
			int currentShape = static_cast<int>(shape);
			if (ImGui::Combo("Shape", &currentShape, shapeItems, 3))
			{
				shape = static_cast<AreaLightShape>(currentShape);
			}

			// 形状に応じたパラメータ表示
			if (shape == AreaLightShape::Rectangle)
			{
				ImGui::DragFloat("Width", &width, 0.1f, 0.1f, 50.0f);
				ImGui::DragFloat("Height", &height, 0.1f, 0.1f, 50.0f);
			}
			else if (shape == AreaLightShape::Disk || shape == AreaLightShape::Sphere)
			{
				ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 50.0f);
			}

			ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.0f, 10.0f);

			ImGui::TreePop();
		}
	}
};
