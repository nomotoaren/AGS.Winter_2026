#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <DxLib.h>
#include "../../Manager/SceneManager.h"
#include "EnemyHudLayout.h"
#include "../Common/HudDraw.h"
#include "EnemyHud.h"

using HudDraw::DrawStringRight;
using HudDraw::HpLayerColor;
using HudDraw::DrawLayeredHp;

namespace
{
	constexpr float BAR_WIDTH = 660.0f;		// 横長のバー(右端がアイコンの裏に入る)
	constexpr float BAR_HEIGHT = EnemyHudLayout::HP_BAR_HEIGHT;

	constexpr float DELAY_WAIT_TIME = 0.5f;
	constexpr float DELAY_SPEED_RATE = 0.45f;
	constexpr float FLASH_TIME = 0.25f;
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
	initialized_(false)
{
}

EnemyHud::~EnemyHud(void)
{
	DeleteFonts();
}

void EnemyHud::DeleteFonts(void)
{
	for (int* font : { &fontName_, &fontIcon_, &fontNum_ })
	{
		if (*font != -1)
		{
			DeleteFontToHandle(*font);
			*font = -1;
		}
	}
}

void EnemyHud::Init(void)
{
	// 2回呼ばれても、前のフォントが残らないようにする
	DeleteFonts();

	int sw = 0;
	int sh = 0;
	GetDrawScreenSize(&sw, &sh);
	const float s = sw / EnemyHudLayout::BASE_WIDTH;

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

	// 初回は現在値に合わせる(最大HPが未指定なら、それを最大値にする)
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

	// 被弾: 赤いバーはそのまま残し、少し待ってから減らす
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

	// ---- レイアウト(画面右上。アイコンが一番右、バーはその左) ----
	const EnemyHudLayout::Anchor a = EnemyHudLayout::Compute(sw);
	const float s = a.scale;
	const float iconR = a.iconR;
	const float iconX = a.iconX;
	const float iconY = a.iconY;

	const float barRight = a.barRight;
	const float textRight = iconX - iconR - 8.0f * s;	// 文字の右端はアイコンの手前
	const float barW = BAR_WIDTH * s;
	const float barH = BAR_HEIGHT * s;
	const float barX = barRight - barW;
	const float barY = a.barY;

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

	// 重なる体力ゲージ(層ごとに色が違い、減った分は赤く残る)
	const int layerNum = DrawLayeredHp(barX, barY, barW, barH, s,
		(float)hp_, delayedHp_, (float)maxHp_, frameColor, frameThick);

	// 残りの層の数(「x4」)をバーの左に出す
	if (layerNum > 0)
	{
		char layerBuf[16];
		std::snprintf(layerBuf, sizeof(layerBuf), "x%d", layerNum);

		DrawStringRight((int)(barX - 8.0f * s), (int)(barY - 3.0f * s),
			layerBuf, HpLayerColor(layerNum - 1), fontNum_);
	}

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