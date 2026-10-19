// DEPTH OF FIELD - Fast Filter Spreading (True Scatter / Compute Shader)
// Based on: "Cinematic Depth of Field" GDC 2017 - Hillesland & Skelton (AMD)
#include "depth_of_field.h"
#include "Engine/system/render_state.h"
#include "Engine/System/Graphics_Core.h"
#include <algorithm>

// ============================================================
// 内部定数
//   固定小数点スケールは ffs_scatter_cs.hlsl / ffs_resolve_cs.hlsl で定義
// ============================================================
namespace
{
	constexpr int MAX_BLUR_RADIUS_ASSUMED = 64;
	constexpr float FFS_FIXED_SCALE = 33554432.0f; // 2^25
}

// ============================================================
// コンストラクタ
// ============================================================
DepthOfField::DepthOfField(
	ID3D11Device* device,
	uint32_t width,
	uint32_t height)
	: full_width(width)
	, full_height(height)
	, work_width(half_resolution ? width >> 1 : width)
	, work_height(half_resolution ? height >> 1 : height)
{
	HRESULT hr = S_OK;

	bit_block_transfer = std::make_unique<FullscreenQuad>(device);

	// ---- CoC マップ（フル解像度） ----
	coc_map = std::make_unique<Framebuffer>(
		device, full_width, full_height,
		DXGI_FORMAT_R16G16_FLOAT, 1, false);

	// ---- FFS の完全な出力画像 ----
	// CS の RWTexture2D（UAV）として書き込み、PS の SRV として読む。
	{
		D3D11_TEXTURE2D_DESC td{};
		td.Width = work_width;
		td.Height = work_height;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

		D3D11_SHADER_RESOURCE_VIEW_DESC  srv_desc{};
		srv_desc.Format = td.Format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;

		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
		uav_desc.Format = td.Format;
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uav_desc.Texture2D.MipSlice = 0;

		hr = device->CreateTexture2D(&td, nullptr, blurred.texture.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"DepthOfField: Failed to create blurred texture");

		hr = device->CreateShaderResourceView(
			blurred.texture.Get(), &srv_desc, blurred.srv.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"DepthOfField: Failed to create blurred SRV");

		hr = device->CreateUnorderedAccessView(
			blurred.texture.Get(), &uav_desc, blurred.uav.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"DepthOfField: Failed to create blurred UAV");
	}

	// ---- 最終出力（フル解像度） ----
	dof_final = std::make_unique<Framebuffer>(
		device, full_width, full_height,
		DXGI_FORMAT_R16G16B16A16_FLOAT, 1, false);

	// ---- FFS デルタバッファ ----`
	// パディング: 各辺に (MaxBlurRadius + 1) ピクセル追加
	// MaxBlurRadius の最大値を定数で決め打ち（例: 64px）
	//   → 実行時に MaxBlurRadius が変化した場合は再生成が必要
	ffs_padding_x = MAX_BLUR_RADIUS_ASSUMED + 1;
	ffs_padding_y = MAX_BLUR_RADIUS_ASSUMED + 1;
	ffs_buf_width = static_cast<int>(work_width) + ffs_padding_x * 2;
	ffs_buf_height = static_cast<int>(work_height) + ffs_padding_y * 2;

	{
		// RGBA × ピクセル数 の int バッファ
		uint32_t element_count = static_cast<uint32_t>(
			ffs_buf_width * ffs_buf_height * 4);
		D3D11_BUFFER_DESC bd{};
		bd.ByteWidth = element_count * sizeof(int);
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bd.MiscFlags = 0;

		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
		uav_desc.Format = DXGI_FORMAT_R32_SINT;
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uav_desc.Buffer.FirstElement = 0;
		uav_desc.Buffer.NumElements = element_count;
		uav_desc.Buffer.Flags = 0;

		delta_buf.element_count = element_count;

		hr = device->CreateBuffer(&bd, nullptr, delta_buf.buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"DepthOfField: Failed to create delta buffer");

		hr = device->CreateUnorderedAccessView(
			delta_buf.buffer.Get(), &uav_desc, delta_buf.uav.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"DepthOfField: Failed to create delta UAV");
	}

	// ---- シェーダ読み込み ----
	// ピクセルシェーダ
	create_ps_from_cso(device, "coc_generation_ps.cso", coc_generation_ps.GetAddressOf());
	create_ps_from_cso(device, "composite_ps.cso", composite_ps.GetAddressOf());

	// コンピュートシェーダ（既存 PS を CS に差し替え）
	create_cs_from_cso(device, "ffs_scatter_cs.cso", ffs_scatter_cs.GetAddressOf());
	create_cs_from_cso(device, "ffs_prefix_h_cs.cso", ffs_prefix_h_cs.GetAddressOf());
	create_cs_from_cso(device, "ffs_prefix_v_cs.cso", ffs_prefix_v_cs.GetAddressOf());
	create_cs_from_cso(device, "ffs_resolve_cs.cso", ffs_resolve_cs.GetAddressOf());

	// ---- 定数バッファ作成 ----
	// b9: dof_local_constants
	{
		D3D11_BUFFER_DESC cbd{};
		cbd.ByteWidth = sizeof(dof_local_constants);
		cbd.Usage = D3D11_USAGE_DEFAULT;
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		hr = device->CreateBuffer(&cbd, nullptr, constant_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"DepthOfField: Failed to create CB b9");
	}

	// b10: ffs_constants
	{
		D3D11_BUFFER_DESC cbd{};
		cbd.ByteWidth = sizeof(ffs_constants);
		cbd.Usage = D3D11_USAGE_DEFAULT;
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		hr = device->CreateBuffer(&cbd, nullptr, ffs_constant_buffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"DepthOfField: Failed to create CB b10");
	}
}

// ============================================================
// make() — 毎フレームの DoF 処理実行
// ============================================================
void DepthOfField::make(
	ID3D11DeviceContext* immediate_context,
	ID3D11ShaderResourceView* color_map)
{
	// ---- ステート保存 ----
	ID3D11ShaderResourceView* const null_srvs[4] = {};
	ID3D11UnorderedAccessView* const null_uavs[2] = {};

	ID3D11ShaderResourceView* cached_srvs[4]{};
	immediate_context->PSGetShaderResources(0, 4, cached_srvs);

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> cached_dss;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState>   cached_rs;
	Microsoft::WRL::ComPtr<ID3D11BlendState>        cached_bs;
	FLOAT blend_factor[4]; UINT sample_mask;
	immediate_context->OMGetDepthStencilState(cached_dss.GetAddressOf(), 0);
	immediate_context->RSGetState(cached_rs.GetAddressOf());
	immediate_context->OMGetBlendState(cached_bs.GetAddressOf(), blend_factor, &sample_mask);

	Microsoft::WRL::ComPtr<ID3D11Buffer> cached_cb9, cached_cb10, cached_cs_cb9, cached_cs_cb10;
	Microsoft::WRL::ComPtr<ID3D11Buffer> cached_ps_cb1, cached_cs_cb1;
	immediate_context->PSGetConstantBuffers(1, 1, cached_ps_cb1.GetAddressOf());
	immediate_context->CSGetConstantBuffers(1, 1, cached_cs_cb1.GetAddressOf());
	immediate_context->PSGetConstantBuffers(9, 1, cached_cb9.GetAddressOf());
	immediate_context->PSGetConstantBuffers(10, 1, cached_cb10.GetAddressOf());
	immediate_context->CSGetConstantBuffers(9, 1, cached_cs_cb9.GetAddressOf());
	immediate_context->CSGetConstantBuffers(10, 1, cached_cs_cb10.GetAddressOf());

	// ---- 2D レンダーステート適用 ----
	Render_State::instance().set_2d_render_states(immediate_context);

	// ---- b9 更新 ----
	dof_local_constants local_data{};
	local_data.is_dof = is_dof;
	local_data.inv_buffer_width = 1.0f / static_cast<float>(work_width);
	local_data.inv_buffer_height = 1.0f / static_cast<float>(work_height);
	local_data.debug_view = (std::clamp)(debug_view, 0, 3);
	local_data.inv_full_width = 1.0f / static_cast<float>(full_width);
	local_data.inv_full_height = 1.0f / static_cast<float>(full_height);
	local_data.work_scale = half_resolution ? 0.5f : 1.0f;
	immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &local_data, 0, 0);
	immediate_context->PSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());

	// CoC 生成 (PS) と Scatter (CS) はどちらも b1 のカメラ定数を参照する。
	// 他のポストエフェクトがステージ別定数バッファを変更していても、
	// DOF が現在のカメラ設定を使うよう明示的にバインドする。
	ID3D11Buffer* camera_constant_buffer = Graphics_Core::instance().get_constant_buffer(
		Graphics_Core::Conastant_Buffer_Type::Camera);
	immediate_context->PSSetConstantBuffers(1, 1, &camera_constant_buffer);
	immediate_context->CSSetConstantBuffers(1, 1, &camera_constant_buffer);

	// ---- b10 更新（FFS 定数） ----
	ffs_constants ffs_data{};
	ffs_data.buf_width = ffs_buf_width;
	ffs_data.buf_height = ffs_buf_height;
	ffs_data.padding_x = ffs_padding_x;
	ffs_data.padding_y = ffs_padding_y;
	ffs_data.work_width_cb = static_cast<int>(work_width);
	ffs_data.work_height_cb = static_cast<int>(work_height);
	const float max_blur_radius = CameraManager::instance().get_active_camera()->get_camera().MaxBlurRadius;
	const float effective_max_blur = (std::min)(
		max_blur_radius * std::clamp(blur_strength, 0.0f, 4.0f),
		static_cast<float>(MAX_BLUR_RADIUS_ASSUMED));
	ffs_data.max_radius_work = effective_max_blur * local_data.work_scale;
	ffs_data.fixed_scale = FFS_FIXED_SCALE;
	immediate_context->UpdateSubresource(ffs_constant_buffer.Get(), 0, 0, &ffs_data, 0, 0);
	// CS / PS 両方にバインド
	immediate_context->PSSetConstantBuffers(10, 1, ffs_constant_buffer.GetAddressOf());
	immediate_context->CSSetConstantBuffers(9, 1, constant_buffer.GetAddressOf());
	immediate_context->CSSetConstantBuffers(10, 1, ffs_constant_buffer.GetAddressOf());

	// ============================================================
	// Pass 1: CoC 生成
	//   入力: depth_map (GBuffer, t1)
	//   出力: coc_map (R=NearCoC, G=FarCoC)
	// ============================================================
	coc_map->Clear(immediate_context, 0, 0, 0, 0);
	coc_map->Activate(immediate_context);
	{
		ID3D11ShaderResourceView* srvs[2] = {
			color_map,
			Graphics_Core::instance().get_geometry_buffer()->GetDepth()
		};
		bit_block_transfer->Blit(immediate_context, srvs, 0, 2, coc_generation_ps.Get());
	}
	// Release G-buffer depth before restoring it as the previous depth target.
	immediate_context->PSSetShaderResources(0, 2, null_srvs);
	coc_map->Deactivate(immediate_context);

	// ============================================================
	// Pass 2: Scatter the complete color image using signed near/far CoC.
	// In-focus pixels are scattered with radius zero, so the resolve still
	// reconstructs a complete image and needs no original/blur lerp.
	// ============================================================
	{
		UINT clear_val[4] = { 0, 0, 0, 0 };
		immediate_context->ClearUnorderedAccessViewUint(delta_buf.uav.Get(), clear_val);

		ID3D11ShaderResourceView* scatter_srvs[2] = {
			color_map, coc_map->GetColorMap()
		};
		immediate_context->CSSetShader(ffs_scatter_cs.Get(), nullptr, 0);
		immediate_context->CSSetShaderResources(0, 2, scatter_srvs);
		immediate_context->CSSetUnorderedAccessViews(0, 1, delta_buf.uav.GetAddressOf(), nullptr);
		immediate_context->Dispatch((work_width + 7) / 8, (work_height + 7) / 8, 1);
		immediate_context->CSSetUnorderedAccessViews(0, 1, null_uavs, nullptr);
		immediate_context->CSSetShaderResources(0, 2, null_srvs);
	}

	// Two integrations per axis invert the 2D second-difference Bartlett stencil.
	immediate_context->CSSetShader(ffs_prefix_h_cs.Get(), nullptr, 0);
	immediate_context->CSSetUnorderedAccessViews(0, 1, delta_buf.uav.GetAddressOf(), nullptr);
	immediate_context->Dispatch(1, static_cast<UINT>(ffs_buf_height), 1);
	immediate_context->CSSetUnorderedAccessViews(0, 1, null_uavs, nullptr);

	immediate_context->CSSetShader(ffs_prefix_v_cs.Get(), nullptr, 0);
	immediate_context->CSSetUnorderedAccessViews(0, 1, delta_buf.uav.GetAddressOf(), nullptr);
	immediate_context->Dispatch(static_cast<UINT>(ffs_buf_width), 1, 1);
	immediate_context->CSSetUnorderedAccessViews(0, 1, null_uavs, nullptr);

	// Resolve fixed-point color by the accumulated alpha weight.
	immediate_context->CSSetShader(ffs_resolve_cs.Get(), nullptr, 0);
	immediate_context->CSSetUnorderedAccessViews(0, 1, delta_buf.uav.GetAddressOf(), nullptr);
	immediate_context->CSSetUnorderedAccessViews(1, 1, blurred.uav.GetAddressOf(), nullptr);
	immediate_context->Dispatch((work_width + 7) / 8, (work_height + 7) / 8, 1);
	ID3D11UnorderedAccessView* null2[2] = { nullptr, nullptr };
	immediate_context->CSSetUnorderedAccessViews(0, 2, null2, nullptr);

	// CS をアンセット
	immediate_context->CSSetShader(nullptr, nullptr, 0);

	// ============================================================
	// Pass 3: Copy the complete resolved FFS image to the full-resolution output.
	// ============================================================
	dof_final->Clear(immediate_context, 0, 0, 0, 1);
	dof_final->Activate(immediate_context);
	{
		ID3D11ShaderResourceView* srvs[3] = {
			color_map,
			coc_map->GetColorMap(),
			blurred.srv.Get()
		};
		bit_block_transfer->Blit(immediate_context, srvs, 0, 3, composite_ps.Get());
	}
	dof_final->Deactivate(immediate_context);
	immediate_context->PSSetShaderResources(0, 4, null_srvs);

	// ---- ステート復元 ----
	immediate_context->PSSetConstantBuffers(1, 1, cached_ps_cb1.GetAddressOf());
	immediate_context->CSSetConstantBuffers(1, 1, cached_cs_cb1.GetAddressOf());
	immediate_context->PSSetConstantBuffers(9, 1, cached_cb9.GetAddressOf());
	immediate_context->PSSetConstantBuffers(10, 1, cached_cb10.GetAddressOf());
	immediate_context->CSSetConstantBuffers(9, 1, cached_cs_cb9.GetAddressOf());
	immediate_context->CSSetConstantBuffers(10, 1, cached_cs_cb10.GetAddressOf());
	immediate_context->OMSetDepthStencilState(cached_dss.Get(), 0);
	immediate_context->RSSetState(cached_rs.Get());
	immediate_context->OMSetBlendState(cached_bs.Get(), blend_factor, sample_mask);
	immediate_context->PSSetShaderResources(0, 4, cached_srvs);

	for (auto* srv : cached_srvs)
		if (srv) srv->Release();
}
