// DEPTH OF FIELD - Fast Filter Spreading (Scatter Method)
// Based on: "Cinematic Depth of Field" GDC 2017 - Hillesland & Skelton (AMD)
#include "depth_of_field.h"
#include "Engine/system/render_state.h"
#include "Engine/System/Graphics_Core.h"

DepthOfField::DepthOfField(ID3D11Device* device, uint32_t width, uint32_t height)
	: full_width(width)
	, full_height(height)
	, work_width(half_resolution ? width >> 1 : width)
	, work_height(half_resolution ? height >> 1 : height)
{
	bit_block_transfer = std::make_unique<FullscreenQuad>(device);

	// CoC マップ（フル解像度）: R=NearCoC, G=FarCoC
	coc_map = std::make_unique<Framebuffer>(
		device, full_width, full_height,
		DXGI_FORMAT_R16G16_FLOAT, false
	);

	// Near / Far 分離バッファ（ワーク解像度）
	near_field = std::make_unique<Framebuffer>(
		device, work_width, work_height,
		DXGI_FORMAT_R16G16B16A16_FLOAT, false
	);
	far_field = std::make_unique<Framebuffer>(
		device, work_width, work_height,
		DXGI_FORMAT_R16G16B16A16_FLOAT, false
	);

	// FFS 中間バッファ（水平 / 垂直）: [0]=near, [1]=far
	for (int i = 0; i < 2; ++i)
	{
		ffs_horizontal[i] = std::make_unique<Framebuffer>(
			device, work_width, work_height,
			DXGI_FORMAT_R16G16B16A16_FLOAT, false
		);
		ffs_vertical[i] = std::make_unique<Framebuffer>(
			device, work_width, work_height,
			DXGI_FORMAT_R16G16B16A16_FLOAT, false
		);
	}

	// 最終出力（フル解像度）
	dof_final = std::make_unique<Framebuffer>(
		device, full_width, full_height,
		DXGI_FORMAT_R16G16B16A16_FLOAT, 0, false
	);

	// シェーダ読み込み（用途名_シェーダータイプ.cso）
	create_ps_from_cso(device, "coc_generation_ps.cso", coc_generation_ps.GetAddressOf());
	create_ps_from_cso(device, "field_separation_near_ps.cso", field_separation_near_ps.GetAddressOf());
	create_ps_from_cso(device, "field_separation_far_ps.cso", field_separation_far_ps.GetAddressOf());
	create_ps_from_cso(device, "ffs_horizontal_ps.cso", ffs_horizontal_ps.GetAddressOf());
	create_ps_from_cso(device, "ffs_vertical_ps.cso", ffs_vertical_ps.GetAddressOf());
	create_ps_from_cso(device, "composite_ps.cso", composite_ps.GetAddressOf());

	// 軽量定数バッファ（b9）: is_dof フラグ + inv_buffer サイズのみ
	// FNumber / FocalLength 等は CAMERA_CONSTANT_BUFFER (b1) から参照する
	HRESULT hr{ S_OK };
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(dof_local_constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), L"DepthOfField: Failed to create constant buffer");
}

void DepthOfField::make(
	ID3D11DeviceContext* immediate_context,
	ID3D11ShaderResourceView* color_map)
{
	// ---- ステート保存 ----
	//ID3D11ShaderResourceView* null_srv{};
	ID3D11ShaderResourceView* const null_srvs[4] = { nullptr, nullptr, nullptr, nullptr };
	ID3D11ShaderResourceView* cached_srvs[4]{};
	immediate_context->PSGetShaderResources(0, 4, cached_srvs);

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> cached_dss;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState>   cached_rs;
	Microsoft::WRL::ComPtr<ID3D11BlendState>        cached_bs;
	FLOAT blend_factor[4];
	UINT  sample_mask;
	immediate_context->OMGetDepthStencilState(cached_dss.GetAddressOf(), 0);
	immediate_context->RSGetState(cached_rs.GetAddressOf());
	immediate_context->OMGetBlendState(cached_bs.GetAddressOf(), blend_factor, &sample_mask);

	Microsoft::WRL::ComPtr<ID3D11Buffer> cached_cb;
	immediate_context->PSGetConstantBuffers(9, 1, cached_cb.GetAddressOf());

	// ---- 2D レンダーステート適用 ----
	Render_State::instance().set_2d_render_states(immediate_context);

	// ---- 軽量 CB 更新（b9）----
	// DoF パラメータ本体（FNumber 等）は b1 から HLSL が直接参照する
	dof_local_constants local_data{};
	local_data.is_dof = is_dof;
	local_data.inv_buffer_width = 1.0f / static_cast<float>(work_width);
	local_data.inv_buffer_height = 1.0f / static_cast<float>(work_height);
	immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &local_data, 0, 0);
	immediate_context->PSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());

	// ============================================================
	// Pass 1: CoC 生成
	//   入力: depth_map(t1)
	//   出力: coc_map (R=NearCoC, G=FarCoC [0,1])
	//
	//   HLSL の ComputeCoC() が CAMERA_CONSTANT_BUFFER (b1) の
	//   FNumber / FocalLength / SensorSize / FocusDist / MaxBlurRadius
	//   および inv_projection を使って薄レンズ公式で CoC を算出する。
	// ============================================================
	coc_map->Clear(immediate_context, 0, 0, 0, 0);
	coc_map->Activate(immediate_context);
	{
		ID3D11ShaderResourceView* srvs[2] = { color_map, Graphics_Core::instance().get_geometry_buffer()->GetDepth() };
		bit_block_transfer->Blit(immediate_context, srvs, 0, 2, coc_generation_ps.Get());
	}
	coc_map->Deactivate(immediate_context);
	immediate_context->PSSetShaderResources(0, 2, null_srvs);

	// ============================================================
	// Pass 2: Near / Far フィールド分離
	//   入力: color_map(t0), coc_map(t1)
	//   出力:
	//     near_field: RGB = color * near_coc, A = near_coc (premult)
	//     far_field : RGB = color,            A = far_coc
	//
	//   本来は MRT で 1 パス完結だが、Framebuffer が MRT 非対応の場合は
	//   near / far を別シェーダ（_near_ps / _far_ps）で 2 パスに分割する。
	// ============================================================
	{
		ID3D11ShaderResourceView* srvs[2] = { color_map, coc_map->GetColorMap() };

		near_field->Clear(immediate_context, 0, 0, 0, 0);
		near_field->Activate(immediate_context);
		bit_block_transfer->Blit(immediate_context, srvs, 0, 2, field_separation_near_ps.Get());
		near_field->Deactivate(immediate_context);
		immediate_context->PSSetShaderResources(0, 2, null_srvs);

		far_field->Clear(immediate_context, 0, 0, 0, 0);
		far_field->Activate(immediate_context);
		bit_block_transfer->Blit(immediate_context, srvs, 0, 2, field_separation_far_ps.Get());
		far_field->Deactivate(immediate_context);
		immediate_context->PSSetShaderResources(0, 2, null_srvs);
	}

	// ============================================================
	// Pass 3 & 4: Fast Filter Spreading（FFS）
	//   near / far それぞれに 水平 → 垂直 の 2 パスを適用。
	//   Gather 近似実装: 各出力ピクセルが周囲を収集する方式。
	//   真の FFS (Compute Shader + InterlockedAdd) に切り替える場合は
	//   ffs_horizontal_ps / ffs_vertical_ps を CS 版に差し替えること。
	// ============================================================
	for (int i = 0; i < 2; ++i)
	{
		Framebuffer* src = (i == 0) ? near_field.get() : far_field.get();

		// 水平パス
		ffs_horizontal[i]->Clear(immediate_context, 0, 0, 0, 0);
		ffs_horizontal[i]->Activate(immediate_context);
		bit_block_transfer->Blit(immediate_context,
			src->GetColorMapAddress(), 0, 1, ffs_horizontal_ps.Get());
		ffs_horizontal[i]->Deactivate(immediate_context);
		immediate_context->PSSetShaderResources(0, 1, null_srvs);

		// 垂直パス（ボケ完成）
		ffs_vertical[i]->Clear(immediate_context, 0, 0, 0, 0);
		ffs_vertical[i]->Activate(immediate_context);
		bit_block_transfer->Blit(immediate_context,
			ffs_horizontal[i]->GetColorMapAddress(), 0, 1, ffs_vertical_ps.Get());
		ffs_vertical[i]->Deactivate(immediate_context);
		immediate_context->PSSetShaderResources(0, 1, null_srvs);
	}

	// ============================================================
	// Pass 5: 最終合成
	//   入力:
	//     t0: color_map      オリジナル
	//     t1: coc_map        blend 比率の参照に使用
	//     t2: near_blurred   Near ボケ済み (premult α)
	//     t3: far_blurred    Far ボケ済み  (A = 累積 CoC)
	// ============================================================
	dof_final->Clear(immediate_context, 0, 0, 0, 1);
	dof_final->Activate(immediate_context);
	{
		ID3D11ShaderResourceView* srvs[4] = {
			color_map,
			coc_map->GetColorMap(),
			ffs_vertical[0]->GetColorMap(),  // near blurred
			ffs_vertical[1]->GetColorMap()   // far blurred
		};
		bit_block_transfer->Blit(immediate_context, srvs, 0, 4, composite_ps.Get());
	}
	dof_final->Deactivate(immediate_context);

	// ---- ステート復元 ----
	immediate_context->PSSetConstantBuffers(9, 1, cached_cb.GetAddressOf());
	immediate_context->OMSetDepthStencilState(cached_dss.Get(), 0);
	immediate_context->RSSetState(cached_rs.Get());
	immediate_context->OMSetBlendState(cached_bs.Get(), blend_factor, sample_mask);
	immediate_context->PSSetShaderResources(0, 4, cached_srvs);
	for (auto* srv : cached_srvs)
	{
		if (srv) srv->Release();
	}
}
