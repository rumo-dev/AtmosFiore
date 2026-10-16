#include "scene.h"
#include "tracy_util.h"
#include <algorithm>
#include <cmath>
#include "Engine/System/graphics_core.h"

static bool is_first_loading = true;

//初期化
void Scene_Loading::initialize()
{
	TRACY_CPU_ZONE_C("Scene_Loading::initialize", TracyCategory::System);

	//スプライト初期化
	log_printf("ローディング開始\n", LogLevel::Info);
	log_printf("ローディング初期化開始\n", LogLevel::Info);
	//スレッド開始
	_thread = new std::thread(loading_thread, this);
	_timer = 0.0f;
	_loading_timer = 0.0f;
	_transition_requested = false;
	log_printf("ローディング初期化終了\n", LogLevel::Info);
}

//終了化
void Scene_Loading::finalize()
{
	TRACY_CPU_ZONE_C("Scene_Loading::finalize", TracyCategory::System);

	//スレッド終了化
	if (_thread != nullptr)
	{
		TRACY_CPU_ZONE_N("Scene_Loading::finalize::ThreadJoin");
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
	TRACY_CPU_ZONE_C("Scene_Loading::update", TracyCategory::Update);

	_loading_timer += elapsedTime;
	//次のシーンの準備が完了したらシーンを切り替える
	if (!_transition_requested && _next_scene->is_ready())
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
		_transition_requested = true;
		//}
		_timer += elapsedTime;
	}
}

//描画処理
void Scene_Loading::render(float elapsedTime)
{
	TRACY_CPU_ZONE_C("Scene_Loading::render", TracyCategory::RenderPass);
	const float width = Graphics_Core::instance().get_screen_width();
	const float height = Graphics_Core::instance().get_screen_height();
	if (!_background_effect.is_initialized())
		_background_effect.initialize(Graphics_Core::instance().get_device(), "loading_background_ps.cso", 16);
	struct alignas(16) LoadingConstants { float resolution[2]; float time; float padding; } constants{};
	constants.resolution[0] = width;
	constants.resolution[1] = height;
	constants.time = _loading_timer;
	_background_effect.draw(&constants, sizeof(constants));

	// 画面下寄りに置き、フォントサイズと領域を実際の解像度に合わせる。
	int dot_count = static_cast<int>(_loading_timer * 2.0f) % 4;
	std::wstring loading_text = L"読み込み中";
	for (int i = 0; i < dot_count; ++i)
	{
		loading_text += L".";
	}
	Text::text_data.font = Text::text->get_font_name(Font_Name::Font_karakaze_R);
	Text::text_data.fontSize = std::max(16.0f, std::min(width * 0.045f, height * 0.055f));
	Text::text_data.Color = D2D1::ColorF(0.92f, 0.90f, 0.82f, 0.95f);
	Text::text_data.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
	Text::text_data.textParagraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
	Text::text->set_font(Text::text_data);
	Text::text->draw_text(loading_text, D2D1::RectF(width * 0.08f, height * 0.78f, width * 0.92f, height * 0.91f), D2D1_DRAW_TEXT_OPTIONS_NONE);
}

//GUI描画
void Scene_Loading::draw_gui()
{
	TRACY_CPU_ZONE_C("Scene_Loading::draw_gui", TracyCategory::UI);
}

//ローディングスレッド
void Scene_Loading::loading_thread(Scene_Loading* scene)
{
	TRACY_CPU_ZONE_C("Scene_Loading::loading_thread", TracyCategory::System);

	//COM関連の初期化でスレッド毎に呼ぶ必要がある
	HRESULT hr = CoInitialize(nullptr);
	if (FAILED(hr)) {
		// COM初期化失敗
		return;
	}
	if (is_first_loading) {
		TRACY_CPU_ZONE_N("Scene_Loading::loading_thread::LoadAllResources");
		Resource_Manager::instance().load_all();
	}
	//次のシーンの初期化を行う
	{
		TRACY_CPU_ZONE_N("Scene_Loading::loading_thread::NextSceneInitialize");
		scene->_next_scene->initialize();
	}
	//スレッドが終わる前にCOM関連の終了化
	CoUninitialize();
	//次のシーンの準備完了設定
	scene->_next_scene->set_ready();
}
