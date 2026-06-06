#pragma once
#include <d3d11.h>
#include <winnt.h>
#include <fstream>
#include <memory>
using namespace std;

#include "Engine/Utilities/misc.h"

// 頂点シェーダーをCSOファイル（バイナリ形式のコンパイル済みシェーダーオブジェクト）から生成する関数
// device: Direct3Dデバイス
// cso_name: シェーダーバイナリファイル名（.csoファイルのパス）
// vertex_shader: 作成された頂点シェーダーの格納先ポインタ
// input_layout: 頂点入力レイアウトの格納先ポインタ
// input_element_desc: 頂点入力レイアウトの定義配列（D3D11_INPUT_ELEMENT_DESCの配列）
// num_elements: 入力レイアウトの要素数（配列の長さ）
// 戻り値: HRESULT（Direct3D APIの成否コード）
// シェーダーのバイナリファイルを読み込み、Direct3Dデバイス上に頂点シェーダーと入力レイアウトを作成する
bool create_vs_from_cso(ID3D11Device* device, const char* cso_name, ID3D11VertexShader** vertex_shader,
	ID3D11InputLayout** input_layout, D3D11_INPUT_ELEMENT_DESC* input_element_desc, UINT num_elements);

// ピクセルシェーダーをCSOファイル（バイナリ形式のコンパイル済みシェーダーオブジェクト）から生成する関数
// device: Direct3Dデバイス
// cso_name: シェーダーバイナリファイル名（.csoファイルのパス）
// pixel_shader: 作成されたピクセルシェーダーの格納先ポインタ
// 戻り値: HRESULT（Direct3D APIの成否コード）
// シェーダーのバイナリファイルを読み込み、Direct3Dデバイス上にピクセルシェーダーを作成する
bool create_ps_from_cso(ID3D11Device* device, const char* cso_name, ID3D11PixelShader** pixel_shader);