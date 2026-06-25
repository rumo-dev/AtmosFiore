
#include "Scene_title.h"
#include "scene_indoor.h"
#include <windows.h>

// 初期化
void Scene_Title::initialize()
{
	log_printf("タイトルシーン開始\n", LogLevel::Info);
	log_printf("タイトルシーン初期化開始\n", LogLevel::Info);
	_timer = 0.0f;

	log_printf("タイトルシーン初期化終了\n", LogLevel::Info);

}

// 終了化
void Scene_Title::finalize()
{

	log_printf("タイトルシーン終了\n", LogLevel::Info);
}

// 更新処理
void Scene_Title::update(float elapsedTime)
{

	//static float t = 0.0f;
	//ImGui::SliderFloat("T", &t, 0.0f, 1.0f);

	//static easing_util::EasingType easing = easing_util::EasingType::InOutBack;
	//easing_util::draw_easing_debug_graph(easing, t);

	//static Easing_Util::EasingType selected = Easing_Util::EasingType::InOutBack;
	//static float t = 0.5f;

	//Easing_Util::draw_easing_editor(selected, t, "test");

	//static Easing_Util::EasingType selected1 = Easing_Util::EasingType::InOutBack;
	//static float t1 = 0.5f;

	//Easing_Util::draw_easing_editor(selected1, t1, "test1");

	//static Easing_Util::EasingType selected2 = Easing_Util::EasingType::InOutBack;
	//static float t2 = 0.5f;

	//Easing_Util::draw_easing_editor(selected2, t2, "test2");

	//static Easing_Util::EasingType selected3 = Easing_Util::EasingType::InOutBack;
	//static float t3 = 0.5f;

	//Easing_Util::draw_easing_editor(selected3, t3, "test3");
	_timer += elapsedTime;

	// スプラッシュスキップ時のキー入力引き継ぎ防止のため、遷移後0.5秒間は入力を受け付けない
	if (_timer > 0.5f)
	{
		if ((GetAsyncKeyState(VK_SPACE) & 0x8000) || (GetAsyncKeyState(VK_RETURN) & 0x8000))
		{
			Scene_Manager::instance().change_scene(new Scene_Loading(new Scene_Indoor()));
		}
	}
}

// 描画処理
void Scene_Title::render(float elapsedTime)
{
	Graphics_Core::instance().clear(Color_Utils::from_hex("#1E1B26"));

	Text::text_data.font = Text::text->get_font_name(Font_Name::Font_karakaze_R);
	Text::text_data.fontSize = 40;
	Text::text_data.Color = Color_Utils::hex_to_colorF("#C9A7EB");
	Text::text_data.textAlignment = DWRITE_TEXT_ALIGNMENT::DWRITE_TEXT_ALIGNMENT_JUSTIFIED;
	Text::text->set_font(Text::text_data);
	//Text::draw("タイトル画面", D2D1::RectF(0, 0, 640, 720), D2D1_DRAW_TEXT_OPTIONS_NONE, true);
	//Text::draw("HOME\t：タイトル\n1キー\t：スプライト\n2キー\t：プリミティブ", D2D1::RectF(0, 300, 640, 720), D2D1_DRAW_TEXT_OPTIONS_NONE, true);
	Text::draw(L"Title", D2D1_RECT_F(Graphics_Core::instance().get_screen_width(), Graphics_Core::instance().get_screen_height()), D2D1_DRAW_TEXT_OPTIONS_NONE, true);

}

// GUI描画
void Scene_Title::draw_gui()
{}
