#include <DxLib.h>
#include <cmath>
#include <cstdio>
#include <string>
#include "../Application.h"
#include "../Manager/SceneManager.h"
#include "../Manager/Camera.h"
#include "../Manager/ResourceManager.h"
#include "../Manager/InputManager.h"
#include "../Manager/PadInput.h"
#include "../Manager/GameProgress.h"
#include "../Manager/StageData.h"
#include "StageSelectScene.h"

namespace
{
	const char* const kFontName = "Meiryo UI";

	// --- 配置(画像と同じ 1920x1080 の画面での座標) ---
	constexpr float kCardX = 120.0f;
	constexpr float kCardTop = 300.0f;
	constexpr float kCardStep = 200.0f;		// カードの縦の間隔
	constexpr float kCardW = 880.0f;
	constexpr float kCardH = 170.0f;
	constexpr float kImageMargin = 20.0f;	// カード画像の、光の余白
	constexpr float kSelectedSlide = 30.0f;	// 選んでいるカードが右へずれる量

	constexpr float kBackX = 120.0f;
	constexpr float kBackY = 920.0f;
	constexpr float kBackW = 500.0f;
	constexpr float kBackH = 80.0f;

	constexpr float kStatusX = 1080.0f;
	constexpr float kStatusY = 40.0f;
	constexpr float kStatusW = 700.0f;

	constexpr float kDetailX = 1080.0f;
	constexpr float kDetailY = 300.0f;
	constexpr float kDetailW = 700.0f;

	// --- 色(GetColor は DxLib の初期化後でないと正しい値を返さないので、関数にしてある) ---
	unsigned int kOrange(void) { return GetColor(255, 194, 51); }
	unsigned int kWhite(void) { return GetColor(255, 255, 255); }
	unsigned int kGray(void) { return GetColor(160, 166, 190); }
	unsigned int kDark(void) { return GetColor(110, 116, 140); }
	unsigned int kGreen(void) { return GetColor(130, 230, 150); }
	unsigned int kBlack(void) { return GetColor(0, 0, 0); }
}

StageSelectScene::StageSelectScene(void)
	:
	scaleX_(1.0f),
	scaleY_(1.0f),
	cursor_(0),
	guardTimer_(0.0f),
	animTimer_(0.0f),
	prevStickDir_(2),
	isDecided_(false),
	cardOffset_{},
	imgBg_(-1),
	imgCardNormal_(-1),
	imgCardSelected_(-1),
	imgCardLocked_(-1),
	imgButtonNormal_(-1),
	imgButtonSelected_(-1),
	imgPanelStatus_(-1),
	imgPanelDetail_(-1),
	fontLevel_(-1),
	fontBig_(-1),
	fontMid_(-1),
	fontSmall_(-1)
{
}

StageSelectScene::~StageSelectScene(void)
{
	// 画像は ResourceManager が解放するので、ここでは消さない

	if (fontLevel_ != -1) { DeleteFontToHandle(fontLevel_); }
	if (fontBig_ != -1) { DeleteFontToHandle(fontBig_); }
	if (fontMid_ != -1) { DeleteFontToHandle(fontMid_); }
	if (fontSmall_ != -1) { DeleteFontToHandle(fontSmall_); }
}

void StageSelectScene::Init(void)
{
	// 前のシーン(戦闘)で追っていた対象は、もうないので外しておく
	mainCamera.SetLockOnTarget(nullptr);
	mainCamera.ChangeMode(Camera::MODE::FIXED_POINT);

	scaleX_ = static_cast<float>(Application::SCREEN_SIZE_X) / 1920.0f;
	scaleY_ = static_cast<float>(Application::SCREEN_SIZE_Y) / 1080.0f;

	LoadImages();

	const int type = DX_FONTTYPE_ANTIALIASING_EDGE_4X4;

	fontLevel_ = CreateFontToHandle(kFontName, static_cast<int>(96.0f * scaleY_), 6, type, -1, 3);
	fontBig_ = CreateFontToHandle(kFontName, static_cast<int>(60.0f * scaleY_), 5, type, -1, 3);
	fontMid_ = CreateFontToHandle(kFontName, static_cast<int>(42.0f * scaleY_), 4, type, -1, 2);
	fontSmall_ = CreateFontToHandle(kFontName, static_cast<int>(30.0f * scaleY_), 3, type, -1, 2);

	// 最後に遊んだステージにカーソルを置く
	cursor_ = GameProgress::Get().selectedStage;

	if (cursor_ < 0 || cursor_ >= GameProgress::STAGE_MAX || !GameProgress::IsUnlocked(cursor_))
	{
		cursor_ = 0;
	}

	for (int i = 0; i < ITEM_COUNT; i++)
	{
		cardOffset_[i] = 0.0f;
	}

	guardTimer_ = INPUT_GUARD_TIME;
	animTimer_ = 0.0f;
	isDecided_ = false;

	// 開いた時にスティックが倒れていても、すぐには動かさない
	prevStickDir_ = 2;
}

void StageSelectScene::LoadImages(void)
{
	ResourceManager& res = ResourceManager::GetInstance();

	imgBg_ = res.Load(ResourceManager::SRC::STAGE_BG).handleId_;
	imgCardNormal_ = res.Load(ResourceManager::SRC::STAGE_CARD_NORMAL).handleId_;
	imgCardSelected_ = res.Load(ResourceManager::SRC::STAGE_CARD_SELECTED).handleId_;
	imgCardLocked_ = res.Load(ResourceManager::SRC::STAGE_CARD_LOCKED).handleId_;
	imgButtonNormal_ = res.Load(ResourceManager::SRC::STAGE_BUTTON_NORMAL).handleId_;
	imgButtonSelected_ = res.Load(ResourceManager::SRC::STAGE_BUTTON_SELECTED).handleId_;
	imgPanelStatus_ = res.Load(ResourceManager::SRC::STAGE_PANEL_STATUS).handleId_;
	imgPanelDetail_ = res.Load(ResourceManager::SRC::STAGE_PANEL_DETAIL).handleId_;
}

//------------------------------------------------------------
// 更新
//------------------------------------------------------------
void StageSelectScene::Update(void)
{
	// プレイヤーが更新されないので、パッド入力を自分で更新する
	PadInput::Update();

	const float dt = SceneManager::GetInstance().GetDeltaTime();

	animTimer_ += dt;

	// 選んでいるカードだけ、右へなめらかにずらす
	for (int i = 0; i < ITEM_COUNT; i++)
	{
		const float goal = (i == cursor_) ? kSelectedSlide : 0.0f;
		float rate = 14.0f * dt;

		if (rate > 1.0f)
		{
			rate = 1.0f;
		}

		cardOffset_[i] += (goal - cardOffset_[i]) * rate;
	}

	if (isDecided_)
	{
		return;
	}

	// カーソルは、待ち時間の間も動かせる
	const int move = ReadMoveInput();

	if (move != 0)
	{
		cursor_ = (cursor_ + move + ITEM_COUNT) % ITEM_COUNT;
	}

	// 決定・戻るは、開いた直後の待ち時間が終わってから受け付ける
	if (guardTimer_ > 0.0f)
	{
		guardTimer_ -= dt;
		return;
	}

	if (IsDecidePressed())
	{
		Decide(cursor_);
		return;
	}

	if (IsCancelPressed())
	{
		Decide(GameProgress::STAGE_MAX);
	}
}

int StageSelectScene::ReadMoveInput(void)
{
	InputManager& ins = InputManager::GetInstance();

	// スティックは「倒した瞬間」だけ反応させたいので、前フレームとの差で見る
	int stickDir = 0;

	if (PadInput::StickY() > STICK_THRESHOLD) { stickDir = 1; }
	if (PadInput::StickY() < -STICK_THRESHOLD) { stickDir = -1; }

	if (prevStickDir_ == 2)
	{
		prevStickDir_ = stickDir;
	}

	const bool stickTrg = (stickDir != 0 && stickDir != prevStickDir_);
	prevStickDir_ = stickDir;

	const bool up =
		ins.IsTrgDown(KEY_INPUT_UP) ||
		ins.IsTrgDown(KEY_INPUT_W) ||
		(stickTrg && stickDir == 1);

	const bool down =
		ins.IsTrgDown(KEY_INPUT_DOWN) ||
		ins.IsTrgDown(KEY_INPUT_S) ||
		(stickTrg && stickDir == -1);

	return (down ? 1 : 0) - (up ? 1 : 0);
}

bool StageSelectScene::IsDecidePressed(void) const
{
	InputManager& ins = InputManager::GetInstance();

	return
		ins.IsTrgDown(KEY_INPUT_RETURN) ||
		ins.IsTrgDown(KEY_INPUT_SPACE) ||
		PadInput::IsTrg(PadInput::BTN::CIRCLE);
}

bool StageSelectScene::IsCancelPressed(void) const
{
	InputManager& ins = InputManager::GetInstance();

	return
		ins.IsTrgDown(KEY_INPUT_ESCAPE) ||
		ins.IsTrgDown(KEY_INPUT_BACK) ||
		PadInput::IsTrg(PadInput::BTN::CROSS);
}

void StageSelectScene::Decide(int index)
{
	// 一番下は「タイトルへ戻る」
	if (index == GameProgress::STAGE_MAX)
	{
		isDecided_ = true;
		SceneManager::GetInstance().ChangeScene(SceneManager::SCENE_ID::TITLE);
		return;
	}

	// まだ選べないステージは、何も起きない
	if (!GameProgress::IsUnlocked(index))
	{
		return;
	}

	GameProgress::Get().selectedStage = index;

	isDecided_ = true;
	SceneManager::GetInstance().ChangeScene(SceneManager::SCENE_ID::GAME);
}

//------------------------------------------------------------
// 描画の部品
//------------------------------------------------------------

// 画像を、元の大きさのまま(画面の大きさに合わせて拡縮して)描く
void StageSelectScene::DrawImageAt(int handle, float x, float y) const
{
	if (handle == -1)
	{
		return;
	}

	int w = 0;
	int h = 0;
	GetGraphSize(handle, &w, &h);

	DrawExtendGraphF(
		x * scaleX_, y * scaleY_,
		(x + static_cast<float>(w)) * scaleX_, (y + static_cast<float>(h)) * scaleY_,
		handle, TRUE);
}

// 文字を描く(align: 0=左 1=中央 2=右)
void StageSelectScene::DrawLabel(float x, float y, const std::string& text, unsigned int color, int font, int align) const
{
	const int w = GetDrawStringWidthToHandle(text.c_str(), static_cast<int>(text.size()), font);

	int px = static_cast<int>(x * scaleX_);

	if (align == 1) { px -= w / 2; }
	if (align == 2) { px -= w; }

	DrawStringToHandle(px, static_cast<int>(y * scaleY_), text.c_str(), color, font, kBlack());
}

//------------------------------------------------------------
// 描画
//------------------------------------------------------------
void StageSelectScene::Draw(void)
{
	// 背景(画面いっぱいに)
	if (imgBg_ != -1)
	{
		DrawExtendGraph(0, 0, Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y, imgBg_, TRUE);
	}
	else
	{
		DrawBox(0, 0, Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y, GetColor(8, 10, 30), TRUE);

		// 画像が見つからないときは、知らせる
		DrawFormatString(20, 20, GetColor(255, 80, 80),
			"画像を読み込めません: Image/StageSelect/bg.png などの場所と、ResourceManager の登録を確認してください");
	}

	DrawStatusPanel();

	for (int i = 0; i < GameProgress::STAGE_MAX; i++)
	{
		DrawStageCard(i);
	}

	DrawBackCard();
	DrawDetailPanel();
	DrawHint();
}

// 左: ステージのカード
void StageSelectScene::DrawStageCard(int index) const
{
	const bool isSelected = (index == cursor_);
	const bool isUnlocked = GameProgress::IsUnlocked(index);

	const float x = kCardX + cardOffset_[index];
	const float y = kCardTop + kCardStep * static_cast<float>(index);

	// 画像(光の余白の分だけ、左上へずらして描く)
	int img = imgCardNormal_;

	if (!isUnlocked)
	{
		img = imgCardLocked_;
	}
	else if (isSelected)
	{
		img = imgCardSelected_;
	}

	DrawImageAt(img, x - kImageMargin, y - kImageMargin);

	char buf[64];
	snprintf(buf, sizeof(buf), "STAGE %d", index + 1);

	const float tx = x + 96.0f;
	const float rx = x + kCardW - 150.0f;

	// ロック中: 名前は出さない
	if (!isUnlocked)
	{
		DrawLabel(tx, y + 24.0f, buf, kDark(), fontSmall_, 0);
		DrawLabel(tx, y + 68.0f, "? ? ?", kDark(), fontBig_, 0);
		return;
	}

	const StageData::Info& info = StageData::Get(index);

	DrawLabel(tx, y + 24.0f, buf, kOrange(), fontSmall_, 0);
	DrawLabel(tx, y + 68.0f, info.name, kWhite(), fontBig_, 0);

	// 右上: クリア済み
	if (GameProgress::Get().cleared[index])
	{
		DrawLabel(rx, y + 30.0f, "CLEAR", kGreen(), fontSmall_, 2);
	}

	// 右下: 推奨レベル(足りないときはオレンジ)
	const bool isShort = (GameProgress::Get().level < info.recommendLv);

	snprintf(buf, sizeof(buf), "推奨 Lv %d", info.recommendLv);
	DrawLabel(rx, y + 100.0f, buf, isShort ? kOrange() : kGray(), fontSmall_, 2);
}

// 一番下の「タイトルへ戻る」
void StageSelectScene::DrawBackCard(void) const
{
	const int index = GameProgress::STAGE_MAX;
	const bool isSelected = (index == cursor_);

	const float x = kBackX + cardOffset_[index];
	const float y = kBackY;

	DrawImageAt(isSelected ? imgButtonSelected_ : imgButtonNormal_, x - kImageMargin, y - kImageMargin);

	DrawLabel(x + kBackW * 0.5f, y + 16.0f, "タイトルへ戻る", isSelected ? kWhite() : kGray(), fontMid_, 1);
}

// 右上: レベルと経験値
void StageSelectScene::DrawStatusPanel(void) const
{
	DrawImageAt(imgPanelStatus_, kStatusX, kStatusY);

	const GameProgress::Data& d = GameProgress::Get();

	// レベル
	DrawLabel(kStatusX + 44.0f, kStatusY + 30.0f, "LEVEL", kOrange(), fontSmall_, 0);

	char buf[64];
	snprintf(buf, sizeof(buf), "%d", d.level);
	DrawLabel(kStatusX + 44.0f, kStatusY + 70.0f, buf, kWhite(), fontLevel_, 0);

	// 経験値のバー
	const float bx0 = kStatusX + 250.0f;
	const float bx1 = kStatusX + kStatusW - 44.0f;
	const float by0 = kStatusY + 112.0f;
	const float by1 = by0 + 20.0f;

	const bool isMax = (d.level >= GameProgress::LEVEL_MAX);

	if (isMax)
	{
		snprintf(buf, sizeof(buf), "EXP  MAX");
	}
	else
	{
		snprintf(buf, sizeof(buf), "EXP  %d / %d", d.exp, GameProgress::ExpToNext(d.level));
	}

	DrawLabel(bx0, kStatusY + 62.0f, buf, kWhite(), fontSmall_, 0);

	float rate = isMax ? 1.0f : static_cast<float>(d.exp) / static_cast<float>(GameProgress::ExpToNext(d.level));

	if (rate > 1.0f)
	{
		rate = 1.0f;
	}

	DrawBoxAA(bx0 * scaleX_, by0 * scaleY_, bx1 * scaleX_, by1 * scaleY_, GetColor(40, 46, 90), TRUE);
	DrawBoxAA(bx0 * scaleX_, by0 * scaleY_, (bx0 + (bx1 - bx0) * rate) * scaleX_, by1 * scaleY_,
		isMax ? GetColor(120, 220, 255) : kOrange(), TRUE);
	DrawBoxAA(bx0 * scaleX_, by0 * scaleY_, bx1 * scaleX_, by1 * scaleY_, GetColor(130, 145, 210), FALSE, 1.5f);

	// 攻撃力
	snprintf(buf, sizeof(buf), "攻撃力  x%.1f", GameProgress::AttackRate());
	DrawLabel(bx0, kStatusY + 150.0f, buf, kGray(), fontSmall_, 0);
}

// 右: 選んでいる項目の説明
void StageSelectScene::DrawDetailPanel(void) const
{
	DrawImageAt(imgPanelDetail_, kDetailX, kDetailY);

	const float lx = kDetailX + 48.0f;
	const float rx = kDetailX + kDetailW - 48.0f;
	const float top = kDetailY;

	// 区切りの線
	auto line = [&](float y)
		{
			DrawLine(static_cast<int>(lx * scaleX_), static_cast<int>((top + y) * scaleY_),
				static_cast<int>(rx * scaleX_), static_cast<int>((top + y) * scaleY_), GetColor(80, 92, 150));
		};

	char buf[96];

	// タイトルへ戻る
	if (cursor_ == GameProgress::STAGE_MAX)
	{
		DrawLabel(lx, top + 50.0f, "タイトルへ戻る", kWhite(), fontBig_, 0);
		line(150.0f);
		DrawLabel(lx, top + 180.0f, "タイトル画面に戻ります。", kGray(), fontSmall_, 0);
		DrawLabel(lx, top + 224.0f, "レベルとクリア状況は保存されています。", kGray(), fontSmall_, 0);
		return;
	}

	snprintf(buf, sizeof(buf), "STAGE %d", cursor_ + 1);
	DrawLabel(lx, top + 34.0f, buf, kOrange(), fontSmall_, 0);

	// まだ選べない
	if (!GameProgress::IsUnlocked(cursor_))
	{
		DrawLabel(lx, top + 76.0f, "? ? ?", kDark(), fontBig_, 0);
		line(172.0f);
		DrawLabel(lx, top + 204.0f, "前のステージをクリアすると", kGray(), fontSmall_, 0);
		DrawLabel(lx, top + 248.0f, "挑戦できるようになります。", kGray(), fontSmall_, 0);
		return;
	}

	const StageData::Info& info = StageData::Get(cursor_);

	DrawLabel(lx, top + 76.0f, info.name, kWhite(), fontBig_, 0);
	line(172.0f);

	// 推奨レベル
	const bool isShort = (GameProgress::Get().level < info.recommendLv);

	DrawLabel(lx, top + 206.0f, "推奨レベル", kGray(), fontSmall_, 0);
	snprintf(buf, sizeof(buf), "Lv %d", info.recommendLv);
	DrawLabel(rx, top + 196.0f, buf, isShort ? kOrange() : kWhite(), fontMid_, 2);

	// 報酬
	DrawLabel(lx, top + 286.0f, "クリア報酬", kGray(), fontSmall_, 0);
	snprintf(buf, sizeof(buf), "EXP +%d", info.expReward);
	DrawLabel(rx, top + 276.0f, buf, kWhite(), fontMid_, 2);

	line(354.0f);

	// ひとこと
	if (isShort)
	{
		DrawLabel(lx, top + 386.0f, "推奨レベルに届いていません。", kOrange(), fontSmall_, 0);
		DrawLabel(lx, top + 430.0f, "前のステージで経験値を稼ぐと楽になります。", kOrange(), fontSmall_, 0);
	}
	else if (GameProgress::Get().cleared[cursor_])
	{
		DrawLabel(lx, top + 386.0f, "クリア済み。", kGreen(), fontSmall_, 0);
		DrawLabel(lx, top + 430.0f, "何度でも挑戦して、経験値を稼げます。", kGreen(), fontSmall_, 0);
	}
	else
	{
		DrawLabel(lx, top + 386.0f, "挑戦できます。", kGreen(), fontSmall_, 0);
	}
}

void StageSelectScene::DrawHint(void) const
{
	DrawLabel(960.0f, 1030.0f,
		"↑↓ 選択     ENTER / ○ 決定     ESC / × 戻る", kGray(), fontSmall_, 1);
}