#include "sprite.h"
#include <sstream>
#include <WICTextureLoader.h>
#include "Engine/Graphics/texture/texture.h"
#include "Engine/Graphics/shader/shader.h"

sprite::sprite(ID3D11Device* device, const wchar_t* filename) {
	create_vs_buffer_object(device);
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		 { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};


	create_vs_from_cso(device, "sprite_vs.cso", &_vertex_shader, &_input_layout, layout, 3);
	create_ps_from_cso(device, "sprite_ps.cso", &_pixel_shader);
	/// --- テクスチャの読み込み ---
	load_texture_from_file(device, filename, &_shader_resource_view, &_texture2d_desc);

	/// --- 不要なテクスチャリソースの開放 ---
	release_all_textures();

}
bool sprite::create_vs_buffer_object(ID3D11Device* device) {
	HRESULT hr{ S_OK };

	Vertex vertices[]
	{
	  { { -1.0, +1.0, 0 }, { 1, 1, 1, 1 }, { 0, 0 } },
	  { { +1.0, +1.0, 0 }, { 1, 1, 1, 1 }, { 1, 0 } },
	  { { -1.0, -1.0, 0 }, { 1, 1, 1, 1 }, { 0, 1 } },
	  { { +1.0, -1.0, 0 }, { 1, 1, 1, 1 }, { 1, 1 } },
	};

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(vertices);
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA subresource_data{};
	subresource_data.pSysMem = vertices;
	subresource_data.SysMemPitch = 0;
	subresource_data.SysMemSlicePitch = 0;
	hr = device->CreateBuffer(&buffer_desc, &subresource_data, &_vertex_buffer);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));


	hr = device->CreateBuffer(&buffer_desc, &subresource_data, &_vertex_buffer);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	if (FAILED(hr)) {
		log_printf("スプライトー＞頂点バッファオブジェクトの生成 >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("スプライトー＞頂点バッファオブジェクトの生成 >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);
	return true;
}
void sprite::render(ID3D11DeviceContext* immediate_context,
	float dx, float dy,         // 矩形の左上の座標（スクリーン座標系）
	float dw, float dh,         // 矩形のサイズ（スクリーン座標系）
	const FLOAT ColorRGBA[4],    // 色
	float angle                  // 回転角度（ラジアン単位）
)
{
	render(immediate_context, dx, dy, dw, dh, ColorRGBA, angle, 0.0f, 0.0f,
		static_cast<float>(_texture2d_desc.Width),
		static_cast<float>(_texture2d_desc.Height));

}
/**
 * @brief テクスチャの一部を指定してスプライトを描画
 *
 * @param immediate_context Direct3Dデバイスコンテキスト
 * @param dx 描画X位置
 * @param dy 描画Y位置
 * @param dw 幅
 * @param dh 高さ
 * @param r 色（赤）
 * @param g 色（緑）
 * @param b 色（青）
 * @param a 色（アルファ）
 * @param angle 回転角度（度）
 * @param sx テクスチャのX位置（切り出し元）
 * @param sy テクスチャのY位置（切り出し元）
 * @param sw テクスチャの幅（切り出しサイズ）
 * @param sh テクスチャの高さ（切り出しサイズ）
 */
void sprite::render(ID3D11DeviceContext* immediate_context,
	float dx, float dy, float dw, float dh,
	const FLOAT ColorRGBA[4],
	float angle, float sx, float sy, float sw, float sh)
{

	D3D11_VIEWPORT viewport{};
	UINT num_viewports = 1;
	immediate_context->RSGetViewports(&num_viewports, &viewport);

	/// --- スプライトの矩形頂点（4点）を計算 ---
	float x0{ dx }, y0{ dy };
	float x1{ dx + dw }, y1{ dy };
	float x2{ dx }, y2{ dy + dh };
	float x3{ dx + dw }, y3{ dy + dh };

	/// --- 回転を適用（スプライトの中心を基準に） ---
	auto rotate = [](float& x, float& y, float cx, float cy, float angle)
		{
			x -= cx; y -= cy;
			float cos = cosf(DirectX::XMConvertToRadians(angle));
			float sin = sinf(DirectX::XMConvertToRadians(angle));
			float tx = x, ty = y;
			x = cos * tx - sin * ty;
			y = sin * tx + cos * ty;
			x += cx; y += cy;
		};

	float cx = dx + dw * 0.5f;
	float cy = dy + dh * 0.5f;
	rotate(x0, y0, cx, cy, angle);
	rotate(x1, y1, cx, cy, angle);
	rotate(x2, y2, cx, cy, angle);
	rotate(x3, y3, cx, cy, angle);

	/// --- 座標をNDC（[-1,1]）に変換 ---
	x0 = 2.0f * x0 / viewport.Width - 1.0f;
	y0 = 1.0f - 2.0f * y0 / viewport.Height;
	x1 = 2.0f * x1 / viewport.Width - 1.0f;
	y1 = 1.0f - 2.0f * y1 / viewport.Height;
	x2 = 2.0f * x2 / viewport.Width - 1.0f;
	y2 = 1.0f - 2.0f * y2 / viewport.Height;
	x3 = 2.0f * x3 / viewport.Width - 1.0f;
	y3 = 1.0f - 2.0f * y3 / viewport.Height;

	/// --- 頂点バッファをCPUからマップ（書き換え準備） ---
	D3D11_MAPPED_SUBRESOURCE mapped_subresource{};
	HRESULT hr = immediate_context->Map(_vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	Vertex* vertices = reinterpret_cast<Vertex*>(mapped_subresource.pData);
	if (vertices)
	{
		vertices[0].position = { x0, y0, 0 };
		vertices[1].position = { x1, y1, 0 };
		vertices[2].position = { x2, y2, 0 };
		vertices[3].position = { x3, y3, 0 };

		vertices[0].color = vertices[1].color = vertices[2].color = vertices[3].color = { ColorRGBA[0], ColorRGBA[1], ColorRGBA[2], ColorRGBA[3] };

		float u0 = sx / _texture2d_desc.Width;
		float v0 = sy / _texture2d_desc.Height;
		float u1 = (sx + sw) / _texture2d_desc.Width;
		float v1 = (sy + sh) / _texture2d_desc.Height;

		vertices[0].texcoord = { u0, v0 };
		vertices[1].texcoord = { u1, v0 };
		vertices[2].texcoord = { u0, v1 };
		vertices[3].texcoord = { u1, v1 };
	}
	immediate_context->Unmap(_vertex_buffer, 0);

	/// --- パイプライン設定と描画実行 ---
	UINT stride{ sizeof(Vertex) };
	UINT offset{ 0 };
	immediate_context->IASetVertexBuffers(0, 1, &_vertex_buffer, &stride, &offset);
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	immediate_context->IASetInputLayout(_input_layout);
	immediate_context->VSSetShader(_vertex_shader, nullptr, 0);
	immediate_context->PSSetShader(_pixel_shader, nullptr, 0);
	immediate_context->PSSetShaderResources(0, 1, &_shader_resource_view);
	immediate_context->Draw(4, 0);
}
bool sprite::create_vs_shader_object_and_input_layout(ID3D11Device* device) {
	HRESULT hr{ S_OK };

	const char* cso_name{ "./data/shaders/cso/sprite_vs.cso" };

	FILE* fp{};
	fopen_s(&fp, cso_name, "rb");

	if (!fp) {
		log_printf("スプライトー＞vsシェーダーの取得 >> 失敗", LogLevel::Warning);
	}
	fseek(fp, 0, SEEK_END);
	long cso_sz{ ftell(fp) };
	fseek(fp, 0, SEEK_SET);

	std::unique_ptr<unsigned char[]> cso_data{ std::make_unique<unsigned char[]>(cso_sz) };
	fread(cso_data.get(), cso_sz, 1, fp);
	fclose(fp);

	hr = device->CreateVertexShader(cso_data.get(), cso_sz, nullptr, &_vertex_shader);
	if (FAILED(hr)) {
		log_printf("スプライトシェーダーオブジェクトの生成 >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("スプライトシェーダーオブジェクトの生成 >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		 { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};


	UINT num_elements = ARRAYSIZE(layout);
	hr = device->CreateInputLayout(layout, num_elements, cso_data.get(), cso_sz, &_input_layout);
	if (FAILED(hr)) {
		log_printf("スプライトー＞入力レイアウトの生成 >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("スプライトー＞入力レイアウトの生成 >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);
	return true;
}

bool sprite::create_ps_shader_object(ID3D11Device* device) {
	HRESULT hr{ S_OK };
	const char* cso_name{ "./data/shaders/cso/sprite_ps.cso" };
	FILE* fp{};
	fopen_s(&fp, cso_name, "rb");
	if (!fp) {
		log_printf("スプライトー＞psシェーダーの取得 >> 失敗", LogLevel::Warning);
		return false;
	}
	fseek(fp, 0, SEEK_END);
	long cso_sz{ ftell(fp) };
	fseek(fp, 0, SEEK_SET);
	std::unique_ptr<unsigned char[]> cso_data{ std::make_unique<unsigned char[]>(cso_sz) };
	fread(cso_data.get(), cso_sz, 1, fp);
	fclose(fp);
	hr = device->CreatePixelShader(cso_data.get(), cso_sz, nullptr, &_pixel_shader);
	if (FAILED(hr)) {
		log_printf("スプライトー＞ピクセルシェーダーの生成 >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
		return false;
	}
	log_printf("スプライトー＞ピクセルシェーダーの生成 >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);
	return true;
}
//bool sprite::create_shader_resource_view_object(ID3D11Device* device, const wchar_t* filename) {
//	ID3D11Resource* resource{};
//	HRESULT hr{ S_OK };
//	hr = DirectX::CreateWICTextureFromFile(device, filename, &resource, &shader_resource_view);
//	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
//	resource->Release();
//
//	if (FAILED(hr)) {
//		std::wstringstream wss;
//		wss << L"スプライトー＞テクスチャの生成 >> 失敗. ファイル名: " << filename << L", HRESULT = 0x" << std::hex << hr;
//		log_printf(std::string(wss.str().begin(), wss.str().end()).c_str(), LogLevel::Error);
//		return false;
//	}
//	log_printf("スプライトー＞テクスチャの生成 >> 成功. ファイル名: %ls, HRESULT = 0x%08X\n", LogLevel::Success, filename, hr);
//
//
//
//	return true;
//}
//
//bool sprite::create_texture2d_desc(ID3D11Device* device) {
//	HRESULT hr{ S_OK };
//	ID3D11Resource* resource{};
//	shader_resource_view->GetResource(&resource);
//	ID3D11Texture2D* texture2d{};
//	hr = resource->QueryInterface(IID_PPV_ARGS(&texture2d));
//	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
//	texture2d->GetDesc(&texture2d_desc);
//	texture2d->Release();
//	resource->Release();
//	if (FAILED(hr)) {
//		log_printf("スプライトー＞テクスチャの情報取得 >> 失敗. HRESULT = 0x%08X\n", LogLevel::Error, hr);
//		return false;
//	}
//	log_printf("スプライトー＞テクスチャの情報取得 >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);
//	return true;
//}