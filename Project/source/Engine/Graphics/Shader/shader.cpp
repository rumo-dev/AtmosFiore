#include"shader.h"
#include <sstream>
#include <memory>
#include <string>

// Helper to load CSO file
static bool load_cso(const char* cso_name, std::unique_ptr<unsigned char[]>& cso_data, size_t& cso_sz)
{
    std::string base_path = "./data/shaders/cso/";
    std::string full_path = base_path + cso_name;
    FILE* fp{ nullptr };
    fopen_s(&fp, full_path.c_str(), "rb");
    _ASSERT_EXPR_A(fp, "CSO File not found");
    if (!fp) return false;

    fseek(fp, 0, SEEK_END);
    long sz{ ftell(fp) };
    fseek(fp, 0, SEEK_SET);

    cso_data = std::make_unique<unsigned char[]>(sz);
    fread(cso_data.get(), sz, 1, fp);
    fclose(fp);
    cso_sz = sz;
    return true;
}

// 頂点シェーダーをCSOファイルから生成する関数
// device: Direct3Dデバイス
// cso_name: シェーダーバイナリ（CSO）ファイル名
// vertex_shader: 作成された頂点シェーダーの格納先ポインタ
// input_layout: 入力レイアウトの格納先ポインタ
// input_element_desc: 頂点入力レイアウトの定義配列
// num_elements: 入力レイアウトの要素数
bool create_vs_from_cso(ID3D11Device* device, const char* cso_name, ID3D11VertexShader** vertex_shader,
	ID3D11InputLayout** input_layout, D3D11_INPUT_ELEMENT_DESC* input_element_desc, UINT num_elements)
{
	std::unique_ptr<unsigned char[]> cso_data;
	size_t cso_sz{};
	if (!load_cso(cso_name, cso_data, cso_sz))
		return false;

	HRESULT hr{ S_OK };
	// 頂点シェーダーの作成
	hr = device->CreateVertexShader(cso_data.get(), static_cast<UINT>(cso_sz), nullptr, vertex_shader);

	// 入力レイアウトの作成（必要な場合のみ実行）
	if (input_layout)
	{
		hr = device->CreateInputLayout(input_element_desc, num_elements,
			cso_data.get(), static_cast<UINT>(cso_sz), input_layout);
	}
	if (FAILED(hr)) {
		std::wstringstream wss;
		wss << L"頂点シェーダーの生成 >> 失敗. ファイル名: " << cso_name << L", HRESULT = 0x" << std::hex << hr;
		log_printf(std::string(wss.str().begin(), wss.str().end()).c_str(), LogLevel::Error);
		return false;
	}
	log_printf("頂点シェーダーの生成 >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);
	return true;
}

// ピクセルシェーダーをCSOファイルから生成する関数
// device: Direct3Dデバイス
// cso_name: シェーダーバイナリ（CSO）ファイル名
// pixel_shader: 作成されたピクセルシェーダーの格納先ポインタ
bool create_ps_from_cso(ID3D11Device* device, const char* cso_name, ID3D11PixelShader** pixel_shader)
{
	std::unique_ptr<unsigned char[]> cso_data;
	size_t cso_sz{};
	if (!load_cso(cso_name, cso_data, cso_sz))
		return false;

	HRESULT hr{ S_OK };
	// ピクセルシェーダーの作成
	hr = device->CreatePixelShader(cso_data.get(), static_cast<UINT>(cso_sz), nullptr, pixel_shader);

	if (FAILED(hr)) {
		std::wstringstream wss;
		wss << L"ピクセルシェーダーの生成 >> 失敗. ファイル名: " << cso_name << L", HRESULT = 0x" << std::hex << hr;
		log_printf(std::string(wss.str().begin(), wss.str().end()).c_str(), LogLevel::Error);
		return false;
	}
	log_printf("ピクセルシェーダーの生成 >> 成功. HRESULT = 0x%08X\n", LogLevel::Success, hr);

	return true;
}
