#pragma once

#include <DirectXMath.h>
#include <cmath>
#include "Engine/utilities/dx_common.h"

#define PI (dx::XM_PI)

/// @brief 度をラジアンに変換
/// @param degrees 角度（度）
constexpr float ToRadians(float degrees) {
	return degrees * (PI / 180.0f);
}

/// @brief ラジアンを度に変換
/// @param radians 角度（ラジアン）
constexpr float ToDegrees(float radians) {
	return radians * (180.0f / PI);
}

/// @brief 3Dベクトル生成
/// @param x X成分
/// @param y Y成分
/// @param z Z成分
/// @return dx::XMVECTOR 3Dベクトル（w=0）
inline dx::XMVECTOR XMVector3(float x, float y, float z) {
	return dx::XMVectorSet(x, y, z, 0.0f);
}

/// @brief 4Dベクトル生成
/// @param x X成分
/// @param y Y成分
/// @param z Z成分
/// @param w W成分
/// @return dx::XMVECTOR 4Dベクトル
inline dx::XMVECTOR XMVector4(float x, float y, float z, float w) {
	return dx::XMVectorSet(x, y, z, w);
}

/// @brief ベクトルの正規化
/// @param v 入力ベクトル
/// @return dx::XMVECTOR 正規化済みベクトル
inline dx::XMVECTOR Normalize(const dx::XMVECTOR& v) {
	return dx::XMVector3Normalize(v);
}

/// @brief ベクトルの長さ（ノルム）
/// @param v 入力ベクトル
/// @return float ベクトルの長さ
inline float Length(const dx::XMVECTOR& v) {
	return dx::XMVectorGetX(dx::XMVector3Length(v));
}

/// @brief 平行移動行列生成
/// @param x X方向
/// @param y Y方向
/// @param z Z方向
/// @return dx::XMMATRIX 平行移動行列
inline dx::XMMATRIX XMTranslate(float x, float y, float z) {
	return dx::XMMatrixTranslation(x, y, z);
}

/// @brief X軸回転行列生成
/// @param rad 回転角（ラジアン）
/// @return dx::XMMATRIX X軸回転行列
inline dx::XMMATRIX XMRotateX(float rad) {
	return dx::XMMatrixRotationX(rad);
}

/// @brief Y軸回転行列生成
/// @param rad 回転角（ラジアン）
/// @return dx::XMMATRIX Y軸回転行列
inline dx::XMMATRIX XMRotateY(float rad) {
	return dx::XMMatrixRotationY(rad);
}

/// @brief Z軸回転行列生成
/// @param rad 回転角（ラジアン）
/// @return dx::XMMATRIX Z軸回転行列
inline dx::XMMATRIX XMRotateZ(float rad) {
	return dx::XMMatrixRotationZ(rad);
}

/// @brief スケーリング行列生成
/// @param x X方向スケール
/// @param y Y方向スケール
/// @param z Z方向スケール
/// @return dx::XMMATRIX スケーリング行列
inline dx::XMMATRIX XMScale(float x, float y, float z) {
	return dx::XMMatrixScaling(x, y, z);
}

/// @brief スケール・回転・平行移動を合成した行列生成 (S * R * T)
/// @param sx Xスケール
/// @param sy Yスケール
/// @param sz Zスケール
/// @param rx X回転（ラジアン）
/// @param ry Y回転（ラジアン）
/// @param rz Z回転（ラジアン）
/// @param tx 平行移動X
/// @param ty 平行移動Y
/// @param tz 平行移動Z
/// @return dx::XMMATRIX 合成行列
inline dx::XMMATRIX XMComposeSRT(float sx, float sy, float sz, float rx, float ry, float rz, float tx, float ty, float tz) {
	return XMScale(sx, sy, sz) *
		XMRotateX(rx) * XMRotateY(ry) * XMRotateZ(rz) *
		XMTranslate(tx, ty, tz);
}

/// @brief LookAt行列生成（視点、注視点、上方向）
/// @param eye カメラ位置
/// @param target 注視点
/// @param up 上方向ベクトル
/// @return dx::XMMATRIX LookAt行列（左手座標系）
inline dx::XMMATRIX XMLookAt(const dx::XMVECTOR& eye, const dx::XMVECTOR& target, const dx::XMVECTOR& up) {
	return dx::XMMatrixLookAtLH(eye, target, up);
}

/// @brief 射影行列生成（透視投影）
/// @param fovY 垂直方向視野角（ラジアン）
/// @param aspect アスペクト比（幅/高さ）
/// @param nearZ 近平面
/// @param farZ 遠平面
/// @return dx::XMMATRIX 射影行列（左手座標系）
inline dx::XMMATRIX XMProjectionFov(float fovY, float aspect, float nearZ, float farZ) {
	return dx::XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
}

// Frustum utilities
struct FrustumPlane {
	dx::XMVECTOR normal; // (a,b,c)
	float distance;      // d
};

struct Frustum {
	FrustumPlane planes[6]; // left, right, top, bottom, near, far
};

// Extract frustum planes from view-projection matrix (in LH, row-major as DirectX XMMATRIX)
inline Frustum ExtractFrustum(const dx::XMMATRIX& view, const dx::XMMATRIX& proj)
{
	Frustum f;
	dx::XMMATRIX m = view * proj; // Note: use view * proj consistent with projection of points
	// Access elements
	const float* mat = reinterpret_cast<const float*>(&m);
	// Left: row4 + row1
	f.planes[0].normal = dx::XMVectorSet(mat[3] + mat[0], mat[7] + mat[4], mat[11] + mat[8], 0.0f);
	f.planes[0].distance = mat[15] + mat[12];
	// Right: row4 - row1
	f.planes[1].normal = dx::XMVectorSet(mat[3] - mat[0], mat[7] - mat[4], mat[11] - mat[8], 0.0f);
	f.planes[1].distance = mat[15] - mat[12];
	// Top: row4 - row2
	f.planes[2].normal = dx::XMVectorSet(mat[3] - mat[1], mat[7] - mat[5], mat[11] - mat[9], 0.0f);
	f.planes[2].distance = mat[15] - mat[13];
	// Bottom: row4 + row2
	f.planes[3].normal = dx::XMVectorSet(mat[3] + mat[1], mat[7] + mat[5], mat[11] + mat[9], 0.0f);
	f.planes[3].distance = mat[15] + mat[13];
	// Near: row3
	f.planes[4].normal = dx::XMVectorSet(mat[2], mat[6], mat[10], 0.0f);
	f.planes[4].distance = mat[14];
	// Far: row4 - row3
	f.planes[5].normal = dx::XMVectorSet(mat[3] - mat[2], mat[7] - mat[6], mat[11] - mat[10], 0.0f);
	f.planes[5].distance = mat[15] - mat[14];

	// Normalize
	for (int i = 0; i < 6; ++i)
	{
		dx::XMVECTOR n = f.planes[i].normal;
		float len = dx::XMVectorGetX(dx::XMVector3Length(n));
		if (len > 0.0f) {
			f.planes[i].normal = dx::XMVectorScale(n, 1.0f / len);
			f.planes[i].distance /= len;
		}
	}

	return f;
}

// Extract frustum directly from a combined view-projection matrix
inline Frustum ExtractFrustumFromMatrix(const dx::XMMATRIX& m)
{
	Frustum f;
	const float* mat = reinterpret_cast<const float*>(&m);
	// Left: row4 + row1
	f.planes[0].normal = dx::XMVectorSet(mat[3] + mat[0], mat[7] + mat[4], mat[11] + mat[8], 0.0f);
	f.planes[0].distance = mat[15] + mat[12];
	// Right: row4 - row1
	f.planes[1].normal = dx::XMVectorSet(mat[3] - mat[0], mat[7] - mat[4], mat[11] - mat[8], 0.0f);
	f.planes[1].distance = mat[15] - mat[12];
	// Top: row4 - row2
	f.planes[2].normal = dx::XMVectorSet(mat[3] - mat[1], mat[7] - mat[5], mat[11] - mat[9], 0.0f);
	f.planes[2].distance = mat[15] - mat[13];
	// Bottom: row4 + row2
	f.planes[3].normal = dx::XMVectorSet(mat[3] + mat[1], mat[7] + mat[5], mat[11] + mat[9], 0.0f);
	f.planes[3].distance = mat[15] + mat[13];
	// Near: row3
	f.planes[4].normal = dx::XMVectorSet(mat[2], mat[6], mat[10], 0.0f);
	f.planes[4].distance = mat[14];
	// Far: row4 - row3
	f.planes[5].normal = dx::XMVectorSet(mat[3] - mat[2], mat[7] - mat[6], mat[11] - mat[10], 0.0f);
	f.planes[5].distance = mat[15] - mat[14];

	// Normalize
	for (int i = 0; i < 6; ++i)
	{
		dx::XMVECTOR n = f.planes[i].normal;
		float len = dx::XMVectorGetX(dx::XMVector3Length(n));
		if (len > 0.0f) {
			f.planes[i].normal = dx::XMVectorScale(n, 1.0f / len);
			f.planes[i].distance /= len;
		}
	}

	return f;
}

// Test AABB (min/max) in world space against frustum. Returns true if intersects or inside.
inline bool AABBIntersectsFrustum(const Frustum& f, const dx::XMVECTOR& bboxMin, const dx::XMVECTOR& bboxMax)
{
	// For each plane, test the positive vertex
	for (int i = 0; i < 6; ++i)
	{
		dx::XMVECTOR n = f.planes[i].normal;
		// choose positive vertex
		dx::XMVECTOR p = dx::XMVectorSelect(bboxMin, bboxMax, dx::XMVectorGreater(n, dx::XMVectorZero()));
		float dist = dx::XMVectorGetX(dx::XMVector3Dot(n, p)) + f.planes[i].distance;
		if (dist < 0.0f) return false; // outside
	}
	return true;
}

// Test BoundingSphere in world space against frustum. Returns true if intersects or inside.
inline bool SphereIntersectsFrustum(const Frustum& f, const dx::XMVECTOR& center, float radius)
{
	for (int i = 0; i < 6; ++i)
	{
		float dist = dx::XMVectorGetX(dx::XMVector3Dot(f.planes[i].normal, center)) + f.planes[i].distance;
		if (dist < -radius)
		{
			return false; // sphere is completely behind this plane
		}
	}
	return true;
}


#include <random>

namespace Random
{
	// 乱数エンジン（1回だけ初期化）
	inline std::mt19937& engine()
	{
		static std::random_device rd;
		static std::mt19937 eng(rd());
		return eng;
	}

	// int の範囲乱数
	inline int Range(int min, int max)
	{
		std::uniform_int_distribution<int> dist(min, max);
		return dist(engine());
	}

	// float の範囲乱数
	inline float Range(float min, float max)
	{
		std::uniform_real_distribution<float> dist(min, max);
		return dist(engine());
	}
}
