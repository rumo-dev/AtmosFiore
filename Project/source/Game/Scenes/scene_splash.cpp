#include "scene_splash.h"
#include <windows.h>
#include "tracy_util.h"
#include "Engine/System/graphics_core.h"
#include "Engine/System/render_state.h"
#include <algorithm>

// 初期化
void Scene_Splash::initialize()
{
	TRACY_CPU_ZONE_C("Scene_Splash::initialize", TracyCategory::System);

	log_printf("スプラッシュ画面開始\n", LogLevel::Info);
	_timer = 0.0f;
	_is_skipped = false;
	_transition_requested = false;
	_brand_logo = std::make_unique<sprite>(Graphics_Core::instance().get_device(), L"./data/AtmosFioreLogo.png");
	_directx_logo = std::make_unique<sprite>(Graphics_Core::instance().get_device(), L"./data/DirectX11Logo.webp");
	set_ready(); // アセットの非同期ロードがないため即座に準備完了
}

// 終了化
void Scene_Splash::finalize()
{
	TRACY_CPU_ZONE_C("Scene_Splash::finalize", TracyCategory::System);

	log_printf("スプラッシュ画面終了\n", LogLevel::Info);
}

// 更新処理
void Scene_Splash::update(float elapsedTime)
{
	TRACY_CPU_ZONE_C("Scene_Splash::update", TracyCategory::Update);

	_timer += elapsedTime;

	// スキップキー（SPACE または ENTER）が押された場合の処理
	if (!_transition_requested && ((GetAsyncKeyState(VK_SPACE) & 0x8000) || (GetAsyncKeyState(VK_RETURN) & 0x8000)))
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
				_transition_requested = true;
				Scene_Manager::instance().change_scene(new Scene_Title());
				return;
			}
		}
	}

	// 演出時間終了でタイトル画面へ遷移
	if (!_transition_requested && _timer >= 3.5f)
	{
		_transition_requested = true;
		Scene_Manager::instance().change_scene(new Scene_Title());
	}
}

// 描画処理
void Scene_Splash::render(float elapsedTime)
{
	TRACY_CPU_ZONE_C("Scene_Splash::render", TracyCategory::RenderPass);
	const float screenWidth = Graphics_Core::instance().get_screen_width();
	const float screenHeight = Graphics_Core::instance().get_screen_height();

	// 2つのロゴは別々の時間帯で表示する。
	float alpha = 0.0f;
	if (_timer < 0.65f)
	{
		alpha = Easing_Util::ease_out_cubic(_timer / 0.65f);
	}
	else if (_timer < 1.3f)
	{
		alpha = 1.0f;
	}
	else if (_timer < 1.65f)
	{
		alpha = 1.0f - Easing_Util::ease_in_cubic((_timer - 1.3f) / 0.35f);
	}

	// 1. 画面クリア（高級感のある深紫色の背景）
	Graphics_Core::instance().clear(Color_Utils::from_hex("#000000"));

	// 2. メインロゴ「AtmosFiore」の描画
	{
		TRACY_CPU_ZONE_N("Scene_Splash::render::MainLogo");

		ID3D11DeviceContext* context = Graphics_Core::instance().get_device_context();
		Render_State::instance().set_2d_render_states(context);
		const FLOAT tint[4] = { 1.0f, 0.88f, 0.62f, alpha };
		const float logoSize = std::min(screenWidth * 0.52f, screenHeight * 0.68f);
		_brand_logo->render(context, (screenWidth - logoSize) * 0.5f, (screenHeight - logoSize) * 0.5f,
			logoSize, logoSize, tint, 0.0f);
	}

	// DirectXロゴはAtmosFioreロゴが消えてからフェードイン。
	float sub_alpha = 0.0f;
	if (_timer >= 1.78f && _timer < 2.38f)
	{
		sub_alpha = Easing_Util::ease_out_cubic((_timer - 1.78f) / 0.60f);
	}
	else if (_timer >= 2.38f && _timer < 3.05f)
	{
		sub_alpha = 1.0f;
	}
	else if (_timer >= 3.05f && _timer < 3.5f)
	{
		sub_alpha = 1.0f - Easing_Util::ease_in_cubic((_timer - 3.05f) / 0.45f);
	}

	if (sub_alpha > 0.01f && _directx_logo && _directx_logo->get_texture_width() > 0 && _directx_logo->get_texture_height() > 0)
	{
		TRACY_CPU_ZONE_N("Scene_Splash::render::SubTitle");

		ID3D11DeviceContext* context = Graphics_Core::instance().get_device_context();
		Render_State::instance().set_2d_render_states(context);
		const FLOAT tint[4] = { 1.0f, 1.0f, 1.0f, sub_alpha };
		const float maxWidth = screenWidth * 0.72f;
		const float maxHeight = screenHeight * 0.24f;
		const float textureWidth = static_cast<float>(_directx_logo->get_texture_width());
		const float textureHeight = static_cast<float>(_directx_logo->get_texture_height());
		const float scale = std::min(maxWidth / textureWidth, maxHeight / textureHeight);
		const float drawWidth = textureWidth * scale;
		const float drawHeight = textureHeight * scale;
		_directx_logo->render(context, (screenWidth - drawWidth) * 0.5f, (screenHeight - drawHeight) * 0.5f,
			drawWidth, drawHeight, tint, 0.0f);
	}

	// 4. スキップ案内「[ SPACE ] キーでスキップ」の描画（控えめに表示）
	float prompt_alpha = _timer < 0.55f ? 0.0f : 0.38f;
	if (_timer >= 3.1f) prompt_alpha *= 1.0f - Easing_Util::ease_in_cubic(std::clamp((_timer - 3.1f) / 0.4f, 0.0f, 1.0f));

	if (prompt_alpha > 0.01f)
	{
		TRACY_CPU_ZONE_N("Scene_Splash::render::SkipPrompt");

		Text::text_data.font = Text::text->get_font_name(Font_Name::Font_rounded_x_mgenplus_1c_regular);
		Text::text_data.fontSize = std::max(14.0f, screenHeight * 0.025f);
		Text::text_data.Color = D2D1::ColorF(0.72f, 0.68f, 0.60f, prompt_alpha);
		Text::text_data.shadowColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
		Text::text_data.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
		Text::text_data.textParagraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
		Text::text->set_font(Text::text_data);

		// 画面下部に配置
		Text::draw(L"AtmosFiore を起動しています", D2D1::RectF(0, screenHeight * 0.84f, screenWidth, screenHeight * 0.96f), D2D1_DRAW_TEXT_OPTIONS_NONE, false);
	}
}

// GUI描画
void Scene_Splash::draw_gui()
{
	TRACY_CPU_ZONE_C("Scene_Splash::draw_gui", TracyCategory::UI);
}
