#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <DxLib.h>
#include "../../Manager/SceneManager.h"
#include "EnemyHud.h"

namespace
{
	// 基準解像度(横1500)でのレイアウト。実際の画面幅に合わせて拡大縮小する
	constexpr float BASE_WIDTH = 1500.0f;

	constexpr float MARGIN_X = 50.0f;		// 画面右端からアイコンまで
	constexpr float MARGIN_Y = 28.0f;		// 画面上端からアイコンまで
	constexpr float ICON_RADIUS = 40.0f;
	constexpr float BAR_WIDTH = 660.0f;		// 細長いバー(右端がアイコンの裏にくる)
	constexpr float BAR_HEIGHT = 16.0f;

	constexpr float DELAY_WAIT_TIME = 0.5f;
	constexpr float DELAY_SPEED_RATE = 0.45f;
	constexpr float FLASH_TIME = 0.25f;

	// 右寄せで文字を描く
	void DrawStringRight(int rightX, int y, const char* str, unsigned int color, int font)
	{
		const int w = GetDrawStringWidthToHandle(str, (int)std::strlen(str), font);
		DrawStringToHandle(rightX - w, y, str, color, font);
	}
}

EnemyHud::EnemyHud(void)
	:
	fontName_(-1),
	fontIcon_(-1),
	fontNum_(-1),
	name_("ENEMY"),
	iconText_("E"),
	hp_(0),
	maxHp_(0),
	delayedHp_(0.0f),
	delayWait_(0.0f),
	flashTimer_(0.0f),
	time_(0.0f),
	initialized_(false)
{
}

EnemyHud::~EnemyHud(void)
{
	if (fontName_ != -1)
	{
		DeleteFontToHandle(fontName_);
	}

	if (fontIcon_ != -1)
	{
		DeleteFontToHandle(fontIcon_);
	}

	if (fontNum_ != -1)
	{
		DeleteFontToHandle(fontNum_);
	}
}

void EnemyHud::Init(void)
{
	int sw = 0;
	int sh = 0;
	GetDrawScreenSize(&sw, &sh);
	const float s = sw / BASE_WIDTH;

	fontName_ = CreateFontToHandle(nullptr, (int)(26.0f * s), 3,
		DX_FONTTYPE_ANTIALIASING_EDGE_4X4, -1, 2);
	fontNum_ = CreateFontToHandle(nullptr, (int)(22.0f * s), 3,
		DX_FONTTYPE_ANTIALIASING_EDGE_4X4, -1, 2);
	fontIcon_ = CreateFontToHandle(nullptr, (int)(36.0f * s), 3,
		DX_FONTTYPE_ANTIALIASING_EDGE_4X4, -1, 2);

	initialized_ = false;
}

void EnemyHud::SetName(const std::string& name) { name_ = name; }
void EnemyHud::SetIconText(const std::string& text) { iconText_ = text; }
void EnemyHud::SetMaxHp(int maxHp) { maxHp_ = maxHp; }

void EnemyHud::Update(int hp)
{
	const float dt = SceneManager::GetInstance().GetDeltaTime();

	time_ += dt;

	// 初回は現在値に合わせる(最大HPが未指定なら、これを最大値にする)
	if (!initialized_)
	{
		initialized_ = true;
		hp_ = hp;
		delayedHp_ = (float)hp;

		if (maxHp_ <= 0)
		{
			maxHp_ = hp;
		}
	}

	if (hp > maxHp_)
	{
		maxHp_ = hp;
	}

	// 被弾: 白いバーはそのまま残し、少し待ってから減らす
	if (hp < hp_)
	{
		delayWait_ = DELAY_WAIT_TIME;
		flashTimer_ = FLASH_TIME;
	}

	// 回復
	if ((float)hp > delayedHp_)
	{
		delayedHp_ = (float)hp;
	}

	if (delayedHp_ > (float)hp)
	{
		if (delayWait_ > 0.0f)
		{
			delayWait_ -= dt;
		}
		else
		{
			delayedHp_ -= (float)maxHp_ * DELAY_SPEED_RATE * dt;

			if (delayedHp_ < (float)hp)
			{
				delayedHp_ = (float)hp;
			}
		}
	}

	if (flashTimer_ > 0.0f)
	{
		flashTimer_ -= dt;

		if (flashTimer_ < 0.0f)
		{
			flashTimer_ = 0.0f;
		}
	}

	hp_ = hp;
}

void EnemyHud::Draw(void)
{
	if (!initialized_ || maxHp_ <= 0)
	{
		return;
	}

	int sw = 0;
	int sh = 0;
	GetDrawScreenSize(&sw, &sh);
	const float s = sw / BASE_WIDTH;

	const float rate = std::clamp((float)hp_ / (float)maxHp_, 0.0f, 1.0f);
	const float delayedRate = std::clamp(delayedHp_ / (float)maxHp_, 0.0f, 1.0f);

	// ---- レイアウト(画面右上。アイコンが一番右、バーはその左) ----
	const float iconR = ICON_RADIUS * s;
	const float iconX = sw - MARGIN_X * s - iconR - 10.0f * s;
	const float iconY = MARGIN_Y * s + iconR;

	const float barRight = iconX - iconR * 0.35f;
	const float textRight = iconX - iconR - 8.0f * s;	// 文字の右端はアイコンの手前
	const float barW = BAR_WIDTH * s;
	const float barH = BAR_HEIGHT * s;
	const float barX = barRight - barW;
	const float barY = iconY - 20.0f * s;

	const unsigned int dark = GetColor(10, 14, 26);

	// ---- 名前(バーの上、右寄せ) ----
	DrawStringRight((int)textRight, (int)(barY - 34.0f * s),
		name_.c_str(), GetColor(255, 255, 255), fontName_);

	// ---- ゲージ(右から左へ減る) ----
	unsigned int frameColor = GetColor(165, 135, 70);
	float frameThick = 2.0f * s;

	if (flashTimer_ > 0.0f)
	{
		frameColor = GetColor(255, 255, 255);
		frameThick = 3.5f * s;
	}

	// 影
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
	DrawBoxAA(barX - 3.0f * s, barY - 3.0f * s, barRight + 3.0f * s, barY + barH + 3.0f * s,
		dark, TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	// 空の部分
	DrawBoxAA(barX, barY, barRight, barY + barH, GetColor(28, 30, 34), TRUE);

	// 遅れて減る部分(水色)
	if (delayedRate > rate)
	{
		DrawBoxAA(barRight - barW * delayedRate, barY, barRight - barW * rate, barY + barH,
			GetColor(110, 215, 235), TRUE);
	}

	// 現在のHP。緑。ピンチ(25%以下)は赤く脈打つ
	int r = 75;
	int g = 215;
	int b = 65;

	if (rate <= 0.25f)
	{
		const float pulse = 0.5f + 0.5f * sinf(time_ * 9.0f);
		r = 215 + (int)(40.0f * pulse);
		g = 45 + (int)(45.0f * pulse);
		b = 50;
	}

	if (rate > 0.0f)
	{
		DrawBoxAA(barRight - barW * rate, barY, barRight, barY + barH, GetColor(r, g, b), TRUE);

		// 上半分に光沢
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 75);
		DrawBoxAA(barRight - barW * rate, barY, barRight, barY + barH * 0.45f,
			GetColor(255, 255, 255), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	}

	DrawBoxAA(barX, barY, barRight, barY + barH, frameColor, FALSE, frameThick);

	// ---- HPの数値(バーの下、右寄せ) ----
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%d", hp_);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 150);
	DrawBoxAA(textRight - 200.0f * s, barY + barH + 1.0f * s,
		textRight + 8.0f * s, barY + barH + 28.0f * s, dark, TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	DrawStringRight((int)textRight, (int)(barY + barH + 2.0f * s),
		buf, GetColor(200, 255, 130), fontNum_);

	// ---- アイコン ----
	DrawCircleAA(iconX, iconY, iconR + 5.0f * s, 48, dark, TRUE);
	DrawCircleAA(iconX, iconY, iconR, 48, GetColor(110, 30, 40), TRUE);

	// 上半分に光沢
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 60);
	DrawCircleAA(iconX, iconY - iconR * 0.28f, iconR * 0.72f, 48, GetColor(255, 255, 255), TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	DrawCircleAA(iconX, iconY, iconR, 48, GetColor(165, 90, 210), FALSE, 4.0f * s);
	DrawCircleAA(iconX, iconY, iconR + 5.0f * s, 48, frameColor, FALSE, 2.0f * s);

	const int tw = GetDrawStringWidthToHandle(iconText_.c_str(), (int)iconText_.size(), fontIcon_);
	DrawStringToHandle((int)(iconX - tw * 0.5f), (int)(iconY - 20.0f * s),
		iconText_.c_str(), GetColor(255, 255, 255), fontIcon_);
}