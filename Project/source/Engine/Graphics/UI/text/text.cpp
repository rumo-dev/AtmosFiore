#pragma once



#include "text.h"
#include <sstream>
#ifdef DISABLE_DWRITE
#define USE_FONT FALSE
#else
#ifdef _DEBUG
#define USE_FONT TRUE
#else
#define USE_FONT TRUE
#endif
#endif


// フォントコレクションローダー
WRL::ComPtr <class Custom_Font_Collection_Loader> pFontCollectionLoader = nullptr;

/**
 * @brief カスタムフォントコレクションローダーの共有ポインタ。
 */
class CustomFontFileEnumerator : public IDWriteFontFileEnumerator
{
public:
	/**
 * @brief カスタムフォントファイルを列挙するクラス。
 * @details IDWriteFontFileEnumeratorを実装し、複数のフォントファイルパスを順にDirectWriteに渡す。
 */
	CustomFontFileEnumerator(IDWriteFactory* factory, const std::vector<std::wstring>& fontFilePaths)
		: refCount_(0), factory_(factory), fontFilePaths_(fontFilePaths), currentFileIndex_(-1)
	{
#if USE_FONT
		factory_->AddRef();
#endif // USE_FONT
	}
	/**
	 * @brief デストラクタ
	 */
	~CustomFontFileEnumerator()
	{
#if USE_FONT
		factory_->Release();
#endif // USE_FONT
	}
	/** @copydoc IUnknown::QueryInterface */
	IFACEMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override
	{
#if USE_FONT
		if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteFontCollectionLoader))
		{
			*ppvObject = this;
			AddRef();
			return S_OK;
		}
		else
		{
			*ppvObject = nullptr;
			return E_NOINTERFACE;
		}
#else
		* ppvObject = nullptr;
		return E_NOINTERFACE;
#endif // USE_FONT
	}
	/** @copydoc IUnknown::AddRef */
	IFACEMETHODIMP_(ULONG) AddRef() override
	{
		return InterlockedIncrement(&refCount_);
	}
	/** @copydoc IUnknown::Release */
	IFACEMETHODIMP_(ULONG) Release() override
	{
#if USE_FONT
		ULONG newCount = InterlockedDecrement(&refCount_);
		if (newCount == 0)
			delete this;

		return newCount;
#else
		return 0;
#endif // USE_FONT
	}
	/**
	 * @brief 次のフォントファイルに進む。
	 * @param[out] hasCurrentFile 次のファイルが存在するか
	 * @return HRESULT
	 */
	IFACEMETHODIMP MoveNext(OUT BOOL* hasCurrentFile) override {
#if USE_FONT
		if (++currentFileIndex_ < static_cast<int>(fontFilePaths_.size())) {
			*hasCurrentFile = TRUE;
			return S_OK;
		}
		else {
			*hasCurrentFile = FALSE;
			return S_OK;
		}
#endif // USE_FONT
		* hasCurrentFile = FALSE;
		return S_OK;
	}

	/**
	 * @brief 現在のフォントファイルを取得。
	 * @param[out] fontFile フォントファイルオブジェクト
	 * @return HRESULT
	 */
	IFACEMETHODIMP GetCurrentFontFile(OUT IDWriteFontFile** fontFile) override
	{
		// フォントファイルを読み込む
		return factory_->CreateFontFileReference(fontFilePaths_[currentFileIndex_].c_str(), nullptr, fontFile);
	}

private:
	ULONG refCount_;							///< 参照カウント
	IDWriteFactory* factory_;					///< DirectWriteファクトリ
	std::vector<std::wstring> fontFilePaths_;	///< フォントファイルのパス配列
	int currentFileIndex_;						///< 現在のファイルインデックス
};

/**
 * @brief カスタムフォントコレクションをロードするクラス。
 * @details IDWriteFontCollectionLoaderを実装し、カスタムフォント列挙子を生成する。
 */
class Custom_Font_Collection_Loader : public IDWriteFontCollectionLoader
{
public:
	/**
	 * @brief コンストラクタ
	 */
	Custom_Font_Collection_Loader() : refCount_(0) {}

	/** @copydoc IUnknown::QueryInterface */
	IFACEMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override
	{
#if USE_FONT
		if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteFontCollectionLoader))
		{
			*ppvObject = this;
			AddRef();
			return S_OK;
		}
		else
		{
			*ppvObject = nullptr;
			return E_NOINTERFACE;
		}

#endif // USE_FONT
		* ppvObject = nullptr;
		return E_NOINTERFACE;
	}
	/** @copydoc IUnknown::AddRef */
	IFACEMETHODIMP_(ULONG) AddRef() override
	{
		return InterlockedIncrement(&refCount_);
	}

	/** @copydoc IUnknown::Release */
	IFACEMETHODIMP_(ULONG) Release() override
	{
#if USE_FONT
		ULONG newCount = InterlockedDecrement(&refCount_);
		if (newCount == 0)
			delete this;

		return newCount;
#else
		return 0;
#endif // USE_FONT
	}

	/**
	 * @brief カスタムフォント列挙子を作成。
	 * @param factory DirectWriteファクトリ
	 * @param collectionKey コレクションキー（未使用）
	 * @param collectionKeySize キーサイズ
	 * @param[out] fontFileEnumerator フォントファイル列挙子
	 * @return HRESULT
	 */
	IFACEMETHODIMP CreateEnumeratorFromKey
	(
		IDWriteFactory* factory,
		void const* collectionKey,
		UINT32 collectionKeySize,
		OUT IDWriteFontFileEnumerator** fontFileEnumerator) override
	{
#if USE_FONT
		// 読み込むフォントファイルのパスを渡す
		std::vector<std::wstring> fontFilePaths(std::begin(font_list::Font_Path), std::end(font_list::Font_Path));

		// カスタムフォントファイル列挙子の作成
		*fontFileEnumerator = new (std::nothrow) CustomFontFileEnumerator(factory, fontFilePaths);

		// メモリ不足の場合はエラーを返す
		if (*fontFileEnumerator == nullptr) { return E_OUTOFMEMORY; }
#endif // USE_FONT

		return S_OK;
	}

private:
	ULONG refCount_;	///< 参照カウント
};

/**
 * @brief DirectWrite および Direct2D を初期化する。
 * @param swapChain DXGIスワップチェイン
 * @return HRESULT 成功時S_OK
 */
HRESULT Text_Write::init(IDXGISwapChain* swapChain)
{
	HRESULT result = S_OK;
#if USE_FONT
	// Direct2Dファクトリ情報の初期化
	result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, _D2D_factory.GetAddressOf());
	if (FAILED(result)) { return result; }

	// バックバッファの取得
	result = swapChain->GetBuffer(0, IID_PPV_ARGS(&_back_buffer));
	if (FAILED(result)) { return result; }

	// dpiの設定
	FLOAT dpiX = 96.0f;
	FLOAT dpiY = 96.0f;

	UINT dpi = GetDpiForWindow(Graphics_Core::instance().get_window_handle()); // hwnd は対象ウィンドウのハンドル
	dpiX = dpiY = static_cast<FLOAT>(dpi);

	// レンダーターゲットの作成
	D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), dpiX, dpiY);

	// サーフェスに描画するレンダーターゲットを作成
	result = _D2D_factory->CreateDxgiSurfaceRenderTarget(_back_buffer.Get(), &props, _render_target.GetAddressOf());
	if (FAILED(result)) { return result; }

	// アンチエイリアシングモードの設定
	_render_target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

	// IDWriteFactoryの作成
	result = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(_dwrite_factory.GetAddressOf()));
	if (FAILED(result)) { return result; }

	// カスタムフォントコレクションローダー
	pFontCollectionLoader = new Custom_Font_Collection_Loader();

	// カスタムフォントコレクションローダーの作成
	result = _dwrite_factory->RegisterFontCollectionLoader(pFontCollectionLoader.Get());
	if (FAILED(result)) { return result; }

	// フォントファイルの読み込み
	result = font_loder();
	if (FAILED(result)) { return result; }

	// フォントを設定
	result = set_font(_setting);
	if (FAILED(result)) { return result; }

#endif // USE_FONT
	return result;
}

/**
 * @brief 指定されたパスのフォントを読み込む。
 * @return HRESULT
 */
HRESULT Text_Write::font_loder()
{
	HRESULT result = S_OK;
#if USE_FONT
	// カスタムフォントコレクションの作成
	result = _dwrite_factory->CreateCustomFontCollection
	(
		pFontCollectionLoader.Get(),
		_font_file_list.data(),
		static_cast<UINT32>(_font_file_list.size()),
		&font_collection
	);
	if (FAILED(result)) { return result; }

	// フォント名を取得
	result = get_font_family_name(font_collection.Get());
	if (FAILED(result)) { return result; }

#endif // USE_FONT
	return S_OK;
}
/**
 * @brief 指定番号のフォント名を取得。
 * @param num フォント番号
 * @return フォント名（存在しない場合はnullptr）
 */
std::wstring Text_Write::get_font_name(int num)
{
#if USE_FONT
	// フォント名のリストが空だった場合
	if (_font_name_list.empty())
	{
		//return nullptr;
		return L"";
	}

	// リストのサイズを超えていた場合
	if (num >= static_cast<int>(_font_name_list.size()))
	{
		return _font_name_list[0];
	}

	return _font_name_list[num];
#else
	return L"";
#endif // USE_FONT

}

/**
 * @brief 読み込んだフォント名の数を取得。
 * @return フォント名リストの要素数
 */
int Text_Write::get_font_name_num()
{
#if USE_FONT
	return static_cast<int>(_font_name_list.size());
#else
	return 0;
#endif // USE_FONT
}

/**
 * @brief フォント設定。
 * @param set フォント設定データ構造体
 * @return HRESULT
 */
HRESULT Text_Write::set_font(Font_Data set)
{
	HRESULT result = S_OK;
#if USE_FONT
	// 設定をコピー
	_setting = set;

	//関数CreateTextFormat()
	//第1引数：フォント名（L"メイリオ", L"Arial", L"Meiryo UI"等）
	//第2引数：フォントコレクション（nullptr）
	//第3引数：フォントの太さ（DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_WEIGHT_BOLD等）
	//第4引数：フォントスタイル（DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STYLE_OBLIQUE, DWRITE_FONT_STYLE_ITALIC）
	//第5引数：フォントの幅（DWRITE_FONT_STRETCH_NORMAL,DWRITE_FONT_STRETCH_EXTRA_EXPANDED等）
	//第6引数：フォントサイズ（20, 30等）
	//第7引数：ロケール名（L""）
	//第8引数：テキストフォーマット（&g_pTextFormat）
	result = _dwrite_factory->CreateTextFormat
	(
		get_font_file_name_without_extension(_setting.font.c_str()),
		font_collection.Get(),
		_setting.fontWeight,
		_setting.fontStyle,
		_setting.fontStretch,
		_setting.fontSize,
		_setting.localeName,
		_text_format.GetAddressOf()
	);
	if (FAILED(result)) { return result; }

	//関数SetTextAlignment()
	//第1引数：テキストの配置（DWRITE_TEXT_ALIGNMENT_LEADING：前, DWRITE_TEXT_ALIGNMENT_TRAILING：後, DWRITE_TEXT_ALIGNMENT_CENTER：中央,
	//                         DWRITE_TEXT_ALIGNMENT_JUSTIFIED：行いっぱい）
	result = _text_format->SetTextAlignment(_setting.textAlignment);
	//関数SetParagraphAlignment()
	//第1引数：テキストの配置（DWRITE_PARAGRAPH_ALIGNMENT_NEAR：上, DWRITE_PARAGRAPH_ALIGNMENT_FAR：下, DWRITE_PARAGRAPH_ALIGNMENT_CENTER：中央）
	result = _text_format->SetParagraphAlignment(_setting.textParagraphAlignment);
	if (FAILED(result)) { return result; }

	//関数CreateSolidColorBrush()
	//第1引数：フォント色（D2D1::ColorF(D2D1::ColorF::Black)：黒, D2D1::ColorF(D2D1::ColorF(0.0f, 0.2f, 0.9f, 1.0f))：RGBA指定）
	result = _render_target->CreateSolidColorBrush(_setting.Color, _brush.GetAddressOf());
	if (FAILED(result)) { return result; }

	// 影用のブラシを作成
	result = _render_target->CreateSolidColorBrush(_setting.shadowColor, _shadow_brush.GetAddressOf());
	if (FAILED(result)) { return result; }

#endif // USE_FONT
	return result;
}

/**
 * @brief フォント設定（個別パラメータ指定版）。
 * @param fontname フォント名
 * @param fontWeight 太さ
 * @param fontStyle スタイル
 * @param fontStretch 幅
 * @param fontSize フォントサイズ
 * @param localeName ロケール名
 * @param textAlignment テキスト配置
 * @param textParagraphAlignment 段落配置
 * @param Color フォント色
 * @param shadowColor 影色
 * @param shadowOffset 影オフセット
 * @return HRESULT
 */
HRESULT Text_Write::set_font(WCHAR const* fontname, DWRITE_FONT_WEIGHT fontWeight, DWRITE_FONT_STYLE fontStyle,
	DWRITE_FONT_STRETCH fontStretch, FLOAT fontSize, WCHAR const* localeName,
	DWRITE_TEXT_ALIGNMENT textAlignment, DWRITE_PARAGRAPH_ALIGNMENT textParagraphAlignment, D2D1_COLOR_F Color, D2D1_COLOR_F shadowColor, D2D1_POINT_2F shadowOffset)
{
	HRESULT result = S_OK;
#if USE_FONT
	_dwrite_factory->CreateTextFormat(get_font_file_name_without_extension(fontname), font_collection.Get(), fontWeight, fontStyle, fontStretch, fontSize, localeName, &_text_format);
	if (FAILED(result)) { return result; }

	_text_format->SetTextAlignment(textAlignment);
	_text_format->SetParagraphAlignment(textParagraphAlignment);
	if (FAILED(result)) { return result; }

	_render_target->CreateSolidColorBrush(Color, _brush.GetAddressOf());
	if (FAILED(result)) { return result; }

	_render_target->CreateSolidColorBrush(shadowColor, _shadow_brush.GetAddressOf());
	if (FAILED(result)) { return result; }

#endif // USE_FONT
	return result;
}

/**
 * @brief 文字を描画する（座標指定）。
 * @param str 描画文字列
 * @param pos 描画位置
 * @param options テキストオプション
 * @param shadow 影を描画するか
 * @return HRESULT
 */
HRESULT Text_Write::draw_text(std::string str, dx::XMFLOAT2 pos, D2D1_DRAW_TEXT_OPTIONS options, bool shadow)
{
	if (!_render_target) {
		return E_FAIL; // または初期化処理を再試行
	}
	HRESULT result = S_OK;
#if USE_FONT
	// 文字列の変換
	std::wstring wstr = string_to_wstring(str.c_str());

	// ターゲットサイズの取得
	D2D1_SIZE_F TargetSize = _render_target->GetSize();


	// テキストレイアウトを作成
	result = _dwrite_factory->CreateTextLayout(wstr.c_str(), static_cast<UINT32>(wstr.size()), _text_format.Get(), TargetSize.width, TargetSize.height, _text_layout.GetAddressOf());
	if (FAILED(result)) { return result; }

	// 描画位置の確定
	D2D1_POINT_2F pounts;
	pounts.x = pos.x;
	pounts.y = pos.y;

	// 描画の開始
	_render_target->BeginDraw();

	// 影を描画する場合
	if (shadow)
	{
		// 影の描画
		_render_target->DrawTextLayout(D2D1::Point2F(pounts.x - _setting.shadowOffset.x, pounts.y - _setting.shadowOffset.y),
			_text_layout.Get(),
			_shadow_brush.Get(),
			options);
	}

	// 描画処理
	_render_target->DrawTextLayout(pounts, _text_layout.Get(), _brush.Get(), options);

	// 描画の終了
	result = _render_target->EndDraw();
	if (FAILED(result)) { return result; }

#endif // USE_FONT
	return S_OK;
}

/**
 * @brief 文字を描画する（矩形指定）。
 * @param str 描画文字列
 * @param rect 描画領域
 * @param options テキストオプション
 * @param shadow 影を描画するか
 * @return HRESULT
 */
HRESULT Text_Write::draw_text(std::string str, D2D1_RECT_F rect, D2D1_DRAW_TEXT_OPTIONS options, bool shadow)
{


	HRESULT result = S_OK;
#if USE_FONT
	// 文字列の変換
	std::wstring wstr = string_to_wstring(str.c_str());

	// 描画の開始
	_render_target->BeginDraw();

	if (shadow)
	{
		// 影の描画
		_render_target->DrawText(wstr.c_str(),
			static_cast<UINT32>(wstr.size()),
			_text_format.Get(),
			D2D1::RectF(rect.left - _setting.shadowOffset.x, rect.top - _setting.shadowOffset.y, rect.right - _setting.shadowOffset.x, rect.bottom - _setting.shadowOffset.y),
			_shadow_brush.Get(), options);
	}

	// 描画処理
	_render_target->DrawText(wstr.c_str(), static_cast<UINT32>(wstr.size()), _text_format.Get(), rect, _brush.Get(), options);

	// 描画の終了
	result = _render_target->EndDraw();
	if (FAILED(result)) { return result; }

#endif // USE_FONT

	return S_OK;
}


/**
 * @brief カスタムフォントコレクションからフォントファミリー名を取得。
 * @param customFontCollection カスタムフォントコレクション
 * @param locale ロケール名（例：L"ja-jp"）
 * @return HRESULT
 */
HRESULT Text_Write::get_font_family_name(IDWriteFontCollection* customFontCollection, const WCHAR* locale)
{
	HRESULT result = S_OK;
#if USE_FONT
	// フォントファミリー名一覧をリセット
	std::vector<std::wstring>().swap(_font_name_list);

	// フォントの数を取得
	UINT32 familyCount = customFontCollection->GetFontFamilyCount();

	for (UINT32 i = 0; i < familyCount; i++)
	{
		// フォントファミリーの取得
		WRL::ComPtr <IDWriteFontFamily> fontFamily = nullptr;
		result = customFontCollection->GetFontFamily(i, fontFamily.GetAddressOf());
		if (FAILED(result)) { return result; }

		// フォントファミリー名の一覧を取得
		WRL::ComPtr <IDWriteLocalizedStrings> familyNames = nullptr;
		result = fontFamily->GetFamilyNames(familyNames.GetAddressOf());
		if (FAILED(result)) { return result; }

		// 指定されたロケールに対応するインデックスを検索
		UINT32 index = 0;
		BOOL exists = FALSE;
		result = familyNames->FindLocaleName(locale, &index, &exists);
		if (FAILED(result)) { return result; }

		// 指定されたロケールが見つからなかった場合は、デフォルトのロケールを使用
		if (!exists)
		{
			if (FAILED(result)) { return result; }
			result = familyNames->FindLocaleName(L"en-us", &index, &exists);
		}

		// フォントファミリー名の長さを取得
		UINT32 length = 0;
		result = familyNames->GetStringLength(index, &length);
		if (FAILED(result)) { return result; }

		// フォントファミリー名の取得
		WCHAR* name = new WCHAR[length + 1];
		result = familyNames->GetString(index, name, length + 1);
		if (FAILED(result)) { return result; }

		// フォントファミリー名を追加
		_font_name_list.push_back(name);

		// フォントファミリー名の破棄
		delete[] name;
	}
#endif // USE_FONT

	return result;
}

/**
 * @brief すべてのフォントファミリー名を取得。
 * @param customFontCollection カスタムフォントコレクション
 * @return HRESULT
 */
HRESULT Text_Write::get_all_font_family_names(IDWriteFontCollection* customFontCollection)
{
	HRESULT result = S_OK;
#if USE_FONT
	// フォントファミリー名一覧をリセット
	std::vector<std::wstring>().swap(_font_name_list);

	// フォントファミリーの数を取得
	UINT32 familyCount = customFontCollection->GetFontFamilyCount();

	for (UINT32 i = 0; i < familyCount; i++)
	{
		// フォントファミリーの取得
		WRL::ComPtr <IDWriteFontFamily> fontFamily = nullptr;
		result = customFontCollection->GetFontFamily(i, fontFamily.GetAddressOf());
		if (FAILED(result)) { return result; }

		// フォントファミリー名の一覧を取得
		WRL::ComPtr <IDWriteLocalizedStrings> familyNames = nullptr;
		result = fontFamily->GetFamilyNames(familyNames.GetAddressOf());
		if (FAILED(result)) { return result; }

		// フォントファミリー名の数を取得
		UINT32 nameCount = familyNames->GetCount();

		// フォントファミリー名の数だけ繰り返す
		for (UINT32 j = 0; j < nameCount; ++j)
		{
			// フォントファミリー名の長さを取得
			UINT32 length = 0;
			result = familyNames->GetStringLength(j, &length);
			if (FAILED(result)) { return result; }

			// フォントファミリー名の取得
			WCHAR* name = new WCHAR[length + 1];
			result = familyNames->GetString(j, name, length + 1);
			if (FAILED(result)) { return result; }

			// フォントファミリー名を追加
			_font_name_list.push_back(name);

			// フォントファミリー名の破棄
			delete[] name;
		}
	}

#endif // USE_FONT
	return result;
}

/**
 * @brief フォントファイルパスから拡張子を除いたファイル名を取得。
 * @param filePath フォントファイルのフルパス
 * @return 拡張子を除いたWCHARポインタ（呼び出し側でdelete[]が必要）
 */
WCHAR* Text_Write::get_font_file_name_without_extension(const std::wstring& filePath)
{
#if USE_FONT
	// 末尾から検索してファイル名と拡張子の位置を取得
	size_t start = filePath.find_last_of(L"/\\") + 1;
	size_t end = filePath.find_last_of(L'.');

	// ファイル名を取得
	std::wstring fileNameWithoutExtension = filePath.substr(start, end - start).c_str();

	// 新しいWCHAR配列を作成
	WCHAR* fileName = new WCHAR[fileNameWithoutExtension.length() + 1];

	// 文字列をコピー
	wcscpy_s(fileName, fileNameWithoutExtension.length() + 1, fileNameWithoutExtension.c_str());

	// ファイル名を返す
	return fileName;
#else
	return nullptr;
#endif // USE_FONT
}
/**
 * @brief std::stringをstd::wstringに変換。
 * @param oString 変換対象の文字列（SJIS）
 * @return 変換後のワイド文字列
 */
std::wstring Text_Write::string_to_wstring(std::string oString)
{
#if USE_FONT
	// SJIS → wstring
	int iBufferSize = MultiByteToWideChar(CP_ACP, 0, oString.c_str(), -1, (wchar_t*)NULL, 0);

	// バッファの取得
	wchar_t* cpUCS2 = new wchar_t[iBufferSize];

	// SJIS → wstring
	MultiByteToWideChar(CP_ACP, 0, oString.c_str(), -1, cpUCS2, iBufferSize);

	// stringの生成
	std::wstring oRet(cpUCS2, cpUCS2 + iBufferSize - 1);

	// バッファの破棄
	delete[] cpUCS2;

	// 変換結果を返す
	return(oRet);
#else
	return L"";
#endif // USE_FONT
}
Text_Write* Text::text = nullptr;
Font_Data Text::text_data;
void Text::initialize()
{
	text = new Text_Write(&text_data);
	text->init(Graphics_Core::instance().get_swap_chain());
	text->get_font_family_name(text->font_collection.Get());

	text_data.fontSize = 60;
	text_data.Color = D2D1::ColorF(D2D1::ColorF::White);
	text_data.shadowColor = D2D1::ColorF(D2D1::ColorF::Black);
	text_data.font = text->get_font_name(Font_Name::Font_karakaze_R);
	text->set_font(text_data);


}
void Text::draw(std::string str, dx::XMFLOAT2 pos, D2D1_DRAW_TEXT_OPTIONS options, bool shadow)
{
	if (text)
	{
		text->draw_text(str, pos, options, shadow);
	}
}
void Text::draw(std::string str, D2D1_RECT_F rect, D2D1_DRAW_TEXT_OPTIONS options, bool shadow)
{
	if (text)
	{
		text->draw_text(str, rect, options, shadow);
	}
}
void Text::draw_type_writer(
	const std::string& str,
	dx::XMFLOAT2 pos,
	float elapsedTime,
	float charsPerSecond,
	TypeWriterEnding ending,
	bool cursor,
	float cursorBlinkSpeed,
	D2D1_DRAW_TEXT_OPTIONS options,
	bool shadow
)
{
	draw(culculate_type_writer_string(str, elapsedTime, charsPerSecond, ending, cursor, cursorBlinkSpeed), pos, options, shadow);
}
void Text::draw_type_writer(
	const std::string& str,
	D2D1_RECT_F rect,
	float elapsedTime,
	float charsPerSecond,
	TypeWriterEnding ending,
	bool cursor,
	float cursorBlinkSpeed,
	D2D1_DRAW_TEXT_OPTIONS options,
	bool shadow
)
{
	draw(culculate_type_writer_string(str, elapsedTime, charsPerSecond, ending, cursor, cursorBlinkSpeed), rect, options, shadow);
}
std::string Text::culculate_type_writer_string(
	const std::string& str,
	float elapsedTime,
	float charsPerSecond,
	TypeWriterEnding ending,
	bool cursor,
	float cursorBlinkSpeed
) {
	const size_t length = str.size();
	float totalTime = length / charsPerSecond;

	size_t maxChars = 0;
	std::string sub;

	switch (ending)
	{
	case TypeWriterEnding::Hold:
		maxChars = (size_t)(elapsedTime * charsPerSecond);
		if (maxChars > length) maxChars = length;
		sub = str.substr(0, maxChars);
		break;

	case TypeWriterEnding::Loop:
	{
		float t = fmod(elapsedTime, totalTime);
		maxChars = (size_t)(t * charsPerSecond);
		sub = str.substr(0, maxChars);
	}
	break;

	case TypeWriterEnding::StopEmpty:
	{
		float t = fmod(elapsedTime, totalTime * 2);
		if (t < totalTime)
			maxChars = (size_t)(t * charsPerSecond);
		else
			maxChars = 0;
		sub = str.substr(0, maxChars);
	}
	break;

	case TypeWriterEnding::PingPong:
	{
		float t = fmod(elapsedTime, totalTime * 2);
		if (t < totalTime)
			maxChars = (size_t)(t * charsPerSecond);
		else
			maxChars = length - (size_t)((t - totalTime) * charsPerSecond);

		if (maxChars > length) maxChars = length;
		if ((int)maxChars < 0) maxChars = 0;
		sub = str.substr(0, maxChars);
	}
	break;


	case TypeWriterEnding::Chat:
	{
		// 改行で分割
		std::vector<std::string> lines;
		{
			std::stringstream ss(str);
			std::string line;
			while (std::getline(ss, line)) {
				lines.push_back(line);
			}
		}

		float cycleTime = 0.0f;
		std::vector<float> lineStartTime;
		for (size_t i = 0; i < lines.size(); i++) {
			lineStartTime.push_back(cycleTime);
			float typeTime = lines[i].size() / charsPerSecond;
			float holdTime = 0.5f;
			float eraseTime = (i == lines.size() - 1) ? 0.0f : (lines[i].size() * 0.5f) / charsPerSecond;
			cycleTime += typeTime + holdTime + eraseTime;
		}

		size_t currentLine = lines.size() - 1;
		for (size_t i = 0; i < lines.size(); i++) {
			float typeTime = lines[i].size() / charsPerSecond;
			float holdTime = 0.5f;
			float eraseTime = (i == lines.size() - 1) ? 0.0f : (lines[i].size() * 0.5f) / charsPerSecond;
			if (elapsedTime < lineStartTime[i] + typeTime + holdTime + eraseTime) {
				currentLine = i;
				break;
			}
		}

		float localTime = elapsedTime - lineStartTime[currentLine];
		size_t lineLength = lines[currentLine].size();

		float typeTime = lineLength / charsPerSecond;
		float holdTime = 0.5f;

		if (currentLine == lines.size() - 1) {
			maxChars = (size_t)(localTime * charsPerSecond);
			if (maxChars > lineLength) maxChars = lineLength;
			sub = lines[currentLine].substr(0, maxChars);
		}
		else {
			if (localTime < typeTime) {
				maxChars = (size_t)(localTime * charsPerSecond);
				sub = lines[currentLine].substr(0, maxChars);
			}
			else if (localTime < typeTime + holdTime) {
				maxChars = lineLength;
				sub = lines[currentLine];
			}
			else {
				float eraseTime = localTime - (typeTime + holdTime);
				size_t remaining = lineLength - (size_t)(eraseTime * charsPerSecond * 2.0f);
				if ((int)remaining < 0) remaining = 0;
				sub = lines[currentLine].substr(0, remaining);
			}
		}
	}
	break;
	break;


	}

	// --- カーソル処理 ---
	if (cursor) {
		bool showCursor = fmod(elapsedTime, cursorBlinkSpeed * 2) < cursorBlinkSpeed;

		if (showCursor || maxChars < length) {
			sub += "|";
		}
	}

	return sub;
}

