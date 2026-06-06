#pragma once
#include "light_base.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"
#include "Engine/Graphics/UI/ImGui/ImGuizmo.h"
#include "Game/World/camera/camera_manager.h" // カメラマネージャーを利用
#include "Engine/system/graphics_core.h"
#include <cmath>

class Directional_Light : public Light_Base
{
public:
	dx::XMFLOAT4 direction = { -0.5f, -1.0f, 0.5f, 0.0f }; // w=0

public:
	Directional_Light() = default;

	// 更新（GPU送信）
	void update() override
	{
		Graphics_Core::instance().set_directional_light(ambientColor, direction, diffuseColor);
	}

	void draw_imgui(const char* label = "DirectionalLight")
	{
		if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
			return;

		// 色編集
		if (ImGui::ColorEdit3("Ambient", (float*)&ambientColor)) update();
		if (ImGui::ColorEdit3("Diffuse", (float*)&diffuseColor)) update();

		// --- 現在アクティブなカメラをマネージャーから安全に参照する ---
		// ※もしマネージャーを使わない場合は、引数で const Camera& cam を受け取る形にしてください。
		const Camera& active_camera = Camera_Manager::instance().get_active_camera()->get_camera();

		// --- ギズモ操作用行列の構築 ---
		dx::XMVECTOR dirVec = dx::XMLoadFloat4(&direction);
		dirVec = dx::XMVector3Normalize(dirVec);

		// 真上・真下を向いた時の外積エラー（ゼロベクトル）対策
		dx::XMVECTOR up = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		if (std::fabs(dx::XMVectorGetX(dx::XMVector3Dot(dirVec, up))) > 0.99f)
		{
			up = dx::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // 平行なら前方向を基準にする
		}

		dx::XMVECTOR right = dx::XMVector3Normalize(dx::XMVector3Cross(up, dirVec));
		dx::XMVECTOR newUp = dx::XMVector3Cross(dirVec, right);

		dx::XMMATRIX lightMat = dx::XMMatrixIdentity();
		lightMat.r[0] = right;
		lightMat.r[1] = newUp;
		lightMat.r[2] = dirVec;
		lightMat.r[3] = dx::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f); // 原点固定

		dx::XMFLOAT4X4 lightMatrix;
		dx::XMStoreFloat4x4(&lightMatrix, lightMat);

		// ImGuizmo 描画準備
		ImGuizmo::SetDrawlist(); // 現在のウィンドウのDrawListを使用するように明示
		ImGuizmo::SetRect(0.0f, 0.0f, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
		ImGuizmo::SetGizmoSizeClipSpace(0.1f);

		// 最新のカメラ行列を取得
		float aspect_ratio = Graphics_Core::instance().get_aspect_ratio();
		dx::XMMATRIX view = active_camera.view;
		dx::XMMATRIX proj = active_camera.projection;

		dx::XMFLOAT4X4 viewFloats, projFloats;
		dx::XMStoreFloat4x4(&viewFloats, view);
		dx::XMStoreFloat4x4(&projFloats, proj);

		// ギズモの操作実行
		bool changed = ImGuizmo::Manipulate(
			&viewFloats.m[0][0],
			&projFloats.m[0][0],
			ImGuizmo::ROTATE,
			ImGuizmo::WORLD,
			&lightMatrix.m[0][0]
		);

		if (changed)
		{
			dx::XMMATRIX newMat = dx::XMLoadFloat4x4(&lightMatrix);
			dx::XMVECTOR newDir = newMat.r[2]; // Z軸がライトの向く方向
			newDir = dx::XMVector3Normalize(newDir);

			// w成分を0にして保存
			dx::XMStoreFloat4(&direction, newDir);
			direction.w = 0.0f;
			update();
		}

		// --- 矢印描画（ライト方向の可視化） ---
		{
			ImDrawList* drawList = ImGui::GetForegroundDrawList();

			// カメラの注視点（Target）を基準に矢印を描画すると画面外に消えにくくなります
			// ここでは元の仕様通り原点 (0,0,0) からのベクトルとして計算します
			dx::XMVECTOR start = dx::XMVectorZero();
			dx::XMVECTOR end = dx::XMVectorScale(dirVec, 2.0f); // 見えやすいように少し長さを2.0に調整

			// 3D -> スクリーン変換ラムダ式
			auto worldToScreen = [&](dx::XMVECTOR pos) -> ImVec2 {
				dx::XMMATRIX viewProj = view * proj;
				dx::XMVECTOR clip = dx::XMVector3TransformCoord(pos, viewProj);

				dx::XMFLOAT3 ndc;
				dx::XMStoreFloat3(&ndc, clip);

				ImVec2 screen;
				screen.x = (ndc.x * 0.5f + 0.5f) * ImGui::GetIO().DisplaySize.x;
				screen.y = (-ndc.y * 0.5f + 0.5f) * ImGui::GetIO().DisplaySize.y;
				return screen;
				};

			// カメラの背後にオブジェクトがある場合のクリッピングチェック（簡易版）
			dx::XMVECTOR camForward = dx::XMVector3Normalize(dx::XMVectorSubtract(active_camera.target, active_camera.position));
			dx::XMVECTOR toStart = dx::XMVectorSubtract(start, active_camera.position);

			// カメラより前方にある場合のみ描画
			if (dx::XMVectorGetX(dx::XMVector3Dot(toStart, camForward)) > 0.0f)
			{
				ImVec2 p1 = worldToScreen(start);
				ImVec2 p2 = worldToScreen(end);

				// 矢印の線を描画
				drawList->AddLine(p1, p2, IM_COL32(255, 255, 0, 255), 2.0f);

				// 矢印の先端（三角）
				ImVec2 dir = ImVec2(p2.x - p1.x, p2.y - p1.y);
				float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
				if (len > 1e-3f)
				{
					dir.x /= len; dir.y /= len;
					ImVec2 ortho(-dir.y, dir.x);
					float arrowSize = 12.0f;

					ImVec2 tip1(p2.x - dir.x * arrowSize + ortho.x * arrowSize * 0.4f, p2.y - dir.y * arrowSize + ortho.y * arrowSize * 0.4f);
					ImVec2 tip2(p2.x - dir.x * arrowSize - ortho.x * arrowSize * 0.4f, p2.y - dir.y * arrowSize - ortho.y * arrowSize * 0.4f);

					drawList->AddTriangleFilled(p2, tip1, tip2, IM_COL32(255, 255, 0, 255));
				}
			}
		}
	}
};