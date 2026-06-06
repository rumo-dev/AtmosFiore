#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <regex>
#include <unordered_map>
#include <iostream>
#include <d3d11.h>

/**
 * @brief 入力レイアウト要素情報
 *
 * HLSLコメントから抽出される頂点入力情報を保持する。
 */
struct InputElementDescInfo {
	std::string semanticName; ///< セマンティク名（例: POSITION, TEXCOORD）
	UINT semanticIndex = 0;   ///< セマンティクインデックス（例: TEXCOORD0 → 0）

	DXGI_FORMAT format = DXGI_FORMAT_R32G32B32_FLOAT; ///< データフォーマット

	UINT inputSlot = 0; ///< 入力スロット
	bool perInstance = false; ///< インスタンスデータかどうか
	UINT instanceDataStepRate = 0; ///< インスタンスステップレート
};

/**
 * @brief InputLayoutパーサ
 *
 * HLSLファイル内のコメントから頂点入力情報を抽出し、
 * D3D11のInputLayout生成に利用する。
 *
 * @remark
 * 使用例（HLSL）:
 * @code
 * // @VertexInput POSITION float3
 * // @VertexInput TEXCOORD0 float2
 * // @VertexInput COLOR float4 0 1 1
 * @endcode
 *
 * @warning
 * - コメントフォーマットが一致しない場合はパースされない
 * - HLSLとInputLayoutの不一致は描画バグの原因となる
 *
 * @todo
 * - DXCリフレクション対応（コメント不要化）
 * - JSON/YAML出力対応
 * - 型チェック強化
 */
namespace InputLayoutParser {

	/**
	 * @brief HLSLファイルから入力レイアウトを解析
	 *
	 * @param hlslPath HLSLファイルパス
	 * @return std::vector<InputElementDescInfo>
	 *
	 * @note
	 * コメント形式:
	 * // @VertexInput <SEMANTIC> <TYPE> [semanticIndex] [inputSlot] [perInstance]
	 */
	std::vector<InputElementDescInfo> ParseFromHLSL(const std::wstring& hlslPath);

	/**
	 * @brief レイアウトをファイルに保存
	 *
	 * @param layoutPath 出力ファイルパス
	 * @param infos 入力レイアウト情報
	 * @return 成功した場合 true
	 *
	 * @remark
	 * 簡易テキスト形式（JSONではない）
	 */
	bool SaveLayoutToFile(const std::wstring& layoutPath,
		const std::vector<InputElementDescInfo>& infos);

	/**
	 * @brief レイアウトファイルをロード
	 *
	 * @param layoutPath ファイルパス
	 * @return 入力レイアウト情報
	 */
	std::vector<InputElementDescInfo> LoadLayoutFromFile(const std::wstring& layoutPath);

	/**
	 * @brief DXGI_FORMAT → 文字列変換
	 */
	std::string FormatToString(DXGI_FORMAT f);

	/**
	 * @brief 文字列 → DXGI_FORMAT変換
	 */
	DXGI_FORMAT StringToFormat(const std::string& s);

} // namespace InputLayoutParser