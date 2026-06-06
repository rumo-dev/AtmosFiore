#include "graphics_core.h"
#include "Engine/utilities/misc.h"
#include "Engine/utilities/color_util.h"

void Graphics_Core::initialize(HWND hwnd) {
	log_printf("グラフィックスコアの初期化開始\n", LogLevel::Info);
	_hwnd = hwnd;
	// 画面のサイズを取得する。
	RECT rc;
	GetClientRect(hwnd, &rc);
	UINT screen_width = rc.right - rc.left;
	UINT screen_height = rc.bottom - rc.top;

	_screen_width = static_cast<float>(screen_width);
	_screen_height = static_cast<float>(screen_height);


	HRESULT hr = S_OK;
#ifdef _DEBUG
	//create_console();

#endif // _DEBUG


	create_device();
	create_swap_chain();
	create_render_target_view();
	set_viewport();
	create_constant_buffer();
	_render_state = std::make_unique<Render_State>();
	post_procss.initialize();
	_g_buffer = std::make_unique<class GeometryBuffer>(_device.Get(), screen_width, screen_height);
	point_light_manager.initialize(_device.Get());
	spot_light_manager.initialize(_device.Get());
	area_light_manager.initialize(_device.Get());


	log_printf("グラフィックスコアの初期化終了\n", LogLevel::Info);
}

void Graphics_Core::clear(Color_Utils::Color color) {
	//log_printf("画面のクリア開始\n", LogLevel::Info);

	_immediate_context.Get()->ClearRenderTargetView(_render_target_view.Get(), &color.r);
	//_immediate_context.Get()->ClearDepthStencilView(_depth_stencil_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	//log_printf("画面のクリア終了\n", LogLevel::Info);
}
void Graphics_Core::set_render_targets() {
	//log_printf("レンダーターゲットの設定開始\n", LogLevel::Info);

	_immediate_context.Get()->RSSetViewports(1, &_viewport);
	_immediate_context.Get()->OMSetRenderTargets(1, _render_target_view.GetAddressOf(), NULL);
	//_immediate_context.Get()->OMSetRenderTargets(1, _render_target_view.GetAddressOf(), _depth_stencil_view.Get());
	//log_printf("レンダーターゲットの設定終了\n", LogLevel::Info);
}
void Graphics_Core::present(UINT sync_interval) {
	//log_printf("画面の表示開始\n", LogLevel::Info);
	//HRESULT hr = _swapchain.Get()->Present(sync_interval, static_cast<UINT>(graphics_constants::PresentFlags::None));
	HRESULT hr = _swapchain.Get()->Present(0, 0);
	if (FAILED(hr)) {
		//log_printf("画面の表示 >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
	}
	else {
		//log_printf("画面の表示 >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);
	}
}

bool Graphics_Core::create_device() {
	HRESULT hr = S_OK;

	log_printf("デバイスの生成開始\n", LogLevel::Info);
	UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	//UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#ifdef _DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL feature_level;
	D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };

	hr = D3D11CreateDevice(
		nullptr,                    // Adapter
		D3D_DRIVER_TYPE_HARDWARE,   // DriverType
		nullptr,                    // Software
		flags,                      // Flags
		feature_levels,             // Feature levels
		1,
		D3D11_SDK_VERSION,
		_device.GetAddressOf(),
		&feature_level,
		_immediate_context.GetAddressOf()
	);
	{
		char buf[256];
		sprintf_s(buf, "Caller: immediate_context=%p \n", _immediate_context);
		OutputDebugStringA(buf);
	}
	if (FAILED(hr)) {
		log_printf("デバイスの生成 >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	else {
		log_printf("デバイスの生成 >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);
		return true;
	}
}

bool Graphics_Core::create_swap_chain() {
	HRESULT hr = S_OK;

	log_printf("スワップチェーンの生成開始\n", LogLevel::Info);
	// DXGIデバイス取得
	IDXGIDevice* dxgi_device = nullptr;
	hr = _device.Get()->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgi_device);

	if (FAILED(hr)) {
		log_printf("DXGIデバイスの取得 >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		log_printf("スワップチェーンの生成 >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}

	// アダプタ取得
	IDXGIAdapter* dxgi_adapter = nullptr;
	hr = dxgi_device->GetAdapter(&dxgi_adapter);
	dxgi_device->Release();

	if (FAILED(hr)) {
		log_printf("アダプタの取得 >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		log_printf("スワップチェーンの生成 >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}

	// ファクトリ取得
	IDXGIFactory* dxgi_factory = nullptr;
	hr = dxgi_adapter->GetParent(__uuidof(IDXGIFactory), (void**)&dxgi_factory);
	dxgi_adapter->Release();
	if (FAILED(hr)) {
		log_printf("ファクトリの取得 >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		log_printf("スワップチェーンの生成 >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}

	// スワップチェーン設定
// スワップチェーン設定
	DXGI_SWAP_CHAIN_DESC desc = {};
	desc.BufferCount = graphics_constants::BackBufferCount;
	desc.BufferDesc.Width = SCREEN_WIDTH;
	desc.BufferDesc.Height = SCREEN_HEIGHT;
	desc.BufferDesc.Format = static_cast<DXGI_FORMAT>(graphics_constants::BackBufferFormat::DEFAULT);
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.OutputWindow = _hwnd;
	desc.SampleDesc.Count = graphics_constants::SampleCount;
	desc.SampleDesc.Quality = graphics_constants::SampleQuality;
	desc.Windowed = !FULLSCREEN;
	desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	desc.Flags = 0;

	// --------------------------------------------------------
	// モニタ（出力デバイス）からリフレッシュレートを自動取得
	// --------------------------------------------------------
	ComPtr<IDXGIFactory> factory;
	hr = CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(factory.GetAddressOf()));
	if (SUCCEEDED(hr))
	{
		ComPtr<IDXGIAdapter> adapter;
		if (SUCCEEDED(factory->EnumAdapters(0, &adapter)))
		{
			ComPtr<IDXGIOutput> output;
			if (SUCCEEDED(adapter->EnumOutputs(0, &output)))
			{
				DXGI_MODE_DESC modeDesc = {};
				modeDesc.Width = SCREEN_WIDTH;
				modeDesc.Height = SCREEN_HEIGHT;
				modeDesc.Format = desc.BufferDesc.Format;

				DXGI_MODE_DESC closestMatch = {};
				if (SUCCEEDED(output->FindClosestMatchingMode(&modeDesc, &closestMatch, nullptr)))
				{
					desc.BufferDesc.RefreshRate = closestMatch.RefreshRate;
					// ログ出力例（デバッグ目的）
					// printf("Detected Refresh Rate: %.3f Hz\n",
					//        (double)closestMatch.RefreshRate.Numerator / closestMatch.RefreshRate.Denominator);
					log_printf("モニタからリフレッシュレートを取得 >> 成功. リフレッシュレート = %.3f Hz\n", LogLevel::Success,
						static_cast<double>(closestMatch.RefreshRate.Numerator) / static_cast<double>(closestMatch.RefreshRate.Denominator));
				}
				else
				{
					// 取得失敗時は安全なデフォルト（60Hz）
					desc.BufferDesc.RefreshRate.Numerator = 60;
					desc.BufferDesc.RefreshRate.Denominator = 1;
				}
			}
		}
	}
	else
	{
		// DXGIファクトリ生成失敗時は安全なデフォルト
		desc.BufferDesc.RefreshRate.Numerator = 60;
		desc.BufferDesc.RefreshRate.Denominator = 1;
	}

	hr = dxgi_factory->CreateSwapChain(_device.Get(), &desc, _swapchain.GetAddressOf());
	dxgi_factory->Release();

	log_printf("スワップチェーンの生成 >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);
	return true;
}

bool Graphics_Core::create_render_target_view() {
	HRESULT hr = S_OK;
	log_printf("レンダーターゲットビューの生成開始\n", LogLevel::Info);
	ID3D11Texture2D* back_buffer = nullptr;

	// バックバッファ取得
	hr = _swapchain.Get()->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer));
	if (FAILED(hr)) {
		log_printf("バックバッファの取得 >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}

	// レンダーターゲットビュー作成
	hr = _device.Get()->CreateRenderTargetView(back_buffer, nullptr, _render_target_view.GetAddressOf());
	if (FAILED(hr)) {
		log_printf("レンダーターゲットビューの生成 >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		back_buffer->Release();
		return false;
	}
	log_printf("レンダーターゲットビューの生成 >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);
	back_buffer->Release();
	return true;
}


void Graphics_Core::set_viewport() {
	log_printf("ビューポートの設定開始\n", LogLevel::Info);
	_viewport.TopLeftX = 0;
	_viewport.TopLeftY = 0;
	_viewport.Width = static_cast<float>(SCREEN_WIDTH);
	_viewport.Height = static_cast<float>(SCREEN_HEIGHT);
	_viewport.MinDepth = graphics_constants::ViewportMinDepth;
	_viewport.MaxDepth = graphics_constants::ViewportMaxDepth;
	_immediate_context.Get()->RSSetViewports(1, &_viewport);
	log_printf("ビューポートの設定終了\n", LogLevel::Info);

}
void Graphics_Core::update_scene_constants(Camera& camera)
{
	using namespace DirectX;
	D3D11_VIEWPORT viewport;
	UINT num_viewports{ 1 };
	_immediate_context->RSGetViewports(&num_viewports, &viewport);
	Scene_Constants data{};

	float aspect_ratio = viewport.Width / viewport.Height;
	{
		dx::XMFLOAT4 light_view_focus = post_procss.GetShadow().get_light_view_focus();
		float light_view_distance = post_procss.GetShadow().get_light_view_distance();
		float light_view_size = post_procss.GetShadow().get_light_view_size();
		float light_view_near_z = post_procss.GetShadow().get_light_view_near_z();
		float light_view_far_z = post_procss.GetShadow().get_light_view_far_z();
		dx::XMVECTOR F{ XMLoadFloat4(&light_view_focus) };
		dx::XMVECTOR E{ F - dx::XMVector3Normalize(dx::XMLoadFloat4(&directional_light_direction)) * light_view_distance };
		dx::XMVECTOR U{ dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) };
		XMMATRIX V{ XMMatrixLookAtLH(E, F, U) };
		XMMATRIX P{ XMMatrixOrthographicLH(light_view_size * 1, light_view_size, light_view_near_z, light_view_far_z) };

		DirectX::XMStoreFloat4x4(&data.light_view_projection, V * P);
	}



	dx::XMMATRIX V = camera.view;
	dx::XMMATRIX P = camera.projection;

	dx::XMStoreFloat4x4(&data.view_projection, V * P);
	dx::XMStoreFloat4x4(&data.inv_view_projection, dx::XMMatrixInverse(nullptr, V * P));

	XMStoreFloat4(&data.camera_position, camera.position);

	_immediate_context->UpdateSubresource(_constant_buffers[static_cast<int>(Conastant_Buffer_Type::Scene)].Get(), 0, nullptr, &data, 0, 0);

	_immediate_context->VSSetConstantBuffers(1, 1, _constant_buffers[static_cast<int>(Conastant_Buffer_Type::Scene)].GetAddressOf());
	_immediate_context->PSSetConstantBuffers(1, 1, _constant_buffers[static_cast<int>(Conastant_Buffer_Type::Scene)].GetAddressOf());
	Light_Constants lights{};
	lights.ambient_color = ambient_color;
	lights.directional_light_direction = directional_light_direction;
	lights.directional_light_color = directional_light_color;
	_immediate_context->UpdateSubresource(_constant_buffers[static_cast<int>(Conastant_Buffer_Type::Light)].Get(), 0, 0, &lights, 0, 0);
	_immediate_context->VSSetConstantBuffers(2, 1, _constant_buffers[static_cast<int>(Conastant_Buffer_Type::Light)].GetAddressOf());
	_immediate_context->PSSetConstantBuffers(2, 1, _constant_buffers[static_cast<int>(Conastant_Buffer_Type::Light)].GetAddressOf());



}