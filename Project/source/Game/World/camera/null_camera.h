#pragma once
#include "camera_base.h"
// 1. 何もしないダミーカメラクラス（ICameraを継承）
class NullCamera : public ICamera {
public:
	void update(float dt) override {} // 何もしない
	void draw_imgui() override { ImGui::Text("No camera active."); }
	// その他のメソッドも空の実装にする
};
