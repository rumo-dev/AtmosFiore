#include "render_state.h"

#include "graphics_core.h"
Render_State::Render_State()
{
	if (!create_sampler_state(Graphics_Core::instance().get_device())) {
		log_printf("レンダーステートの生成 >> 失敗\n", LogLevel::Error);
	}
	else {
		log_printf("レンダーステートの生成 >> 成功\n", LogLevel::Success);
	}

	if (!create_depth_stencil_state(Graphics_Core::instance().get_device())) {
		log_printf("デプスステートの生成 >> 失敗\n", LogLevel::Error);
	}
	else {
		log_printf("デプスステートの生成 >> 成功\n", LogLevel::Success);
	}
	if (!create_blend_state(Graphics_Core::instance().get_device())) {
		log_printf("ブレンドステートの生成 >> 失敗\n", LogLevel::Error);
	}
	else {
		log_printf("ブレンドステートの生成 >> 成功\n", LogLevel::Success);
	}
	if (!create_rasterizer_state(Graphics_Core::instance().get_device())) {
		log_printf("ラスタライザーステートの生成 >> 失敗\n", LogLevel::Error);
	}
	else {
		log_printf("ラスタライザーステートの生成 >> 成功\n", LogLevel::Success);
	}
}


bool Render_State::create_sampler_state(ID3D11Device* device)
{
	HRESULT hr{ S_OK };
	D3D11_SAMPLER_DESC sampler_desc{};
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.MipLODBias = 0;
	sampler_desc.MaxAnisotropy = 16;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	sampler_desc.BorderColor[0] = 0;
	sampler_desc.BorderColor[1] = 0;
	sampler_desc.BorderColor[2] = 0;
	sampler_desc.BorderColor[3] = 0;
	sampler_desc.MinLOD = 0;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = device->CreateSamplerState(&sampler_desc, _sampler_states[POINT_WRAP].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	hr = device->CreateSamplerState(&sampler_desc, _sampler_states[LINEAR_WRAP].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	hr = device->CreateSamplerState(&sampler_desc, _sampler_states[ANISOTROPIC_WRAP].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	hr = device->CreateSamplerState(&sampler_desc, _sampler_states[POINT_CLAMP].GetAddressOf());

	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	hr = device->CreateSamplerState(&sampler_desc, _sampler_states[LINEAR_CLAMP].GetAddressOf());

	sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	hr = device->CreateSamplerState(&sampler_desc, _sampler_states[ANISOTROPIC_CLAMP].GetAddressOf());

	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler_desc.BorderColor[0] = 0;
	sampler_desc.BorderColor[1] = 0;
	sampler_desc.BorderColor[2] = 0;
	sampler_desc.BorderColor[3] = 1;
	hr = device->CreateSamplerState(&sampler_desc, _sampler_states[POINT_BORDER_OPAQUE_BLACK].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler_desc.BorderColor[0] = 0;
	sampler_desc.BorderColor[1] = 0;
	sampler_desc.BorderColor[2] = 0;
	sampler_desc.BorderColor[3] = 1;
	hr = device->CreateSamplerState(&sampler_desc, _sampler_states[LINEAR_BORDER_OPAQUE_BLACK].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler_desc.BorderColor[0] = 1;
	sampler_desc.BorderColor[1] = 1;
	sampler_desc.BorderColor[2] = 1;
	sampler_desc.BorderColor[3] = 1;
	hr = device->CreateSamplerState(&sampler_desc, _sampler_states[POINT_BORDER_OPAQUE_WHITE].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler_desc.BorderColor[0] = 1;
	sampler_desc.BorderColor[1] = 1;
	sampler_desc.BorderColor[2] = 1;
	sampler_desc.BorderColor[3] = 1;
	hr = device->CreateSamplerState(&sampler_desc, _sampler_states[LINEAR_BORDER_OPAQUE_WHITE].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
	sampler_desc.BorderColor[0] = 0;
	sampler_desc.BorderColor[1] = 0;
	sampler_desc.BorderColor[2] = 0;
	sampler_desc.BorderColor[3] = 0;
	hr = device->CreateSamplerState(&sampler_desc, _sampler_states[POINT_MIRROR].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
	hr = device->CreateSamplerState(&sampler_desc, _sampler_states[LINEAR_MIRROR].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
	hr = device->CreateSamplerState(&sampler_desc, _sampler_states[ANISOTROPIC_MIRROR].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	sampler_desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; //D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL; //D3D11_COMPARISON_LESS_EQUAL
	sampler_desc.BorderColor[0] = 1;
	sampler_desc.BorderColor[1] = 1;
	sampler_desc.BorderColor[2] = 1;
	sampler_desc.BorderColor[3] = 1;
	hr = device->CreateSamplerState(&sampler_desc, _sampler_states[COMPARISON_DEPTH].GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	return true;
}
void Render_State::set_sampler_state(ID3D11DeviceContext* immediate_context)
{
	immediate_context->PSSetSamplers(POINT_WRAP, 1, _sampler_states[POINT_WRAP].GetAddressOf());
	immediate_context->PSSetSamplers(LINEAR_WRAP, 1, _sampler_states[LINEAR_WRAP].GetAddressOf());
	immediate_context->PSSetSamplers(ANISOTROPIC_WRAP, 1, _sampler_states[ANISOTROPIC_WRAP].GetAddressOf());

	immediate_context->PSSetSamplers(POINT_CLAMP, 1, _sampler_states[POINT_CLAMP].GetAddressOf());
	immediate_context->PSSetSamplers(LINEAR_CLAMP, 1, _sampler_states[LINEAR_CLAMP].GetAddressOf());
	immediate_context->PSSetSamplers(ANISOTROPIC_CLAMP, 1, _sampler_states[ANISOTROPIC_CLAMP].GetAddressOf());

	immediate_context->PSSetSamplers(POINT_BORDER_OPAQUE_BLACK, 1, _sampler_states[POINT_BORDER_OPAQUE_BLACK].GetAddressOf());
	immediate_context->PSSetSamplers(LINEAR_BORDER_OPAQUE_BLACK, 1, _sampler_states[LINEAR_BORDER_OPAQUE_BLACK].GetAddressOf());

	immediate_context->PSSetSamplers(POINT_BORDER_OPAQUE_WHITE, 1, _sampler_states[POINT_BORDER_OPAQUE_WHITE].GetAddressOf());
	immediate_context->PSSetSamplers(LINEAR_BORDER_OPAQUE_WHITE, 1, _sampler_states[LINEAR_BORDER_OPAQUE_WHITE].GetAddressOf());

	immediate_context->PSSetSamplers(POINT_MIRROR, 1, _sampler_states[POINT_MIRROR].GetAddressOf());
	immediate_context->PSSetSamplers(LINEAR_MIRROR, 1, _sampler_states[LINEAR_MIRROR].GetAddressOf());
	immediate_context->PSSetSamplers(ANISOTROPIC_MIRROR, 1, _sampler_states[ANISOTROPIC_MIRROR].GetAddressOf());

	immediate_context->PSSetSamplers(COMPARISON_DEPTH, 1, _sampler_states[COMPARISON_DEPTH].GetAddressOf());

}

bool Render_State::create_depth_stencil_state(ID3D11Device* device)
{
	HRESULT hr{ S_OK };
	D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{};
	depth_stencil_desc.DepthEnable = TRUE;
	depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS;
	depth_stencil_desc.StencilEnable = FALSE;
	hr = device->CreateDepthStencilState(&depth_stencil_desc, &_depth_stencil_states[static_cast<int>(Depth_State::Test_Enable_Write_Enable)]);
	if (FAILED(hr)) {
		log_printf("デプスステンシルステートオブジェクトの生成(Test_Enable_Write_Enable) >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("デプスステンシルステートオブジェクトの生成(Test_Enable_Write_Enable) >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);
	depth_stencil_desc.DepthEnable = TRUE;
	depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS;
	depth_stencil_desc.StencilEnable = FALSE;
	hr = device->CreateDepthStencilState(&depth_stencil_desc, &_depth_stencil_states[static_cast<int>(Depth_State::Test_Enable_Write_Disable)]);
	if (FAILED(hr)) {
		log_printf("デプスステンシルステートオブジェクトの生成(Test_Enable_Write_Disable) >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("デプスステンシルステートオブジェクトの生成(Test_Enable_Write_Disable) >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);
	depth_stencil_desc.DepthEnable = FALSE;
	depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS;
	depth_stencil_desc.StencilEnable = FALSE;
	hr = device->CreateDepthStencilState(&depth_stencil_desc, &_depth_stencil_states[static_cast<int>(Depth_State::Test_Disable_Write_Enable)]);
	if (FAILED(hr)) {
		log_printf("デプスステンシルステートオブジェクトの生成(Test_Disable_Write_Enable) >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("デプスステンシルステートオブジェクトの生成(Test_Disable_Write_Enable) >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);
	depth_stencil_desc.DepthEnable = FALSE;
	depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS;
	depth_stencil_desc.StencilEnable = FALSE;
	hr = device->CreateDepthStencilState(&depth_stencil_desc, &_depth_stencil_states[static_cast<int>(Depth_State::Test_Disable_Write_Disable)]);

	if (FAILED(hr)) {
		log_printf("デプスステンシルステートオブジェクトの生成(Test_Disable_Write_Disable) >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("デプスステンシルステートオブジェクトの生成(Test_Disable_Write_Disable) >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);
	return true;
}

bool Render_State::create_blend_state(ID3D11Device* device)
{
	D3D11_BLEND_DESC blend_desc{};
	HRESULT hr{ S_OK };
	blend_desc.AlphaToCoverageEnable = false;
	blend_desc.IndependentBlendEnable = false;

	// アルファブレンド（透明度による合成）
	blend_desc.RenderTarget[0].BlendEnable = true;
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = device->CreateBlendState(&blend_desc, &_blend_states[static_cast<int>(Blend_State::Alpha)]);
	if (FAILED(hr)) {
		log_printf("ブレンドステートオブジェクトの生成(Alpha) >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("ブレンドステートオブジェクトの生成(Alpha) >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);


	// 加算合成（色を加算して明るくする）
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	hr = device->CreateBlendState(&blend_desc, &_blend_states[static_cast<int>(Blend_State::Add)]);
	if (FAILED(hr)) {
		log_printf("ブレンドステートオブジェクトの生成(Add) >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("ブレンドステートオブジェクトの生成(Add) >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);



	// 減算合成（色を引いて暗くする）
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;
	hr = device->CreateBlendState(&blend_desc, &_blend_states[static_cast<int>(Blend_State::Subtract)]);
	if (FAILED(hr)) {
		log_printf("ブレンドステートオブジェクトの生成(Subtract) >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("ブレンドステートオブジェクトの生成(Subtract) >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);

	// 置換合成（上書き描画）
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	hr = device->CreateBlendState(&blend_desc, &_blend_states[static_cast<int>(Blend_State::Replace)]);
	if (FAILED(hr)) {
		log_printf("ブレンドステートオブジェクトの生成(Replace) >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("ブレンドステートオブジェクトの生成(Replace) >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);


	// 乗算合成（アルファモード：透明度で色を乗算）
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_COLOR;
	hr = device->CreateBlendState(&blend_desc, &_blend_states[static_cast<int>(Blend_State::Multiply_Alpha)]);
	if (FAILED(hr)) {
		log_printf("ブレンドステートオブジェクトの生成(Multiply_Alpha) >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("ブレンドステートオブジェクトの生成(Multiply_Alpha) >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);

	// 乗算合成（カラーモード：描画先の色で乗算）
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_DEST_COLOR;
	hr = device->CreateBlendState(&blend_desc, &_blend_states[static_cast<int>(Blend_State::Multiply_Color)]);
	if (FAILED(hr)) {
		log_printf("ブレンドステートオブジェクトの生成(Multiply_Color) >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}

	log_printf("ブレンドステートオブジェクトの生成(Multiply_Color) >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);


	// 明るい方を選択（MAX合成）
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_COLOR;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;
	hr = device->CreateBlendState(&blend_desc, &_blend_states[static_cast<int>(Blend_State::Lighten)]);
	if (FAILED(hr)) {
		log_printf("ブレンドステートオブジェクトの生成(Lighten) >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("ブレンドステートオブジェクトの生成(Lighten) >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);

	// 暗い方を選択（MIN合成）
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MIN;
	hr = device->CreateBlendState(&blend_desc, &_blend_states[static_cast<int>(Blend_State::Darken)]);
	if (FAILED(hr)) {
		log_printf("ブレンドステートオブジェクトの生成(Darken) >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("ブレンドステートオブジェクトの生成(Darken) >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);


	// スクリーン合成（明るくする合成方法）
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_INV_DEST_COLOR;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	hr = device->CreateBlendState(&blend_desc, &_blend_states[static_cast<int>(Blend_State::Screen)]);
	if (FAILED(hr)) {
		log_printf("ブレンドステートオブジェクトの生成(Screen) >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("ブレンドステートオブジェクトの生成(Screen) >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);

	// 合成なし（通常描画）
	blend_desc.RenderTarget[0].BlendEnable = false;
	hr = device->CreateBlendState(&blend_desc, &_blend_states[static_cast<int>(Blend_State::Opaque)]);
	if (FAILED(hr)) {
		log_printf("ブレンドステートオブジェクトの生成(Opaque) >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("ブレンドステートオブジェクトの生成(Opaque) >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);




	return true;
}

bool Render_State::create_rasterizer_state(ID3D11Device* device)
{//ラスタライザーステートの生成
	//カリングなし
	D3D11_RASTERIZER_DESC rasterizer_desc{};
	rasterizer_desc.FillMode = D3D11_FILL_SOLID;
	rasterizer_desc.CullMode = D3D11_CULL_NONE;
	rasterizer_desc.FrontCounterClockwise = FALSE;
	rasterizer_desc.DepthBias = 0;
	rasterizer_desc.DepthBiasClamp = 0;
	rasterizer_desc.SlopeScaledDepthBias = 0;
	rasterizer_desc.DepthClipEnable = TRUE;
	rasterizer_desc.ScissorEnable = FALSE;
	rasterizer_desc.MultisampleEnable = FALSE;
	rasterizer_desc.AntialiasedLineEnable = FALSE;
	device->CreateRasterizerState(&rasterizer_desc, &_rasterizer_states[static_cast<int>(Rasterizer_State::Cull_Back_CW)]);
	rasterizer_desc.FrontCounterClockwise = TRUE;
	device->CreateRasterizerState(&rasterizer_desc, &_rasterizer_states[static_cast<int>(Rasterizer_State::Cull_Back_CCW)]);
	//裏面カリング
	rasterizer_desc.CullMode = D3D11_CULL_BACK;
	rasterizer_desc.FrontCounterClockwise = FALSE;
	device->CreateRasterizerState(&rasterizer_desc, &_rasterizer_states[static_cast<int>(Rasterizer_State::Cull_None_CW)]);
	rasterizer_desc.FrontCounterClockwise = TRUE;
	device->CreateRasterizerState(&rasterizer_desc, &_rasterizer_states[static_cast<int>(Rasterizer_State::Cull_None_CCW)]);
	//前面カリング
	rasterizer_desc.CullMode = D3D11_CULL_FRONT;
	rasterizer_desc.FrontCounterClockwise = FALSE;
	device->CreateRasterizerState(&rasterizer_desc, &_rasterizer_states[static_cast<int>(Rasterizer_State::Cull_Front_CW)]);
	rasterizer_desc.FrontCounterClockwise = TRUE;
	device->CreateRasterizerState(&rasterizer_desc, &_rasterizer_states[static_cast<int>(Rasterizer_State::Cull_Front_CCW)]);
	//ワイヤーフレーム
	rasterizer_desc.FillMode = D3D11_FILL_WIREFRAME;
	rasterizer_desc.CullMode = D3D11_CULL_BACK;
	rasterizer_desc.FrontCounterClockwise = FALSE;
	rasterizer_desc.AntialiasedLineEnable = TRUE;
	device->CreateRasterizerState(&rasterizer_desc, &_rasterizer_states[static_cast<int>(Rasterizer_State::Wireframe_CW)]);
	rasterizer_desc.FrontCounterClockwise = TRUE;
	device->CreateRasterizerState(&rasterizer_desc, &_rasterizer_states[static_cast<int>(Rasterizer_State::Wireframe_CCW)]);
	rasterizer_desc.DepthClipEnable = FALSE;
	rasterizer_desc.CullMode = D3D11_CULL_NONE;
	device->CreateRasterizerState(&rasterizer_desc, &_rasterizer_states[static_cast<int>(Rasterizer_State::Wireframe_No_Depth_CW)]);
	rasterizer_desc.FrontCounterClockwise = TRUE;
	device->CreateRasterizerState(&rasterizer_desc, &_rasterizer_states[static_cast<int>(Rasterizer_State::Wireframe_No_Depth_CCW)]);




	return true;
}

void Render_State::set_depth_stencil_state(ID3D11DeviceContext* immediate_context, Depth_State state, Stencil_Ref stencil_ref)
{
	immediate_context->OMSetDepthStencilState(_depth_stencil_states[static_cast<int>(state)].Get(), static_cast<UINT>(stencil_ref));
}
void Render_State::set_blend_state(ID3D11DeviceContext* immediate_context, Blend_State state, Color_Utils::Color blend_factor, UINT sample_mask)
{
	FLOAT blend_factor_array[4]{ blend_factor.r,blend_factor.g,blend_factor.b,blend_factor.a };
	immediate_context->OMSetBlendState(_blend_states[static_cast<int>(state)].Get(), blend_factor_array, sample_mask);
}
void Render_State::set_rasterizer_state(ID3D11DeviceContext* immediate_context, Rasterizer_State state)
{
	immediate_context->RSSetState(_rasterizer_states[static_cast<int>(state)].Get());
}

void Render_State::set_render_states(ID3D11DeviceContext* immediate_context,
	Depth_State depth_state,
	Blend_State blend_state,
	Rasterizer_State rasterizer_state,
	Color_Utils::Color blend_factor,
	UINT sample_mask,
	Stencil_Ref stencil_ref)
{
	set_depth_stencil_state(immediate_context, depth_state, stencil_ref);
	set_blend_state(immediate_context, blend_state, blend_factor, sample_mask);
	set_rasterizer_state(immediate_context, rasterizer_state);
	set_sampler_state(immediate_context);
}

void Render_State::set_3d_render_states(ID3D11DeviceContext* immediate_context, Rasterizer_State rs)
{
	set_depth_stencil_state(immediate_context, Depth_State::Test_Enable_Write_Enable, Stencil_Ref::Default);
	set_blend_state(immediate_context, Blend_State::Alpha, Color_Utils::Colors::transparent, 0xFFFFFFFF);
	set_rasterizer_state(immediate_context, rs);
	set_sampler_state(immediate_context);
}
void Render_State::set_2d_render_states(ID3D11DeviceContext* immediate_context)
{
	set_depth_stencil_state(immediate_context, Depth_State::Test_Disable_Write_Disable, Stencil_Ref::Default);
	set_blend_state(immediate_context, Blend_State::Alpha, Color_Utils::Colors::transparent, 0xFFFFFFFF);
	set_rasterizer_state(immediate_context, Rasterizer_State::Cull_None_CW);
	set_sampler_state(immediate_context);
}
void Render_State::set_deferred_render_states(ID3D11DeviceContext* immediate_context, Rasterizer_State rs)
{
	set_depth_stencil_state(immediate_context, Depth_State::Test_Enable_Write_Enable, Stencil_Ref::Default);
	set_blend_state(immediate_context, Blend_State::Opaque, Color_Utils::Colors::transparent, 0xFFFFFFFF);
	set_rasterizer_state(immediate_context, rs);
	set_sampler_state(immediate_context);


}