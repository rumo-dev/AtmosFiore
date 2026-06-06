#pragma once
#include "camera.h"

/**
 * @brief カメラ制御基底クラス
 */
class Camera_Base {
public:
	virtual ~Camera_Base() = default;

	// 毎フレームのカメラ挙動更新（派生クラスでオーバーライド）
	virtual void update(float deltaTime) = 0;

	// カメラデータの取得
	Camera& get_camera() { return camera_; }
	const Camera& get_camera() const { return camera_; }
	virtual void draw_imgui() {}
protected:
	Camera camera_; // 派生クラスからアクセス可能なカメラデータ実体
};