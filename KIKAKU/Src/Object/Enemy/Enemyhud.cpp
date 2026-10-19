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

	// ---- 重なる体力ゲージ(カカロット方式) ----
	//   体力を HP_LAYERS 枚の層に分け、いちばん上の層から順に減らす。
	//   減りきると、下の層の色が現れる。層ごとに色が違う。
	//   ダメージで減った分は、赤いバーが遅れて縮む。
	constexpr int HP_LAYERS = 5;

	// 層の色(下から順)。1層目(最後の1層)が赤、5層目(いちばん上)が濃い緑
	unsigned int HpLayerColor(int layer)
	{
		switch (layer)
		{
		case 0:  return GetColor(190, 35, 45);		// 1層目: 赤
		case 1:  return GetColor(250, 235, 120);	// 2層目: 薄い黄色
		case 2:  return GetColor(225, 175, 20);		// 3層目: 濃い黄色
		case 3:  return GetColor(140, 225, 110);	// 4層目: 薄い緑
		default: return GetColor(25, 150, 55);		// 5層目: 濃い緑
		}
	}

	// 値 v が何番目の層にあるか(0 が最下層)
	int HpLayerOf(float v, float layerHp)
	{
		if (v <= 0.0f) { return 0; }

		int layer = (int)std::ceil(v / layerHp) - 1;
		if (layer < 0) { layer = 0; }
		if (layer > HP_LAYERS - 1) { layer = HP_LAYERS - 1; }
		return layer;
	}

	// 層の中での割合(0~1)
	float HpLayerFrac(float v, int layer, float layerHp)
	{
		return std::clamp((v - layer * layerHp) / layerHp, 0.0f, 1.0f);
	}

	// 戻り値: 今の層の数(「x3」と表示する用)
	int DrawLayeredHp(float x, float y, float w, float h, float s,
		float hp, float delayedHp, float maxHp,
		unsigned int frameColor, float frameThick, float time)
	{
		const float layerHp = maxHp / (float)HP_LAYERS;
		const float right = x + w;

		const int curL = HpLayerOf(hp, layerHp);
		const float frac = (hp <= 0.0f) ? 0.0f : HpLayerFrac(hp, curL, layerHp);

		// 影
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
		DrawBoxAA(x - 3.0f * s, y - 3.0f * s, right + 3.0f * s, y + h + 3.0f * s,
			GetColor(10, 10, 14), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

		// 空の部分 / 下の層(すぐ下の層の色が、全幅で見えている)
		DrawBoxAA(x, y, right, y + h, GetColor(28, 30, 34), TRUE);

		if (curL > 0)
		{
			DrawBoxAA(x, y, right, y + h, HpLayerColor(curL - 1), TRUE);
		}

		// ダメージで減った分: 明るい赤のバーが遅れて縮む
		if (delayedHp > hp)
		{
			const int dL = HpLayerOf(delayedHp, layerHp);
			const float dFrac = (dL == curL) ? HpLayerFrac(delayedHp, dL, layerHp) : 1.0f;

			if (dFrac > frac)
			{
				DrawBoxAA(right - w * dFrac, y, right - w * frac, y + h, GetColor(255, 85, 70), TRUE);
			}
		}

		// 今の層(1層目の赤は、元から赤いので脈打たせない)
		const unsigned int color = HpLayerColor(curL);

		if (frac > 0.0f)
		{
			DrawBoxAA(right - w * frac, y, right, y + h, color, TRUE);

			// 上半分に光沢
			SetDrawBlendMode(DX_BLENDMODE_ALPHA, 75);
			DrawBoxAA(right - w * frac, y, right, y + h * 0.45f, GetColor(255, 255, 255), TRUE);
			SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		}

		DrawBoxAA(x, y, right, y + h, frameColor, FALSE, frameThick);

		return (hp <= 0.0f) ? 0 : curL + 1;
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

	// 重なる体力ゲージ(層ごとに色が違い、減った分は赤い)
	const int layerNum = DrawLayeredHp(barX, barY, barW, barH, s,
		(float)hp_, delayedHp_, (float)maxHp_, frameColor, frameThick, time_);

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