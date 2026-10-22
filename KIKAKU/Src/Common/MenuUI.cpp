#include <DxLib.h>
#include <cmath>
#include "../Application.h"
#include "../Manager/InputManager.h"
#include "../Manager/PadInput.h"
#include "MenuUI.h"

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

void MenuUI::CreateFonts(void) const
{
	// 作成済みなら何もしない
	if (fontHeading_ != -1)
	{
		return;
	}

	const int type = DX_FONTTYPE_ANTIALIASING_EDGE_4X4;
	fontHeading_ = CreateFontToHandle("Meiryo UI", 96, 6, type, -1, 4);
	fontItem_ = CreateFontToHandle("Meiryo UI", 40, 4, type, -1, 2);
	fontCaption_ = CreateFontToHandle("Meiryo UI", 32, 3, type, -1, 2);
}

void MenuUI::Open(const std::vector<std::string>& items, int defaultIndex)
{
	items_ = items;
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
	prevStickDir_ = 2;
}

void MenuUI::Close(void)
{
	isOpen_ = false;
}

int MenuUI::Update(float dt)
{
	if (!isOpen_ || items_.empty())
	{
		return RESULT_NONE;
	}

	InputManager& ins = InputManager::GetInstance();

	animTimer_ += dt;

	// 決定・戻るを受け付けるまでの待ち時間
	const bool guarding = guardTimer_ > 0.0f;
	if (guarding)
	{
		guardTimer_ -= dt;
	}

	const int count = static_cast<int>(items_.size());

	// --- 上下の選択 ---
	// スティックは「倒した瞬間」だけ反応させたいので、前フレームとの差で見る
	int stickDir = 0;
	if (PadInput::StickY() > STICK_THRESHOLD) { stickDir = 1; }
	if (PadInput::StickY() < -STICK_THRESHOLD) { stickDir = -1; }

	// 開いた直後(prevStickDir_ == 2)は、倒しっぱなしでも反応しないよう今の状態で上書きする
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

	if (up)
	{
		// 上の端から更に上へ行くと、下の端へ回り込む
		cursor_ = (cursor_ + count - 1) % count;
	}
	if (down)
	{
		cursor_ = (cursor_ + 1) % count;
	}

	if (guarding)
	{
		return RESULT_NONE;
	}

	// --- 決定 ---
	const bool decide =
		ins.IsTrgDown(KEY_INPUT_RETURN) ||
		ins.IsTrgDown(KEY_INPUT_SPACE) ||
		PadInput::IsTrg(PadInput::BTN::CIRCLE);

	if (decide)
	{
		return cursor_;
	}

	// --- 戻る ---
	const bool cancel =
		ins.IsTrgDown(KEY_INPUT_ESCAPE) ||
		ins.IsTrgDown(KEY_INPUT_BACK) ||
		PadInput::IsTrg(PadInput::BTN::CROSS);

	if (cancel)
	{
		return RESULT_CANCEL;
	}

	return RESULT_NONE;
}

void MenuUI::Draw(int topY) const
{
	if (!isOpen_)
	{
		return;
	}

	CreateFonts();

	const int cx = Application::SCREEN_SIZE_X / 2;

	// 説明文
	if (!caption_.empty())
	{
		const int w = GetDrawStringWidthToHandle(caption_.c_str(), static_cast<int>(caption_.size()), fontCaption_);
		DrawStringToHandle(cx - w / 2, topY - 80, caption_.c_str(), GetColor(255, 255, 255), fontCaption_, GetColor(0, 0, 0));
	}

	// 選択中の項目はゆっくり明滅させる(0.6 ～ 1.0)
	const float pulse = 0.8f + 0.2f * sinf(animTimer_ * 6.0f);
	const int bright = static_cast<int>(255.0f * pulse);

	for (int i = 0; i < static_cast<int>(items_.size()); i++)
	{
		const std::string& text = items_[i];
		const int w = GetDrawStringWidthToHandle(text.c_str(), static_cast<int>(text.size()), fontItem_);
		const int y = topY + i * ITEM_INTERVAL;

		if (i == cursor_)
		{
			// 選択中: 帯 + 矢印 + 明るい文字
			SetDrawBlendMode(DX_BLENDMODE_ALPHA, 110);
			DrawBox(cx - 260, y - 8, cx + 260, y + 52, GetColor(255, 170, 40), TRUE);
			SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

			DrawStringToHandle(cx - w / 2, y, text.c_str(), GetColor(bright, bright, bright), fontItem_, GetColor(120, 60, 0));
			// 左側に三角の矢印(文字ではなく図形で描く)
			const int ax = cx - w / 2 - 44;
			DrawTriangle(ax, y + 4, ax, y + 40, ax + 28, y + 22, GetColor(255, 200, 60), TRUE);
		}
		else
		{
			DrawStringToHandle(cx - w / 2, y, text.c_str(), GetColor(170, 170, 170), fontItem_, GetColor(0, 0, 0));
		}
	}
}

void MenuUI::DrawHeading(const std::string& text, int centerY, unsigned int color, int alpha) const
{
	CreateFonts();

	if (alpha < 0) { alpha = 0; }
	if (alpha > 255) { alpha = 255; }

	const int w = GetDrawStringWidthToHandle(text.c_str(), static_cast<int>(text.size()), fontHeading_);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
	DrawStringToHandle(
		Application::SCREEN_SIZE_X / 2 - w / 2, centerY - 48,
		text.c_str(), color, fontHeading_, GetColor(0, 0, 0));
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void MenuUI::DrawDim(int alpha, unsigned int color)
{
	if (alpha <= 0)
	{
		return;
	}
	if (alpha > 255) { alpha = 255; }

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
	DrawBox(0, 0, Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y, color, TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}