#pragma once
#include "Engine/utilities/dx_common.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"

//------------------------------------
// ライト基底クラス
//------------------------------------
class Light_Base
{
public:
	virtual ~Light_Base() = default;

	// 共通属性
	dx::XMFLOAT4 ambientColor = { 0.2f, 0.2f, 0.2f, 1.0f };
	dx::XMFLOAT4 diffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	dx::XMFLOAT4 specularColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 共通インターフェース
	virtual void update() = 0;
	virtual void draw_imgui(const char* label = "Light") = 0;
};