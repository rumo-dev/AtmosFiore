#include "scene_splash.h"
#include <windows.h>

// 初期化
void Scene_Splash::initialize()
{
	log_printf("スプラッシュ画面開始\n", LogLevel::Info);
	_timer = 0.0f;
	_is_skipped = false;
	set_ready(); // アセットの非同期ロードがないため即座に準備完了
}

// 終了化
void Scene_Splash::finalize()
{
	log_printf("スプラッシュ画面終了\n", LogLevel::Info);
}

// 更新処理
void Scene_Splash::update(float elapsedTime)
{
	_timer += elapsedTime;

	// スキップキー（SPACE または ENTER）が押された場合の処理
	if ((GetAsyncKeyState(VK_SPACE) & 0x8000) || (GetAsyncKeyState(VK_RETURN) & 0x8000))
	{
		if (!_is_skipped)
		{
			if (_timer < 2.5f)
			{
				_timer = 2.5f; // フェードアウト演出フェーズへジャンプ
				_is_skipped = true;
			}
			else
			{
				// すでにフェードアウト中の場合は即遷移
				Scene_Manager::instance().change_scene(new Scene_Title());
				return;
			}
		}
	}

	// 演出時間終了でタイトル画面へ遷移
	if (_timer >= 3.5f)
	{
		Scene_Manager::instance().change_scene(new Scene_Title());
	}
}

// 描画処理
void Scene_Splash::render(float elapsedTime)
{
	// 経過時間に基づいて全体の透明度 (alpha) を計算
	float alpha = 0.0f;
	if (_timer < 1.0f)
	{
		// フェードイン (0.0s ～ 1.0s)
		alpha = Easing_Util::ease_out_cubic(_timer / 1.0f);
	}
	else if (_timer < 2.5f)
	{
		// 表示維持 (1.0s ～ 2.5s)
		alpha = 1.0f;
	}
	else if (_timer < 3.5f)
	{
		// フェードアウト (2.5s ～ 3.5s)
		alpha = 1.0f - Easing_Util::ease_in_cubic((_timer - 2.5f) / 1.0f);
	}
	else
	{
		alpha = 0.0f;
	}

	// 1. 画面クリア（高級感のある深紫色の背景）
	Graphics_Core::instance().clear(Color_Utils::from_hex("#09080F"));

	// 2. メインロゴ「AtmosFiore」の描画
	Text::text_data.font = Text::text->get_font_name(Font_Name::Font_karakaze_R);
	Text::text_data.fontSize = 80;
	Text::text_data.Color = D2D1::ColorF(0.85f, 0.70f, 1.0f, alpha);
	Text::text_data.shadowColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, alpha * 0.5f);
	Text::text_data.shadowOffset = D2D1::Point2F(4.0f, -4.0f);
	Text::text_data.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
	Text::text_data.textParagraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
	Text::text->set_font(Text::text_data);

	// 中央からやや上寄りに描画
	Text::draw(L"AtmosFiore", D2D1::RectF(0, 0, 1280, 600), D2D1_DRAW_TEXT_OPTIONS_NONE, true);

	// 3. サブタイトル「A Game of Light & Shadow」の描画（少し遅れてフェードイン）
	float sub_alpha = 0.0f;
	if (_timer >= 0.5f && _timer < 1.5f)
	{
		sub_alpha = Easing_Util::ease_out_cubic((_timer - 0.5f) / 1.0f) * alpha;
	}
	else if (_timer >= 1.5f && _timer < 2.5f)
	{
		sub_alpha = alpha;
	}
	else if (_timer >= 2.5f && _timer < 3.5f)
	{
		sub_alpha = alpha;
	}

	if (sub_alpha > 0.01f)
	{
		Text::text_data.font = Text::text->get_font_name(Font_Name::Font_LightNovelPOPv2);
		Text::text_data.fontSize = 24;
		Text::text_data.Color = D2D1::ColorF(0.6f, 0.55f, 0.8f, sub_alpha);
		Text::text_data.shadowColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, sub_alpha * 0.3f);
		Text::text_data.shadowOffset = D2D1::Point2F(2.0f, -2.0f);
		Text::text_data.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
		Text::text_data.textParagraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
		Text::text->set_font(Text::text_data);

		// メインロゴの下に配置
		Text::draw(L"A Game of Light & Shadow", D2D1::RectF(0, 140, 1280, 720), D2D1_DRAW_TEXT_OPTIONS_NONE, true);
	}

	// 4. スキップ案内「[ SPACE ] キーでスキップ」の描画（控えめに表示）
	float prompt_alpha = 0.0f;
	if (_timer >= 1.0f)
	{
		prompt_alpha = 0.4f * alpha;
	}

	if (prompt_alpha > 0.01f)
	{
		Text::text_data.font = Text::text->get_font_name(Font_Name::Font_rounded_x_mgenplus_1c_regular);
		Text::text_data.fontSize = 16;
		Text::text_data.Color = D2D1::ColorF(0.5f, 0.5f, 0.6f, prompt_alpha);
		Text::text_data.shadowColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
		Text::text_data.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
		Text::text_data.textParagraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
		Text::text->set_font(Text::text_data);

		// 画面下部に配置
		Text::draw(L"[ SPACE ] キーでスキップ", D2D1::RectF(0, 600, 1280, 720), D2D1_DRAW_TEXT_OPTIONS_NONE, false);
	}
}

// GUI描画
void Scene_Splash::draw_gui()
{}
