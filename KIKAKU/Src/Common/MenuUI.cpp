#include <DxLib.h>
#include <cmath>
#include "../Application.h"
#include "../Manager/InputManager.h"
#include "../Manager/PadInput.h"
#include "MenuUI.h"

namespace
{
	// --- フォント(名前, 大きさ, 太さ, 縁の太さ) ---
	const char* const kFontName = "Meiryo UI";
	constexpr int kHeadingSize = 96;
	constexpr int kHeadingThick = 6;
	constexpr int kHeadingEdge = 4;
	constexpr int kItemSize = 40;
	constexpr int kItemThick = 4;
	constexpr int kItemEdge = 2;
	constexpr int kCaptionSize = 32;
	constexpr int kCaptionThick = 3;
	constexpr int kCaptionEdge = 2;

	// --- 配置 ---
	constexpr int kCaptionOffsetY = 80;		// 説明文は、最初の項目の何px上か
	constexpr int kHeadingOffsetY = 48;		// 見出しの文字の高さの半分くらい
	constexpr int kBarHalfWidth = 260;		// 選択中の帯の、中心から端までの幅
	constexpr int kBarTop = -8;				// 帯の上端(項目のYからの差)
	constexpr int kBarBottom = 52;			// 帯の下端(項目のYからの差)
	constexpr int kArrowGap = 44;			// 文字の左端から、矢印までの距離
	constexpr int kArrowTop = 4;
	constexpr int kArrowBottom = 40;
	constexpr int kArrowWidth = 28;

	// --- 見た目 ---
	constexpr float kPulseBase = 0.8f;		// 選択中の文字の明るさ(0.6 〜 1.0 で明滅)
	constexpr float kPulseWidth = 0.2f;
	constexpr float kPulseSpeed = 6.0f;
	constexpr int kBarAlpha = 110;
	constexpr int kNormalGray = 170;		// 選択されていない項目の文字色

	// 数値を最小〜最大の範囲に収める
	int ClampInt(int v, int lo, int hi)
	{
		return v < lo ? lo : (v > hi ? hi : v);
	}
}

MenuUI::MenuUI(void)
	:
	isOpen_(false),
	cursor_(0),
	guardTimer_(0.0f),
	animTimer_(0.0f),
	prevStickDir_(0),
	fontHeading_(-1),
	fontItem_(-1),
	fontCaption_(-1)
{
}

MenuUI::~MenuUI(void)
{
	if (fontHeading_ != -1) { DeleteFontToHandle(fontHeading_); }
	if (fontItem_ != -1) { DeleteFontToHandle(fontItem_); }
	if (fontCaption_ != -1) { DeleteFontToHandle(fontCaption_); }
}

void MenuUI::Open(const std::vector<std::string>& items, int defaultIndex)
{
	items_ = items;

	// 範囲外の指定は、先頭の項目にする
	cursor_ = defaultIndex;
	if (cursor_ < 0 || cursor_ >= static_cast<int>(items_.size()))
	{
		cursor_ = 0;
	}

	isOpen_ = true;
	guardTimer_ = INPUT_GUARD_TIME;
	animTimer_ = 0.0f;
	caption_.clear();

	// 開いた時にスティックが倒れていても、すぐには動かさない
	prevStickDir_ = STICK_UNREAD;
}

void MenuUI::Close(void)
{
	isOpen_ = false;
}

//------------------------------------------------------------
// 更新
//------------------------------------------------------------
int MenuUI::Update(float dt)
{
	if (!isOpen_ || items_.empty())
	{
		return RESULT_NONE;
	}

	animTimer_ += dt;

	// カーソルは、待ち時間の間も動かせる
	MoveCursor(ReadMoveInput());

	// 決定・戻るは、開いた直後の待ち時間が終わってから受け付ける
	if (guardTimer_ > 0.0f)
	{
		guardTimer_ -= dt;
		return RESULT_NONE;
	}

	if (IsDecidePressed())
	{
		return cursor_;
	}

	if (IsCancelPressed())
	{
		return RESULT_CANCEL;
	}

	return RESULT_NONE;
}

int MenuUI::ReadMoveInput(void)
{
	InputManager& ins = InputManager::GetInstance();

	// スティックは「倒した瞬間」だけ反応させたいので、前フレームとの差で見る
	int stickDir = 0;
	if (PadInput::StickY() > STICK_THRESHOLD) { stickDir = 1; }
	if (PadInput::StickY() < -STICK_THRESHOLD) { stickDir = -1; }

	// 開いた直後は、倒しっぱなしでも反応しないよう今の状態で上書きする
	if (prevStickDir_ == STICK_UNREAD)
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

	// 同時に押したら打ち消し合う
	return (down ? 1 : 0) - (up ? 1 : 0);
}

void MenuUI::MoveCursor(int move)
{
	const int count = static_cast<int>(items_.size());

	// 上の端から更に上へ行くと下の端へ、下の端から更に下へ行くと上の端へ回り込む
	cursor_ = (cursor_ + move + count) % count;
}

bool MenuUI::IsDecidePressed(void)
{
	InputManager& ins = InputManager::GetInstance();

	return
		ins.IsTrgDown(KEY_INPUT_RETURN) ||
		ins.IsTrgDown(KEY_INPUT_SPACE) ||
		PadInput::IsTrg(PadInput::BTN::CIRCLE);
}

bool MenuUI::IsCancelPressed(void)
{
	InputManager& ins = InputManager::GetInstance();

	return
		ins.IsTrgDown(KEY_INPUT_ESCAPE) ||
		ins.IsTrgDown(KEY_INPUT_BACK) ||
		PadInput::IsTrg(PadInput::BTN::CROSS);
}

//------------------------------------------------------------
// 描画
//------------------------------------------------------------
void MenuUI::CreateFonts(void) const
{
	// 作成済みなら何もしない
	if (fontHeading_ != -1)
	{
		return;
	}

	const int type = DX_FONTTYPE_ANTIALIASING_EDGE_4X4;
	fontHeading_ = CreateFontToHandle(kFontName, kHeadingSize, kHeadingThick, type, -1, kHeadingEdge);
	fontItem_ = CreateFontToHandle(kFontName, kItemSize, kItemThick, type, -1, kItemEdge);
	fontCaption_ = CreateFontToHandle(kFontName, kCaptionSize, kCaptionThick, type, -1, kCaptionEdge);
}

int MenuUI::GetTextWidth(const std::string& text, int font) const
{
	return GetDrawStringWidthToHandle(text.c_str(), static_cast<int>(text.size()), font);
}

void MenuUI::Draw(int topY) const
{
	if (!isOpen_)
	{
		return;
	}

	CreateFonts();

	DrawCaption(topY);

	// 選択中の項目は、ゆっくり明滅させる
	const float pulse = kPulseBase + kPulseWidth * sinf(animTimer_ * kPulseSpeed);
	const int brightness = ClampInt(static_cast<int>(255.0f * pulse), 0, 255);

	for (int i = 0; i < static_cast<int>(items_.size()); i++)
	{
		DrawItem(i, topY + i * ITEM_INTERVAL, brightness);
	}
}

void MenuUI::DrawCaption(int topY) const
{
	if (caption_.empty())
	{
		return;
	}

	const int cx = Application::SCREEN_SIZE_X / 2;
	const int w = GetTextWidth(caption_, fontCaption_);

	DrawStringToHandle(
		cx - w / 2, topY - kCaptionOffsetY,
		caption_.c_str(), GetColor(255, 255, 255), fontCaption_, GetColor(0, 0, 0));
}

void MenuUI::DrawItem(int index, int y, int brightness) const
{
	const std::string& text = items_[index];
	const int cx = Application::SCREEN_SIZE_X / 2;
	const int w = GetTextWidth(text, fontItem_);

	// 選択されていない項目: 灰色の文字だけ
	if (index != cursor_)
	{
		DrawStringToHandle(
			cx - w / 2, y, text.c_str(),
			GetColor(kNormalGray, kNormalGray, kNormalGray), fontItem_, GetColor(0, 0, 0));
		return;
	}

	// 選択中: 帯 + 矢印 + 明るい文字
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, kBarAlpha);
	DrawBox(cx - kBarHalfWidth, y + kBarTop, cx + kBarHalfWidth, y + kBarBottom, GetColor(255, 170, 40), TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	DrawStringToHandle(
		cx - w / 2, y, text.c_str(),
		GetColor(brightness, brightness, brightness), fontItem_, GetColor(120, 60, 0));

	// 左側に三角の矢印(文字ではなく図形で描く)
	const int ax = cx - w / 2 - kArrowGap;
	DrawTriangle(
		ax, y + kArrowTop,
		ax, y + kArrowBottom,
		ax + kArrowWidth, y + (kArrowTop + kArrowBottom) / 2,
		GetColor(255, 200, 60), TRUE);
}

void MenuUI::DrawHeading(const std::string& text, int centerY, unsigned int color, int alpha) const
{
	CreateFonts();

	const int w = GetTextWidth(text, fontHeading_);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, ClampInt(alpha, 0, 255));
	DrawStringToHandle(
		Application::SCREEN_SIZE_X / 2 - w / 2, centerY - kHeadingOffsetY,
		text.c_str(), color, fontHeading_, GetColor(0, 0, 0));
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void MenuUI::DrawDim(int alpha, unsigned int color)
{
	if (alpha <= 0)
	{
		return;
	}

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, ClampInt(alpha, 0, 255));
	DrawBox(0, 0, Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y, color, TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}