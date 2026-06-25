
#include "scene.h"


static bool is_first_loading = true;
//初期化
void Scene_Loading::initialize()
{
	//スプライト初期化
	log_printf("ローディング開始\n", LogLevel::Info);
	log_printf("ローディング初期化開始\n", LogLevel::Info);
	//スレッド開始
	_thread = new std::thread(loading_thread, this);

	_timer = 0.0f;
	_loading_timer = 0.0f;
	log_printf("ローディング初期化終了\n", LogLevel::Info);
}

//終了化
void Scene_Loading::finalize()
{
	//スレッド終了化
	if (_thread != nullptr)
	{
		_thread->join();
		delete _thread;
		_thread = nullptr;
	}

	is_first_loading = false;

	log_printf("ローディング終了\n", LogLevel::Info);
}

//更新処理
void Scene_Loading::update(float elapsedTime)
{
	_loading_timer += elapsedTime;

	//次のシーンの準備が完了したらシーンを切り替える
	if (_next_scene->is_ready())
	{
		//if (is_first_loading)
		//{	//初回のみ3秒以上経過で遷移可能
		//	if (_timer > 3.f) {

		//		// 初回のみスペースキーが押されたら遷移

		//		//}
		//		if (GetAsyncKeyState(VK_SPACE) & 0x8000) {

		//			Scene_Manager::instance().change_scene(_next_scene);
		//		}
		//	}
		//}
		//else
		//{
			// 2回目以降は即遷移
		Scene_Manager::instance().change_scene(_next_scene);
		//}


		_timer += elapsedTime;
	}
}

//描画処理
void Scene_Loading::render(float elapsedTime)
{
	//elapsedTime;
	//if (is_first_loading)
	//{
	//	ID3D11DeviceContext* immediate_context = Graphics_Core::instance().get_device_context();
	//	Render_State::instance().set_depth_stencil_state(immediate_context, Depth_State::Test_Disable_Write_Disable);
	//	Render_State::instance().set_blend_state(immediate_context, Blend_State::Alpha);

	//	Graphics_Core::instance().clear(Color_Utils::from_hex("#09080F"));
	//	Text::text_data.font = Text::text->get_font_name(Font_Name::Font_karakaze_R);
	//	Text::text_data.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
	//	Text::text_data.textParagraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
	//	Text::text_data.Color = Color_Utils::hex_to_colorF("#F2D7EE");
	//	Text::text->set_font(Text::text_data);
	//	if (_timer > 0.f)
	//	{
	//		Text::text->draw_text(L"スペースキーで開始", D2D1::RectF(0, 0, 1280, 720), D2D1_DRAW_TEXT_OPTIONS_NONE);

	//	}
	//	else {


	//	}
	//}
	//else {

	//	Graphics_Core::instance().clear(Color_Utils::from_hex("#09080F"));
	//	Text::text_data.font = Text::text->get_font_name(Font_Name::Font_karakaze_R);
	//	Text::text_data.Color = Color_Utils::hex_to_colorF("#D1B8F0");
	//	Text::text->set_font(Text::text_data);

	//	Text::text->draw_text(L"ローディング", DirectX::XMFLOAT2(100, 200), D2D1_DRAW_TEXT_OPTIONS_NONE, true);

	//}
	// 「読み込み中...」の点滅・増殖アニメーション
	int dot_count = static_cast<int>(_loading_timer * 2.0f) % 4;
	std::wstring loading_text = L"読み込み中";
	for (int i = 0; i < dot_count; ++i)
	{
		loading_text += L".";
	}
	Text::text->draw_text(loading_text, D2D1::RectF(0, 0, 1280, 720), D2D1_DRAW_TEXT_OPTIONS_NONE);
}

//GUI描画
void Scene_Loading::draw_gui()
{

}

//ローディングスレッド
void Scene_Loading::loading_thread(Scene_Loading* scene)
{
	//COM関連の初期化でスレッド毎に呼ぶ必要がある
	HRESULT hr = CoInitialize(nullptr);
	if (FAILED(hr)) {
		// COM初期化失敗
		return;
	}
	if (is_first_loading) {

		Resource_Manager::instance().load_all();
	}

	//次のシーンの初期化を行う
	scene->_next_scene->initialize();

	//スレッドが終わる前にCOM関連の終了化
	CoUninitialize();

	//次のシーンの準備完了設定
	scene->_next_scene->set_ready();
}
