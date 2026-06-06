#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <memory>

#include <DirectXMath.h>

#include "Engine/utilities/color_util.h"
#include "Engine/System/graphics_constants.h"
#include "Engine/System/render_state.h"
#include "Engine/utilities/dx_common.h"

#include "Engine/utilities/misc.h"
#include "Game/World/camera/camera_manager.h"
#include "Engine/System/Manager/PostProcess/post_process_manager.h"
#include "Engine/Graphics/geometry_buffer/geometry_buffer.h"
#include "Engine/System/Manager/Light/point_light_manager.h"
#include "Engine/System/Manager/Light/spot_light_manager.h"
#include "Engine/System/Manager/Light/area_light_manager.h"


CONST LONG SCREEN_WIDTH{ 1280 };
CONST LONG SCREEN_HEIGHT{ 720 };

CONST BOOL FULLSCREEN{ FALSE };

class Graphics_Core
{
public:
	struct Scene_Constants
	{
		DirectX::XMFLOAT4X4 view_projection;         //ビュー・プロジェクション変換行列
		DirectX::XMFLOAT4X4 inv_view_projection;         //ビュー・プロジェクション変換行列
		DirectX::XMFLOAT4 camera_position;          //カメラの位置
		DirectX::XMFLOAT4X4 light_view_projection;

	};
	struct Light_Constants
	{
		DirectX::XMFLOAT4 ambient_color;
		DirectX::XMFLOAT4 directional_light_direction;
		DirectX::XMFLOAT4 directional_light_color;

	};
	enum class Conastant_Buffer_Type
	{
		Scene,
		Light,
		Num
	};
private:
	DirectX::XMFLOAT4 ambient_color{ 0.2f, 0.2f, 0.2f, 0.2f };
	DirectX::XMFLOAT4 directional_light_direction{ 0.0f, -1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT4 directional_light_color{ 1.0f, 1.0f, 1.0f, 1.0f };

	/*	DirectX::XMFLOAT4 light_view_focus{ 0, 0, 0, 1 };
		float light_view_distance{ 10.0f };
		float light_view_size{ 12.0f };
		float light_view_near_z{ 2.0f };
		float light_view_far_z{ 18.0f };*/
	PointLightManager point_light_manager;
	SpotLightManager spot_light_manager;
	AreaLightManager area_light_manager;
public:
	PointLightManager& get_point_light_manager() { return point_light_manager; }
	SpotLightManager& get_spot_light_manager() { return spot_light_manager; }
	AreaLightManager& get_area_light_manager() { return area_light_manager; }


private:
	bool create_device();
	bool create_swap_chain();
	bool create_render_target_view();
	bool create_depth_stencil_view();
	void set_viewport();
	HWND _hwnd = nullptr;
	ComPtr<ID3D11Device> _device;
	ComPtr<ID3D11DeviceContext> _immediate_context;
	ComPtr<IDXGISwapChain> _swapchain;
	ComPtr<ID3D11RenderTargetView> _render_target_view;
	std::unique_ptr<GeometryBuffer> _g_buffer;
	//ComPtr<ID3D11DepthStencilView> _depth_stencil_view;
	D3D11_VIEWPORT _viewport = {};

	std::unique_ptr<Render_State>					_render_state;
	ComPtr<ID3D11Buffer> _constant_buffers[static_cast<int>(Conastant_Buffer_Type::Num)];

	std::unique_ptr<FullscreenQuad> bit_block_transfer;
public:
	ID3D11Buffer* get_constant_buffer(Conastant_Buffer_Type type) {
		return _constant_buffers[static_cast<int>(type)].Get();
	}
	ID3D11Buffer** get_constant_buffer_address(Conastant_Buffer_Type type) {
		return _constant_buffers[static_cast<int>(type)].GetAddressOf();

	}
	Render_State* get_render_state() const {
		return _render_state.get();
	}

private:

	float _screen_width = 0;
	float _screen_height = 0;



public:
	Graphics_Core() = default;
	~Graphics_Core() = default;
	Graphics_Core(const Graphics_Core&) = delete;
	Graphics_Core& operator=(const Graphics_Core&) = delete;

public:
	// インスタンス取得
	static Graphics_Core& instance()
	{
		static Graphics_Core instance;
		return instance;
	}

	// 初期化
	void initialize(HWND hwnd);

	// クリア
	void clear(Color_Utils::Color color);

	// レンダーターゲット設定
	void set_render_targets();
	//スクリーンクアド取得
	FullscreenQuad* get_fullscreen_quad()
	{
		if (bit_block_transfer == nullptr)
		{
			bit_block_transfer = std::make_unique<FullscreenQuad>(_device.Get());
		}
		return bit_block_transfer.get();
	}

	// 画面表示
	void present(UINT sync_interval);

	// ウインドウハンドル取得
	HWND get_window_handle() { return _hwnd; }

	// デバイス取得
	ID3D11Device* get_device() { return _device.Get(); }

	IDXGISwapChain* get_swap_chain() const { return _swapchain.Get(); }
	// デバイスコンテキスト取得
	ID3D11DeviceContext* get_device_context() {

		return _immediate_context.Get();
	}
	// レンダーターゲットビュー取得
	ComPtr<ID3D11RenderTargetView> get_render_target_view() const
	{
		return _render_target_view;
	}

	// レンダーステート取得
	Render_State* get_render_state() { return _render_state.get(); }
	GeometryBuffer* get_geometry_buffer() { return _g_buffer.get(); }

	void get_viewport(D3D11_VIEWPORT& out_viewport) const
	{
		out_viewport = _viewport;
	}

	// スクリーン幅取得
	float get_screen_width() const { return _screen_width; }

	// スクリーン高さ取得
	float get_screen_height() const { return _screen_height; }
	void set_borderless_window() {
		// ウィンドウ枠を削除
		//SetWindowLong(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);
	};
	void unset_borderless_window() {
		//SetWindowLong(hwnd_, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);

	};
	void resize_window(int target_width = SCREEN_WIDTH, int target_height = SCREEN_HEIGHT)
	{
		// 現在のウィンドウスタイル取得
		DWORD style = static_cast<DWORD>(GetWindowLong(_hwnd, GWL_STYLE));

		// サイズ変更関連のスタイルを削除（リサイズ禁止）
		style &= ~WS_THICKFRAME;    // 枠ドラッグによるサイズ変更禁止
		style &= ~WS_MAXIMIZEBOX;   // 最大化ボタン無効化

		// 更新したスタイルを適用
		SetWindowLong(_hwnd, GWL_STYLE, style);

		// スタイルに応じたウィンドウサイズ補正
		RECT rc{ 0, 0, target_width, target_height };
		AdjustWindowRect(&rc, style, FALSE);
		int window_width = rc.right - rc.left;
		int window_height = rc.bottom - rc.top;

		// 画面中央に配置
		int screen_width = GetSystemMetrics(SM_CXSCREEN);
		int screen_height = GetSystemMetrics(SM_CYSCREEN);
		int x = (screen_width - window_width) / 2;
		int y = (screen_height - window_height) / 2;

		_screen_width = static_cast<float>(target_width);
		_screen_height = static_cast<float>(target_height);

		// サイズと位置を変更
		SetWindowPos(_hwnd, nullptr, x, y, window_width, window_height,
			SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		create_render_target_view();
	}



	float get_aspect_ratio() const { return _screen_width / _screen_height; }
	float get_primary_aspect_ratio() const { return static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT); }

	void create_constant_buffer()
	{
		HRESULT hr{ S_OK };
		D3D11_BUFFER_DESC buffer_desc{};
		{
			buffer_desc.ByteWidth = sizeof(Scene_Constants);
			buffer_desc.Usage = D3D11_USAGE_DEFAULT;
			buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			buffer_desc.CPUAccessFlags = 0;
			buffer_desc.MiscFlags = 0;
			buffer_desc.StructureByteStride = 0;
			hr = _device->CreateBuffer(&buffer_desc, nullptr, _constant_buffers[static_cast<int>(Conastant_Buffer_Type::Scene)].GetAddressOf());
		}
		{
			buffer_desc.ByteWidth = sizeof(Light_Constants);
			hr = _device->CreateBuffer(&buffer_desc, nullptr, _constant_buffers[static_cast<int>(Conastant_Buffer_Type::Light)].GetAddressOf());
		}
	}
	void update_constants()
	{
		update_scene_constants(Camera_Manager::instance().get_active_camera()->get_camera());
	}

	void set_directional_light(const dx::XMFLOAT4 ambient, const dx::XMFLOAT4& direction, const dx::XMFLOAT4& color)
	{
		ambient_color = ambient;
		directional_light_direction = direction;
		directional_light_color = color;
	}
	DirectX::XMFLOAT4 get_ambient_color() const { return ambient_color; }
	DirectX::XMFLOAT4 get_directional_light_direction() const { return directional_light_direction; }
	DirectX::XMFLOAT4 get_directional_light_color() const { return directional_light_color; }
private:

	void update_scene_constants(Camera& camera);

public:
	Post_Process_Manager post_procss;
};
