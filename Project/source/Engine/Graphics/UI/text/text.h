#pragma once

#include <string>						// 文字列
#include <vector>						// 動的配列
#include <wrl.h>						// ComPtr

#pragma warning(push)
#pragma warning(disable:4005)

#include <d2d1.h>						// Direct2D
#include <DWrite.h>						// DirectWrite
#include <DirectXmath.h>					// 数学系ライブラリ

#pragma warning(pop)

#pragma comment(lib,"d2d1.lib")			// Direct2D用
#pragma comment(lib,"Dwrite.lib")		// DirectWrite用

using namespace Microsoft;
#include "Engine/Utilities/dx_common.h"
#include "Engine/System/graphics_core.h"

class Custom_Font_Collection_Loader;

//=============================================================================
//		フォントの保存場所
//=============================================================================
/**
 * @namespace Font_List
 * @brief フォントファイルのパスを管理する名前空間
 */
namespace font_list
{
	/**
	 * @brief 利用可能なフォントファイルのパス一覧
	 */
	const std::wstring Font_Path[] =
	{
		L"./data/fonts/27_ninaroman-Regular.otf",
		L"./data/fonts/AkazukiPOP.otf",
		L"./data/fonts/Amari_Font_30-100-Regular.otf",
		L"./data/fonts/aoharu-marker-mini.ttf",
		L"./data/fonts/Arikko-Regular.otf",
		L"./data/fonts/BestTen-CRT.otf",
		L"./data/fonts/BestTen-DOT.otf",
		L"./data/fonts/DokiDokiFantasia.otf",
		L"./data/fonts/EMO.otf",
		L"./data/fonts/FUTENE.ttf",
		L"./data/fonts/g_brushtappitsu_freeB.ttf",
		L"./data/fonts/genkai-mincho.ttf",
		L"./data/fonts/HOSHINOTIRABARI.ttf",
		L"./data/fonts/Isego.otf",
		L"./data/fonts/karakaze-R.ttf",
		L"./data/fonts/Kei_Ji-P.ttf",
		L"./data/fonts/LightNovelPOPv2.otf",
		L"./data/fonts/M+A1_regular-35-2.0.otf",
		L"./data/fonts/MatatakiMincho.otf",
		L"./data/fonts/Mbf-Regular.ttf",
		L"./data/fonts/migikataagari.ttf",
		L"./data/fonts/MRT-すぷめら.ttf",
		L"./data/fonts/MRT-まるこいあすα.ttf",
		L"./data/fonts/nagino.otf",
		L"./data/fonts/N-utamin=02=20220217.otf",
		L"./data/fonts/oshigo.otf",
		L"./data/fonts/PENGS.ttf",
		L"./data/fonts/RailwayTypeface-R.otf",
		L"./data/fonts/rounded-x-mgenplus-1c-regular.ttf",
		L"./data/fonts/SoukouMincho.ttf",
		L"./data/fonts/STYK_MellowMelody.otf",
		L"./data/fonts/TekitouPoem.ttf",
		L"./data/fonts/tsumugi.ttf",
		L"./data/fonts/TsunagiGothic.ttf",
		L"./data/fonts/watashinotameni-Regular.ttf",
		L"./data/fonts/YasashisaGothicBold-V2.otf",
		L"./data/fonts/YDWaosagi.otf",
		L"./data/fonts/YonagaOldMincho-Regular.ttf",
		L"./data/fonts/yosugaraver1_2.ttf",
		L"./data/fonts/ZeroGothic.otf",
		L"./data/fonts/アプリ明朝.otf",
		L"./data/fonts/しょかきさらり行体.ttf",
		L"./data/fonts/リングスオブサターン.otf",
		L"./data/fonts/時雨院.ttf",
		L"./data/fonts/瀞ノグリッチ明朝H1.otf",


	};
}

/**
 * @enum Font_Name
 * @brief 利用可能なフォントの列挙型
 */
enum Font_Name {
	Font_27_ninaroman,
	Font_AkazukiPOP,
	Font_Amari_Font,
	Font_aoharu_marker_mini,
	Font_Arikko,
	Font_BestTen_CRT,
	Font_BestTen_DOT,
	Font_DokiDokiFantasia,
	Font_EMO,
	Font_FUTENE,
	Font_g_brushtappitsu_freeB,
	Font_genkai_mincho,
	Font_HOSHINOTIRABARI,
	Font_Isego,
	Font_karakaze_R,
	Font_Kei_Ji_P,
	Font_LightNovelPOPv2,
	Font_M_A1_regular,
	Font_MatatakiMincho,
	Font_Mbf_Regular,
	Font_migikataagari,
	Font_MRT_supumera,
	Font_MRT_marukoiasu_alpha,
	Font_nagino,
	Font_N_utamin_02_20220217,
	Font_oshigo,
	Font_PENGS,
	Font_RailwayTypeface_R,
	Font_rounded_x_mgenplus_1c_regular,
	Font_SoukouMincho,
	Font_STYK_MellowMelody,
	Font_TekitouPoem,
	Font_tsumugi,
	Font_TsunagiGothic,
	Font_watashinotameni_Regular,
	Font_YasashisaGothicBold_V2,
	Font_YDWaosagi,
	Font_YonagaOldMincho_Regular,
	Font_yosugaraver1_2,
	Font_ZeroGothic,
	Font_ApplicaMincho,
	Font_shokakisarari_Gyotai,
	Font_RingsofSaturn,
	Font_Shigurein,
	Font_ToronoGlitchMinchoH1,

};

//=============================================================================
//		フォント設定
//=============================================================================
/**
 * @struct Font_Data
 * @brief フォント設定情報を保持する構造体
 */
struct Font_Data
{
	std::wstring font;							///< フォント名
	IDWriteFontCollection* fontCollection;		///< フォントコレクション
	DWRITE_FONT_WEIGHT fontWeight;				///< フォントの太さ
	DWRITE_FONT_STYLE fontStyle;				///< フォントスタイル
	DWRITE_FONT_STRETCH fontStretch;			///< フォントの幅
	FLOAT fontSize;								///< フォントサイズ
	WCHAR const* localeName;					///< ロケール名
	DWRITE_TEXT_ALIGNMENT textAlignment;		///< テキストの配置
	DWRITE_PARAGRAPH_ALIGNMENT textParagraphAlignment;///< テキストの配置

	D2D1_COLOR_F Color;							///< フォントの色

	D2D1_COLOR_F shadowColor;					///< 影の色
	D2D1_POINT_2F shadowOffset;					///< 影のオフセット

	/**
	 * @brief デフォルト設定のコンストラクタ
	 */
	Font_Data()
	{
		font = L"";
		fontCollection = nullptr;
		fontWeight = DWRITE_FONT_WEIGHT::DWRITE_FONT_WEIGHT_NORMAL;
		fontStyle = DWRITE_FONT_STYLE::DWRITE_FONT_STYLE_NORMAL;
		fontStretch = DWRITE_FONT_STRETCH::DWRITE_FONT_STRETCH_NORMAL;
		fontSize = 20;
		localeName = L"ja-jp";
		textAlignment = DWRITE_TEXT_ALIGNMENT::DWRITE_TEXT_ALIGNMENT_LEADING;
		textParagraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT::DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
		Color = D2D1::ColorF(D2D1::ColorF::White);

		shadowColor = D2D1::ColorF(D2D1::ColorF::Black);
		shadowOffset = D2D1::Point2F(2.0f, -2.0f);
	}
};

//=============================================================================
//		DirectWrite
//=============================================================================
/**
 * @class text_write
 * @brief DirectWriteを用いたテキスト描画クラス
 */
class Text_Write
{
public:

	/**
	 * @brief デフォルトコンストラクタを禁止
	 */
	Text_Write() = delete;

	/**
	 * @brief フォント設定を受け取るコンストラクタ
	 * @param set フォント設定データ
	 */
	Text_Write(Font_Data* set) :_setting(*set) {};

	/**
	 * @brief 初期化処理
	 * @param swapChain スワップチェーン
	 * @return HRESULT 結果コード
	 */
	HRESULT init(IDXGISwapChain* swapChain);

	/**
	 * @brief フォント設定（構造体指定）
	 * @param set フォント設定データ
	 * @return HRESULT 結果コード
	 */
	HRESULT set_font(Font_Data set);

	/**
	 * @brief フォント設定（個別指定）
	 * @param fontname フォント名
	 * @param fontWeight フォントの太さ
	 * @param fontStyle フォントスタイル
	 * @param fontStretch フォントの幅
	 * @param fontSize フォントサイズ
	 * @param localeName ロケール名
	 * @param textAlignment テキストの配置
	 * @param Color フォントの色
	 * @param shadowColor 影の色
	 * @param shadowOffset 影のオフセット
	 * @return HRESULT 結果コード
	 */
	HRESULT set_font
	(
		WCHAR const* fontname,
		DWRITE_FONT_WEIGHT		fontWeight,
		DWRITE_FONT_STYLE		fontStyle,
		DWRITE_FONT_STRETCH		fontStretch,
		FLOAT					fontSize,
		WCHAR const* localeName,
		DWRITE_TEXT_ALIGNMENT	textAlignment,
		DWRITE_PARAGRAPH_ALIGNMENT textParagraphAlignment,
		D2D1_COLOR_F			Color,
		D2D1_COLOR_F			shadowColor,
		D2D1_POINT_2F			shadowOffset
	);

	/**
	 * @brief 文字列を指定位置に描画する
	 * @param str 描画する文字列
	 * @param pos 描画位置
	 * @param options テキスト描画オプション
	 * @param shadow 影を描画するか
	 * @return HRESULT 結果コード
	 */
	HRESULT draw_text(std::string str, dx::XMFLOAT2 pos, D2D1_DRAW_TEXT_OPTIONS options, bool shadow = false);

	/**
	 * @brief 文字列を指定領域に描画する
	 * @param str 描画する文字列
	 * @param rect 描画領域
	 * @param options テキスト描画オプション
	 * @param shadow 影を描画するか
	 * @return HRESULT 結果コード
	 */
	HRESULT draw_text(std::string str, D2D1_RECT_F rect, D2D1_DRAW_TEXT_OPTIONS options, bool shadow = false);

	/**
	 * @brief 指定されたパスのフォントを読み込む
	 * @return HRESULT 結果コード
	 */
	HRESULT font_loder();

	/**
	 * @brief フォント名を取得する
	 * @param num フォント番号
	 * @return フォント名
	 */
	std::wstring get_font_name(int num);

	/**
	 * @brief 読み込んだフォント名の数を取得する
	 * @return フォント名の数
	 */
	int get_font_name_num();

	/**
	 * @brief フォント名を取得し直す
	 * @param custom_font_collection カスタムフォントコレクション
	 * @param locale ロケール名（省略可）
	 * @return HRESULT 結果コード
	 */
	HRESULT get_font_family_name(IDWriteFontCollection* custom_font_collection, const WCHAR* locale = L"ja-jp");

	/**
	 * @brief 全てのフォント名を取得し直す
	 * @param custom_font_collection カスタムフォントコレクション
	 * @return HRESULT 結果コード
	 */
	HRESULT get_all_font_family_names(IDWriteFontCollection* custom_font_collection);

	/**
	 * @brief カスタムフォントコレクション
	 */
	WRL::ComPtr <IDWriteFontCollection> font_collection = nullptr;

private:

	WRL::ComPtr <ID2D1Factory>			 _D2D_factory = nullptr;		///< Direct2Dリソース
	WRL::ComPtr <ID2D1RenderTarget>		_render_target = nullptr;	///< Direct2Dレンダーターゲット
	WRL::ComPtr <ID2D1SolidColorBrush>	_brush = nullptr;			///< Direct2Dブラシ設定
	WRL::ComPtr <ID2D1SolidColorBrush>	_shadow_brush = nullptr;		///< Direct2Dブラシ設定（影）
	WRL::ComPtr <IDWriteFactory>		_dwrite_factory = nullptr;	///< DirectWriteリソース
	WRL::ComPtr <IDWriteTextFormat>		_text_format = nullptr;		///< DirectWriteテキスト形式
	WRL::ComPtr <IDWriteTextLayout>		_text_layout = nullptr;		///< DirectWriteテキスト書式
	WRL::ComPtr <IDXGISurface>			_back_buffer = nullptr;		///< サーフェス情報
	ComPtr<ID3D11Texture2D> _d3d_text_texture = nullptr;
	ComPtr<ID3D11ShaderResourceView> _d3d_text_srv = nullptr;
	/**
	 * @brief フォントファイルリスト
	 */
	std::vector<WRL::ComPtr<IDWriteFontFile>> _font_file_list;

	/**
	 * @brief フォントデータ
	 */
	Font_Data  _setting = Font_Data();

	/**
	 * @brief フォント名リスト
	 */
	std::vector<std::wstring> _font_name_list;

	/**
	 * @brief フォントのファイル名（拡張子なし）を取得する
	 * @param filePath ファイルパス
	 * @return ファイル名（拡張子なし）
	 */
	WCHAR* get_font_file_name_without_extension(const std::wstring& filePath);

	/**
	 * @brief string型をwstring型へ変換する
	 * @param oString 変換元文字列
	 * @return 変換後のwstring
	 */
	std::wstring string_to_wstring(std::string oString);
};

class Text {
public:
	enum class TypeWriterEnding {
		Hold,       // 最後まで表示し続ける
		Loop,       // 最初からループ
		StopEmpty,  // 完全に非表示に戻る（消える）
		PingPong,   // 増えていき、最後まで行ったら逆に減る
		Chat,	   //改行するまで文章を入力→全部消す→また入力
	};
	static Font_Data text_data;

	static Text_Write* text;
	static void initialize();
	static void draw(std::string str, dx::XMFLOAT2 pos, D2D1_DRAW_TEXT_OPTIONS options, bool shadow = false);
	static void draw(std::string str, D2D1_RECT_F rect, D2D1_DRAW_TEXT_OPTIONS options, bool shadow = false);

	static std::string culculate_type_writer_string(
		const std::string& str,
		float elapsedTime,
		float charsPerSecond,
		TypeWriterEnding ending,
		bool cursor,
		float cursorBlinkSpeed
	);

	static void draw_type_writer(
		const std::string& str,
		dx::XMFLOAT2 pos,
		float elapsedTime,
		float charsPerSecond = 15,
		TypeWriterEnding ending = TypeWriterEnding::Hold,
		bool cursor = false,
		float cursorBlinkSpeed = 0.5f,
		D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE,
		bool shadow = false
	);

	static void draw_type_writer(
		const std::string& str,
		D2D1_RECT_F rect,
		float elapsedTime,
		float charsPerSecond = 15,
		TypeWriterEnding ending = TypeWriterEnding::Hold,
		bool cursor = false,
		float cursorBlinkSpeed = 0.5f,
		D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE,
		bool shadow = false
	);

};
