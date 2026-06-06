
#include "Scene_title.h"
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
	Text::draw_type_writer(
		"〇〇へ\n"
		"最近はどうしてる？\n"
		"こっちは毎日慌ただしいけど、なんとか元気に過ごしてるよ。\n"
		"昨日は久しぶりに夜更かしして映画を見たんだ。\n"
		"昔一緒に観た作品を思い出して、ちょっと懐かしい気持ちになった。\n"
		"あの頃は何も考えずに笑っていられたよね。\n"
		"今は色々と忙しいけど、あの時間がすごく大切だったんだと改めて思う。\n"
		"そういえば最近、新しいカフェを見つけたんだ。\n"
		"静かで落ち着いた雰囲気で、読書するのにぴったりだった。\n"
		"もし時間が合えば一緒に行けたらいいな。\n"
		"君が好きそうなメニューもあったから、きっと気に入ると思う。\n"
		"それから、少し前に散歩したときに夕焼けがすごく綺麗でね。\n"
		"写真を撮ろうと思ったけど、結局眺めてるだけで満足しちゃった。\n"
		"君にも見せたかったなって思ったよ。\n"
		"こういう小さな出来事を共有できる相手がいるって幸せだよね。\n"
		"最近はお互い忙しいけど、またゆっくり話せる時間を作ろう。\n"
		"近況を聞けるだけでも嬉しいし、何気ない会話が一番心地いいんだ。\n"
		"寒くなってきたから、体調には気をつけてね。\n"
		"風邪をひかないように、ちゃんと温かくして過ごしてほしい。\n"
		"また近いうちに会えることを楽しみにしてる。\n"
		"〇〇より"
		,
		D2D1::RectF(0, 0, 1280, 144),
		_timer,
		30.0f,
		Text::TypeWriterEnding::Chat,
		true,
		0.5f,
		D2D1_DRAW_TEXT_OPTIONS_NONE,
		true
	);
	Text::draw_type_writer(

		"こっちは毎日慌ただしいけど、なんとか元気に過ごしてるよ。"
		,
		D2D1::RectF(0, 144, 1280, 288),
		_timer,
		30.0f,
		Text::TypeWriterEnding::Hold,
		true,
		0.5f,
		D2D1_DRAW_TEXT_OPTIONS_NONE,
		true
	);
	Text::draw_type_writer(

		"こっちは毎日慌ただしいけど、なんとか元気に過ごしてるよ。"
		,
		D2D1::RectF(0, 288, 1280, 432),
		_timer,
		30.0f,
		Text::TypeWriterEnding::Loop,
		true,
		0.5f,
		D2D1_DRAW_TEXT_OPTIONS_NONE,
		true
	);
	Text::draw_type_writer(

		"こっちは毎日慌ただしいけど、なんとか元気に過ごしてるよ。"
		,
		D2D1::RectF(0, 432, 1280, 576),
		_timer,
		30.0f,
		Text::TypeWriterEnding::PingPong,
		true,
		0.5f,
		D2D1_DRAW_TEXT_OPTIONS_NONE,
		true
	);
	Text::draw_type_writer(
		"こっちは毎日慌ただしいけど、なんとか元気に過ごしてるよ。"
		,
		D2D1::RectF(0, 576, 1280, 720),
		_timer,
		30.0f,
		Text::TypeWriterEnding::StopEmpty,
		true,
		0.5f,
		D2D1_DRAW_TEXT_OPTIONS_NONE,
		true
	);

}

// GUI描画
void Scene_Title::draw_gui()
{
}
