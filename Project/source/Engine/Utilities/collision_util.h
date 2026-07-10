#pragma once
#include <DirectXMath.h>
#include <vector>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

// Windows.h が NOMINMAX なしで先にインクルードされていると、
// std::min / std::max がマクロの min(a,b) / max(a,b) に置換されて
// 初期化子リスト版 std::min({...}) がマクロの引数不一致で
// コンパイルエラーになる。ここで明示的に無効化しておく。
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace dx = DirectX;

// ============================================================
//  コリジョン用プリミティブ構造体
// ============================================================

/// @brief レイ（半直線）
struct Ray
{
	dx::XMVECTOR origin;    ///< 始点（w=1）
	dx::XMVECTOR direction; ///< 方向（正規化済み, w=0）
};

/// @brief ワールド空間の三角形（頂点3つ）
struct CollisionTriangle
{
	dx::XMVECTOR v0, v1, v2;
};

/// @brief カプセル（軸は上下 Y 方向）
struct CollisionCapsule
{
	dx::XMVECTOR base;   ///< カプセル下端スフィア中心（w=1）
	dx::XMVECTOR tip;    ///< カプセル上端スフィア中心（w=1）
	float        radius; ///< 半径（m）
};

/// @brief ワールド空間の球体
struct CollisionSphere
{
	dx::XMVECTOR center;  ///< 中心座標（w=1）
	float        radius;  ///< 半径（m）
};

// ============================================================
//  レイキャスト結果
// ============================================================

struct RayHitResult
{
	bool         hit = false;
	float        t = FLT_MAX; ///< 始点からの距離
	dx::XMVECTOR point;            ///< ヒット座標
	dx::XMVECTOR normal;           ///< ヒット面の法線（外向き、正規化済み）
};

// ============================================================
//  RayCast vs Triangle (Möller–Trumbore)
// ============================================================

/**
 * @brief レイと三角形の交差判定（Möller–Trumbore アルゴリズム）
 *
 * 裏面カリングなし（両面判定）。
 *
 * @param ray       レイ（originとdirectionは事前に正規化されていること）
 * @param tri       ワールド空間の三角形
 * @param max_dist  判定最大距離（この距離を超えたヒットは無視）
 * @return RayHitResult
 */
inline RayHitResult RayCastTriangle(const Ray& ray, const CollisionTriangle& tri, float max_dist = FLT_MAX)
{
	RayHitResult result;
	constexpr float EPSILON = 1e-6f;

	dx::XMVECTOR edge1 = dx::XMVectorSubtract(tri.v1, tri.v0);
	dx::XMVECTOR edge2 = dx::XMVectorSubtract(tri.v2, tri.v0);
	dx::XMVECTOR h = dx::XMVector3Cross(ray.direction, edge2);
	float        a = dx::XMVectorGetX(dx::XMVector3Dot(edge1, h));

	if (fabsf(a) < EPSILON) return result; // 平行

	float        f = 1.0f / a;
	dx::XMVECTOR s = dx::XMVectorSubtract(ray.origin, tri.v0);
	float        u = f * dx::XMVectorGetX(dx::XMVector3Dot(s, h));
	if (u < 0.0f || u > 1.0f) return result;

	dx::XMVECTOR q = dx::XMVector3Cross(s, edge1);
	float        v = f * dx::XMVectorGetX(dx::XMVector3Dot(ray.direction, q));
	if (v < 0.0f || u + v > 1.0f) return result;

	float t = f * dx::XMVectorGetX(dx::XMVector3Dot(edge2, q));
	if (t < EPSILON || t > max_dist) return result;

	result.hit = true;
	result.t = t;
	result.point = dx::XMVectorAdd(ray.origin, dx::XMVectorScale(ray.direction, t));
	result.normal = dx::XMVector3Normalize(dx::XMVector3Cross(edge1, edge2));

	// レイが面の裏から入っていたら法線を反転
	if (dx::XMVectorGetX(dx::XMVector3Dot(result.normal, ray.direction)) > 0.0f)
		result.normal = dx::XMVectorNegate(result.normal);

	return result;
}

// ============================================================
//  カプセル vs 三角形（スフィア掃引 vs 三角形）
// ============================================================

/**
 * @brief 移動するスフィアと三角形の交差判定
 *
 * 三角形の平面をスフィア半径分オフセットして面ヒット判定。
 * 三角形外の場合は各エッジに対してスフィアキャストを試みる。
 *
 * @note
 * 三角形の巻き順（表裏）に関わらず正しく判定できるよう、
 * 法線は常にレイ始点（＝スフィア側）を向くように補正してから使用する。
 * これにより、glTFメッシュの巻き順が不揃いな場合でも
 * 「衝突検知はしたが法線の向きが逆で補正がスキップされる」
 * というすり抜けバグを防ぐ。
 *
 * @param ray      スフィア中心の移動レイ（正規化方向）
 * @param radius   スフィア半径
 * @param tri      ワールド空間の三角形
 * @param max_dist 判定最大距離
 */
inline RayHitResult SphereCastTriangle(const Ray& ray, float radius,
	const CollisionTriangle& tri, float max_dist = FLT_MAX)
{
	RayHitResult result;
	constexpr float EPSILON = 1e-6f;

	// 三角形の法線
	dx::XMVECTOR edge1 = dx::XMVectorSubtract(tri.v1, tri.v0);
	dx::XMVECTOR edge2 = dx::XMVectorSubtract(tri.v2, tri.v0);
	dx::XMVECTOR normal = dx::XMVector3Normalize(dx::XMVector3Cross(edge1, edge2));

	// ---- 巻き順依存バグ修正 -----------------------------------------
	// cross(edge1, edge2) の結果はメッシュの巻き順次第でどちらを向くか
	// 不定なため、必ず「レイ始点（スフィア）側」を向くように補正する。
	// これをやらないと、法線が逆向きの三角形にヒットした際に
	// CapsuleCastTriangles 側の dot 判定が誤り、補正が一切効かず
	// すり抜けてしまう。
	dx::XMVECTOR to_origin = dx::XMVectorSubtract(ray.origin, tri.v0);
	if (dx::XMVectorGetX(dx::XMVector3Dot(normal, to_origin)) < 0.0f)
	{
		normal = dx::XMVectorNegate(normal);
	}
	// -------------------------------------------------------------------

	float nd = dx::XMVectorGetX(dx::XMVector3Dot(normal, ray.direction));
	if (fabsf(nd) < EPSILON) return result; // 平行

	// 面をスフィア半径分オフセットして RayCast
	dx::XMVECTOR offset_plane_origin = dx::XMVectorAdd(tri.v0, dx::XMVectorScale(normal, radius));
	float plane_d = dx::XMVectorGetX(dx::XMVector3Dot(normal, offset_plane_origin));
	float t_plane = (plane_d - dx::XMVectorGetX(dx::XMVector3Dot(normal, ray.origin))) / nd;

	if (t_plane < 0.0f || t_plane > max_dist) return result;

	// ヒット時のスフィア中心
	dx::XMVECTOR sphere_center = dx::XMVectorAdd(ray.origin, dx::XMVectorScale(ray.direction, t_plane));

	// バリセントリック座標で三角形内外判定
	dx::XMVECTOR c0 = dx::XMVector3Cross(edge1, dx::XMVectorSubtract(sphere_center, tri.v0));
	dx::XMVECTOR c1 = dx::XMVector3Cross(dx::XMVectorSubtract(tri.v2, tri.v1),
		dx::XMVectorSubtract(sphere_center, tri.v1));
	dx::XMVECTOR c2 = dx::XMVector3Cross(dx::XMVectorSubtract(tri.v0, tri.v2),
		dx::XMVectorSubtract(sphere_center, tri.v2));

	bool inside = (dx::XMVectorGetX(dx::XMVector3Dot(c0, normal)) >= 0.0f) &&
		(dx::XMVectorGetX(dx::XMVector3Dot(c1, normal)) >= 0.0f) &&
		(dx::XMVectorGetX(dx::XMVector3Dot(c2, normal)) >= 0.0f);

	if (inside)
	{
		result.hit = true;
		result.t = t_plane;
		result.point = dx::XMVectorSubtract(sphere_center, dx::XMVectorScale(normal, radius));
		result.normal = (nd > 0.0f) ? dx::XMVectorNegate(normal) : normal;
		return result;
	}

	// エッジに対するスフィア判定
	const dx::XMVECTOR verts[3] = { tri.v0, tri.v1, tri.v2 };
	const dx::XMVECTOR edges[3] = {
		dx::XMVectorSubtract(tri.v1, tri.v0),
		dx::XMVectorSubtract(tri.v2, tri.v1),
		dx::XMVectorSubtract(tri.v0, tri.v2)
	};

	float            best_t = FLT_MAX;
	dx::XMVECTOR     best_normal = dx::XMVectorZero();

	for (int e = 0; e < 3; ++e)
	{
		dx::XMVECTOR ev = edges[e];
		dx::XMVECTOR ev_n = dx::XMVector3Normalize(ev);
		float        ev_len = dx::XMVectorGetX(dx::XMVector3Length(ev));

		dx::XMVECTOR w = dx::XMVectorSubtract(ray.origin, verts[e]);
		float rd_ev = dx::XMVectorGetX(dx::XMVector3Dot(ray.direction, ev_n));
		float a = 1.0f - rd_ev * rd_ev;
		float b = 2.0f * (dx::XMVectorGetX(dx::XMVector3Dot(ray.direction, w))
			- rd_ev * dx::XMVectorGetX(dx::XMVector3Dot(w, ev_n)));
		float c = dx::XMVectorGetX(dx::XMVector3Dot(w, w))
			- powf(dx::XMVectorGetX(dx::XMVector3Dot(w, ev_n)), 2.0f)
			- radius * radius;

		float discriminant = b * b - 4.0f * a * c;
		if (discriminant < 0.0f || fabsf(a) < EPSILON) continue;

		float t_hit = (-b - sqrtf(discriminant)) / (2.0f * a);
		if (t_hit < 0.0f || t_hit > max_dist || t_hit >= best_t) continue;

		dx::XMVECTOR hit_center = dx::XMVectorAdd(ray.origin, dx::XMVectorScale(ray.direction, t_hit));
		float proj = dx::XMVectorGetX(dx::XMVector3Dot(
			dx::XMVectorSubtract(hit_center, verts[e]), ev_n));
		if (proj < 0.0f || proj > ev_len) continue;

		dx::XMVECTOR closest_on_edge = dx::XMVectorAdd(verts[e], dx::XMVectorScale(ev_n, proj));
		dx::XMVECTOR en = dx::XMVector3Normalize(dx::XMVectorSubtract(hit_center, closest_on_edge));

		best_t = t_hit;
		best_normal = en;
	}

	if (best_t < FLT_MAX)
	{
		result.hit = true;
		result.t = best_t;
		result.point = dx::XMVectorAdd(ray.origin, dx::XMVectorScale(ray.direction, best_t));
		result.normal = best_normal;
	}

	return result;
}

// ============================================================
//  カプセル vs 三角形リスト（壁ずり補正付き）
// ============================================================

/**
 * @brief 三角形リストに対してカプセルキャストを行い、壁ずり補正後の移動ベクトルを返す
 *
 * カプセルの上端・下端スフィアそれぞれで SphereCastTriangle を実行し、
 * 最も近い衝突面の法線に対して移動ベクトルの法線成分を除去（壁ずり）する。
 * max_iterations 回反復することでコーナー処理も行う。
 *
 * @param capsule        現在フレームのカプセル（移動前）
 * @param move_delta     今フレームの移動ベクトル
 * @param triangles      ワールド空間三角形頂点リスト（3頂点で1三角形）
 * @param out_corrected  補正後の移動ベクトル
 * @param max_iterations 壁ずり反復回数（通常 3 で十分）
 */
inline void CapsuleCastTriangles(
	const CollisionCapsule& capsule,
	const dx::XMVECTOR& move_delta,
	const std::vector<dx::XMFLOAT3>& triangles,
	dx::XMVECTOR& out_corrected,
	int                                    max_iterations = 3)
{
	out_corrected = move_delta;

	float move_len = dx::XMVectorGetX(dx::XMVector3Length(out_corrected));
	if (move_len < 1e-6f) return;

	// カプセルの現在の位置と移動後の位置を包む最大AABBを計算する
	dx::XMFLOAT3 cb, ct;
	dx::XMStoreFloat3(&cb, capsule.base);
	dx::XMStoreFloat3(&ct, capsule.tip);

	float r = capsule.radius;
	float cap_min_x = (std::min)(cb.x, ct.x) - r;
	float cap_max_x = (std::max)(cb.x, ct.x) + r;
	float cap_min_y = (std::min)(cb.y, ct.y) - r;
	float cap_max_y = (std::max)(cb.y, ct.y) + r;
	float cap_min_z = (std::min)(cb.z, ct.z) - r;
	float cap_max_z = (std::max)(cb.z, ct.z) + r;

	dx::XMFLOAT3 d;
	dx::XMStoreFloat3(&d, move_delta);

	float sweep_min_x = (std::min)(cap_min_x, cap_min_x + d.x);
	float sweep_max_x = (std::max)(cap_max_x, cap_max_x + d.x);
	float sweep_min_y = (std::min)(cap_min_y, cap_min_y + d.y);
	float sweep_max_y = (std::max)(cap_max_y, cap_max_y + d.y);
	float sweep_min_z = (std::min)(cap_min_z, cap_min_z + d.z);
	float sweep_max_z = (std::max)(cap_max_z, cap_max_z + d.z);

	// 最大AABBと交差する三角形だけをローカルバッファに抽出する
	// アロケーションを避けるため thread_local vector を再利用
	thread_local std::vector<CollisionTriangle> culled_triangles;
	culled_triangles.clear();

	size_t tri_count = triangles.size() / 3;
	for (size_t ti = 0; ti < tri_count; ++ti)
	{
		const dx::XMFLOAT3& p0 = triangles[ti * 3 + 0];
		const dx::XMFLOAT3& p1 = triangles[ti * 3 + 1];
		const dx::XMFLOAT3& p2 = triangles[ti * 3 + 2];

		float tri_min_x = (std::min)({ p0.x, p1.x, p2.x });
		float tri_max_x = (std::max)({ p0.x, p1.x, p2.x });
		float tri_min_y = (std::min)({ p0.y, p1.y, p2.y });
		float tri_max_y = (std::max)({ p0.y, p1.y, p2.y });
		float tri_min_z = (std::min)({ p0.z, p1.z, p2.z });
		float tri_max_z = (std::max)({ p0.z, p1.z, p2.z });

		// AABB交差判定
		if (tri_max_x < sweep_min_x || tri_min_x > sweep_max_x ||
			tri_max_y < sweep_min_y || tri_min_y > sweep_max_y ||
			tri_max_z < sweep_min_z || tri_min_z > sweep_max_z)
		{
			continue;
		}

		culled_triangles.push_back({
			dx::XMLoadFloat3(&p0),
			dx::XMLoadFloat3(&p1),
			dx::XMLoadFloat3(&p2)
			});
	}

	if (culled_triangles.empty()) return;

	for (int iter = 0; iter < max_iterations; ++iter)
	{
		float iter_len = dx::XMVectorGetX(dx::XMVector3Length(out_corrected));
		if (iter_len < 1e-6f) break;

		dx::XMVECTOR dir = dx::XMVector3Normalize(out_corrected);

		// カプセルの上下2スフィアのレイ
		Ray ray_bot{ capsule.base, dir };
		Ray ray_top{ capsule.tip,  dir };

		float        best_t = iter_len;
		dx::XMVECTOR best_n = dx::XMVectorZero();
		bool         hit_any = false;

		for (const auto& tri : culled_triangles)
		{
			// 下スフィア
			RayHitResult r0 = SphereCastTriangle(ray_bot, capsule.radius, tri, best_t);
			if (r0.hit && r0.t < best_t)
			{
				best_t = r0.t;
				best_n = r0.normal;
				hit_any = true;
			}

			// 上スフィア
			RayHitResult r1 = SphereCastTriangle(ray_top, capsule.radius, tri, best_t);
			if (r1.hit && r1.t < best_t)
			{
				best_t = r1.t;
				best_n = r1.normal;
				hit_any = true;
			}
		}

		if (!hit_any) break;

		// 壁ずり：法線方向成分を除去
		float dot = dx::XMVectorGetX(dx::XMVector3Dot(out_corrected, best_n));
		if (dot < 0.0f)
		{
			out_corrected = dx::XMVectorSubtract(
				out_corrected,
				dx::XMVectorScale(best_n, dot)
			);
		}
		else
		{
			// ---- 貫通防止クランプ -----------------------------------
			// 法線とdotが非負（＝壁から離れる方向）でも、衝突自体は
			// 検出されているため、念のため移動距離をヒット距離で
			// クランプしてから抜ける。これにより、万一 best_n の向きが
			// 想定と異なる場合でも move_delta がそのまま素通りして
			// 貫通することを防ぐ。
			out_corrected = dx::XMVectorScale(dir, best_t);
			break;
			// -----------------------------------------------------------
		}
	}
}

// ============================================================
//  球体 vs 三角形リスト（壁ずり補正付き）
// ============================================================

/**
 * @brief 三角形リストに対して球体キャストを行い、壁ずり補正後の移動ベクトルを返す
 *
 * 球体で SphereCastTriangle を実行し、
 * 最も近い衝突面の法線に対して移動ベクトルの法線成分を除去（壁ずり）する。
 * max_iterations 回反復することでコーナー処理も行う。
 *
 * @param sphere         現在フレームの球体（移動前）
 * @param move_delta     今フレームの移動ベクトル
 * @param triangles      ワールド空間三角形頂点リスト（3頂点で1三角形）
 * @param out_corrected  補正後の移動ベクトル
 * @param max_iterations 壁ずり反復回数（通常 3 で十分）
 */
inline void SphereCastTriangles(
	const CollisionSphere& sphere,
	const dx::XMVECTOR& move_delta,
	const std::vector<dx::XMFLOAT3>& triangles,
	dx::XMVECTOR& out_corrected,
	int max_iterations = 3)
{
	out_corrected = move_delta;

	float move_len = dx::XMVectorGetX(dx::XMVector3Length(out_corrected));
	if (move_len < 1e-6f) return;

	// ---- 球体の現在位置と移動後の位置を包む最大AABBを計算 ----
	dx::XMFLOAT3 center_f;
	dx::XMStoreFloat3(&center_f, sphere.center);

	float r = sphere.radius;
	float sphere_min_x = center_f.x - r;
	float sphere_max_x = center_f.x + r;
	float sphere_min_y = center_f.y - r;
	float sphere_max_y = center_f.y + r;
	float sphere_min_z = center_f.z - r;
	float sphere_max_z = center_f.z + r;

	dx::XMFLOAT3 d;
	dx::XMStoreFloat3(&d, move_delta);

	float sweep_min_x = (std::min)(sphere_min_x, sphere_min_x + d.x);
	float sweep_max_x = (std::max)(sphere_max_x, sphere_max_x + d.x);
	float sweep_min_y = (std::min)(sphere_min_y, sphere_min_y + d.y);
	float sweep_max_y = (std::max)(sphere_max_y, sphere_max_y + d.y);
	float sweep_min_z = (std::min)(sphere_min_z, sphere_min_z + d.z);
	float sweep_max_z = (std::max)(sphere_max_z, sphere_max_z + d.z);

	// ---- AABB交差する三角形だけを抽出 ----
	thread_local std::vector<CollisionTriangle> culled_triangles;
	culled_triangles.clear();

	size_t tri_count = triangles.size() / 3;
	for (size_t ti = 0; ti < tri_count; ++ti)
	{
		const dx::XMFLOAT3& p0 = triangles[ti * 3 + 0];
		const dx::XMFLOAT3& p1 = triangles[ti * 3 + 1];
		const dx::XMFLOAT3& p2 = triangles[ti * 3 + 2];

		float tri_min_x = (std::min)({ p0.x, p1.x, p2.x });
		float tri_max_x = (std::max)({ p0.x, p1.x, p2.x });
		float tri_min_y = (std::min)({ p0.y, p1.y, p2.y });
		float tri_max_y = (std::max)({ p0.y, p1.y, p2.y });
		float tri_min_z = (std::min)({ p0.z, p1.z, p2.z });
		float tri_max_z = (std::max)({ p0.z, p1.z, p2.z });

		if (tri_max_x < sweep_min_x || tri_min_x > sweep_max_x ||
			tri_max_y < sweep_min_y || tri_min_y > sweep_max_y ||
			tri_max_z < sweep_min_z || tri_min_z > sweep_max_z)
		{
			continue;
		}

		culled_triangles.push_back({
			dx::XMLoadFloat3(&p0),
			dx::XMLoadFloat3(&p1),
			dx::XMLoadFloat3(&p2)
			});
	}

	if (culled_triangles.empty()) return;

	// ---- 壁ずり反復 ----
	dx::XMVECTOR current_pos = sphere.center;

	for (int iter = 0; iter < max_iterations; ++iter)
	{
		float iter_len = dx::XMVectorGetX(dx::XMVector3Length(out_corrected));
		if (iter_len < 1e-6f) break;

		dx::XMVECTOR dir = dx::XMVector3Normalize(out_corrected);

		// 球体の中心レイ
		Ray sphere_ray{ current_pos, dir };

		float        best_t = iter_len;
		dx::XMVECTOR best_n = dx::XMVectorZero();
		bool         hit_any = false;

		for (const auto& tri : culled_triangles)
		{
			RayHitResult r = SphereCastTriangle(sphere_ray, sphere.radius, tri, best_t);
			if (r.hit && r.t < best_t)
			{
				best_t = r.t;
				best_n = r.normal;
				hit_any = true;
			}
		}

		if (!hit_any) break;

		// 壁ずり：法線方向成分を除去
		float dot = dx::XMVectorGetX(dx::XMVector3Dot(out_corrected, best_n));
		if (dot < 0.0f)
		{
			out_corrected = dx::XMVectorSubtract(
				out_corrected,
				dx::XMVectorScale(best_n, dot)
			);
			// 壁ずり後、次の反復のために現在位置をヒット地点まで進める
			current_pos = dx::XMVectorAdd(current_pos, dx::XMVectorScale(dir, best_t));
		}
		else
		{
			// 貫通防止クランプ
			out_corrected = dx::XMVectorScale(dir, best_t);
			break;
		}
	}
}

// ============================================================
//  接地レイキャスト（下方向レイ vs 三角形リスト）
// ============================================================

/**
 * @brief 指定座標から下方向へレイを飛ばし、最も近い床の Y 座標を返す
 *
 * @param from       レイ始点（w=1）
 * @param triangles  三角形頂点リスト（3頂点で1三角形）
 * @param max_dist   探索最大距離
 * @param out_y      検出した床の Y 座標（ヒットした場合のみ有効）
 * @return true: 床を検出、false: 検出なし
 */
inline bool GroundRayCast(
	const dx::XMVECTOR& from,
	const std::vector<dx::XMFLOAT3>& triangles,
	float                            max_dist,
	float& out_y)
{
	Ray down_ray{ from, dx::XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f) };

	float best_t = max_dist;
	bool  hit_any = false;

	dx::XMFLOAT3 f;
	dx::XMStoreFloat3(&f, from);

	float ray_min_x = f.x - 1e-4f;
	float ray_max_x = f.x + 1e-4f;
	float ray_min_y = f.y - max_dist;
	float ray_max_y = f.y;
	float ray_min_z = f.z - 1e-4f;
	float ray_max_z = f.z + 1e-4f;

	size_t tri_count = triangles.size() / 3;
	for (size_t ti = 0; ti < tri_count; ++ti)
	{
		const dx::XMFLOAT3& p0 = triangles[ti * 3 + 0];
		const dx::XMFLOAT3& p1 = triangles[ti * 3 + 1];
		const dx::XMFLOAT3& p2 = triangles[ti * 3 + 2];

		float tri_min_x = (std::min)({ p0.x, p1.x, p2.x });
		float tri_max_x = (std::max)({ p0.x, p1.x, p2.x });
		float tri_min_y = (std::min)({ p0.y, p1.y, p2.y });
		float tri_max_y = (std::max)({ p0.y, p1.y, p2.y });
		float tri_min_z = (std::min)({ p0.z, p1.z, p2.z });
		float tri_max_z = (std::max)({ p0.z, p1.z, p2.z });

		// AABB交差判定
		if (tri_max_x < ray_min_x || tri_min_x > ray_max_x ||
			tri_max_y < ray_min_y || tri_min_y > ray_max_y ||
			tri_max_z < ray_min_z || tri_min_z > ray_max_z)
		{
			continue;
		}

		CollisionTriangle tri{
			dx::XMLoadFloat3(&p0),
			dx::XMLoadFloat3(&p1),
			dx::XMLoadFloat3(&p2)
		};

		RayHitResult res = RayCastTriangle(down_ray, tri, best_t);
		if (res.hit && res.t < best_t)
		{
			best_t = res.t;
			hit_any = true;
		}
	}

	if (hit_any)
	{
		out_y = dx::XMVectorGetY(from) - best_t;
		return true;
	}
	return false;
}

/**
 * @brief 任意の方向にレイを飛ばし、最も近い衝突点情報を返す
 *
 * @param ray        レイ（始点と方向）
 * @param triangles  三角形頂点リスト（3頂点で1三角形）
 * @param max_dist   探索最大距離
 * @param out_result 衝突結果
 * @return true: ヒットあり、false: ヒットなし
 */
inline bool RayCastTriangles(
	const Ray& ray,
	const std::vector<dx::XMFLOAT3>& triangles,
	float                            max_dist,
	RayHitResult& out_result)
{
	out_result.hit = false;
	out_result.t = max_dist;

	dx::XMFLOAT3 origin_f, dir_f;
	dx::XMStoreFloat3(&origin_f, ray.origin);
	dx::XMStoreFloat3(&dir_f, ray.direction);

	// レイのAABB（簡易的な絞り込み用）
	float min_x = (std::min)(origin_f.x, origin_f.x + dir_f.x * max_dist) - 0.5f;
	float max_x = (std::max)(origin_f.x, origin_f.x + dir_f.x * max_dist) + 0.5f;
	float min_y = (std::min)(origin_f.y, origin_f.y + dir_f.y * max_dist) - 0.5f;
	float max_y = (std::max)(origin_f.y, origin_f.y + dir_f.y * max_dist) + 0.5f;
	float min_z = (std::min)(origin_f.z, origin_f.z + dir_f.z * max_dist) - 0.5f;
	float max_z = (std::max)(origin_f.z, origin_f.z + dir_f.z * max_dist) + 0.5f;

	size_t tri_count = triangles.size() / 3;
	bool hit_any = false;

	for (size_t ti = 0; ti < tri_count; ++ti)
	{
		const dx::XMFLOAT3& p0 = triangles[ti * 3 + 0];
		const dx::XMFLOAT3& p1 = triangles[ti * 3 + 1];
		const dx::XMFLOAT3& p2 = triangles[ti * 3 + 2];

		float tri_min_x = (std::min)({ p0.x, p1.x, p2.x });
		float tri_max_x = (std::max)({ p0.x, p1.x, p2.x });
		float tri_min_y = (std::min)({ p0.y, p1.y, p2.y });
		float tri_max_y = (std::max)({ p0.y, p1.y, p2.y });
		float tri_min_z = (std::min)({ p0.z, p1.z, p2.z });
		float tri_max_z = (std::max)({ p0.z, p1.z, p2.z });

		// AABB交差判定
		if (tri_max_x < min_x || tri_min_x > max_x ||
			tri_max_y < min_y || tri_min_y > max_y ||
			tri_max_z < min_z || tri_min_z > max_z)
		{
			continue;
		}

		CollisionTriangle tri{
			dx::XMLoadFloat3(&p0),
			dx::XMLoadFloat3(&p1),
			dx::XMLoadFloat3(&p2)
		};

		RayHitResult res = RayCastTriangle(ray, tri, out_result.t);
		if (res.hit && res.t < out_result.t)
		{
			out_result = res;
			hit_any = true;
		}
	}

	return hit_any;
}

// ============================================================
//  空間グリッド（近傍三角形の高速絞り込み）
// ============================================================

/**
 * @brief XZ平面ベースの簡易空間グリッド
 *
 * @details
 * コリジョン三角形リストが大きい場合、毎フレーム全三角形を
 * CapsuleCastTriangles / GroundRayCast に渡すと極端に重くなる。
 * このクラスはあらかじめ三角形をセルに登録しておき、
 * プレイヤー周辺のセルだけから候補三角形を高速に取り出すために使う。
 *
 * @note スレッドセーフではない。build() と query() は同一スレッドから呼ぶこと。
 * @note query() が返す out には、参照元 build() に渡した vector への
 *       ポインタ寿命が有効な間のみ有効なデータが入る。
 */
class CollisionTriangleGrid
{
public:
	/**
	 * @brief 三角形リストからグリッドを構築する
	 * @param triangles 三角形頂点リスト（3頂点で1三角形、ワールド空間）
	 * @param cell_size セル1辺の長さ（m）。プレイヤーの移動速度や
	 *                  コリジョン検索半径に応じて調整する。
	 */
	void build(const std::vector<dx::XMFLOAT3>& triangles, float cell_size = 2.0f)
	{
		_cell_size = cell_size;
		_cells.clear();
		_triangles = &triangles;

		size_t tri_count = triangles.size() / 3;
		_cells.reserve(tri_count); // ざっくり予約

		_visited_tokens.assign(tri_count, 0);
		_query_token = 0;

		for (size_t ti = 0; ti < tri_count; ++ti)
		{
			const dx::XMFLOAT3& p0 = triangles[ti * 3 + 0];
			const dx::XMFLOAT3& p1 = triangles[ti * 3 + 1];
			const dx::XMFLOAT3& p2 = triangles[ti * 3 + 2];

			float min_x = (std::min)({ p0.x, p1.x, p2.x });
			float max_x = (std::max)({ p0.x, p1.x, p2.x });
			float min_z = (std::min)({ p0.z, p1.z, p2.z });
			float max_z = (std::max)({ p0.z, p1.z, p2.z });

			int cx0 = static_cast<int>(std::floor(min_x / _cell_size));
			int cx1 = static_cast<int>(std::floor(max_x / _cell_size));
			int cz0 = static_cast<int>(std::floor(min_z / _cell_size));
			int cz1 = static_cast<int>(std::floor(max_z / _cell_size));

			for (int cx = cx0; cx <= cx1; ++cx)
			{
				for (int cz = cz0; cz <= cz1; ++cz)
				{
					_cells[cell_key(cx, cz)].push_back(static_cast<int>(ti));
				}
			}
		}
	}

	/**
	 * @brief 指定座標の周辺（半径 radius 内のセル）にある三角形だけを取り出す
	 * @param center 検索中心座標（通常はプレイヤー座標）
	 * @param radius 検索半径（m）。カプセル半径＋今フレームの移動距離を
	 *               十分カバーできる値にすること（例: 5.0f 前後）。
	 * @param out    結果格納先（3頂点で1三角形、呼び出しごとにclearされる）
	 */
	void query(const dx::XMFLOAT3& center, float radius, std::vector<dx::XMFLOAT3>& out) const
	{
		out.clear();
		if (!_triangles || _triangles->empty()) return;

		int cx0 = static_cast<int>(std::floor((center.x - radius) / _cell_size));
		int cx1 = static_cast<int>(std::floor((center.x + radius) / _cell_size));
		int cz0 = static_cast<int>(std::floor((center.z - radius) / _cell_size));
		int cz1 = static_cast<int>(std::floor((center.z + radius) / _cell_size));

		_query_token++;
		if (_query_token == 0)
		{
			std::fill(_visited_tokens.begin(), _visited_tokens.end(), 0);
			_query_token = 1;
		}

		for (int cx = cx0; cx <= cx1; ++cx)
		{
			for (int cz = cz0; cz <= cz1; ++cz)
			{
				auto it = _cells.find(cell_key(cx, cz));
				if (it == _cells.end()) continue;

				for (int ti : it->second)
				{
					if (_visited_tokens[ti] == _query_token) continue;
					_visited_tokens[ti] = _query_token;
					out.push_back((*_triangles)[ti * 3 + 0]);
					out.push_back((*_triangles)[ti * 3 + 1]);
					out.push_back((*_triangles)[ti * 3 + 2]);
				}
			}
		}
	}

	/// グリッドが未構築、または三角形が0件かどうか
	bool empty() const { return !_triangles || _triangles->empty(); }

	/// 構築時に使用したセルサイズ
	float cell_size() const { return _cell_size; }

private:
	static int64_t cell_key(int cx, int cz)
	{
		return (static_cast<int64_t>(cx) << 32) ^ static_cast<uint32_t>(cz);
	}

	float _cell_size = 2.0f;
	const std::vector<dx::XMFLOAT3>* _triangles = nullptr;
	std::unordered_map<int64_t, std::vector<int>> _cells;

	mutable std::vector<uint32_t> _visited_tokens;
	mutable uint32_t _query_token = 0;
};