#include <cmath>
#include <cstring>
#include <DxLib.h>
#include "EnemyStunBar.h"

namespace
{
	// EnemyHud と同じ基準解像度(約1500)でのレイアウト。実際の画面幅に合わせて拡大縮小する
	constexpr float BASE_WIDTH = 1500.0f;

	// EnemyHud.cpp の配置と合わせる値
	constexpr float MARGIN_X = 50.0f;
	constexpr float MARGIN_Y = 28.0f;
	constexpr float ICON_RADIUS = 40.0f;
	constexpr float HP_BAR_HEIGHT = 16.0f;

	constexpr float BAR_WIDTH = 300.0f;
	constexpr float BAR_HEIGHT = 10.0f;
	constexpr float OFFSET_BELOW_HP = 36.0f;	// HPバーの下からの距離(HPの数字の下に置く)
}

void EnemyStunBar::Draw(float rate, bool stunned, float time) const
{
	// たまっていないときは出さない(うるさくならないように)
	if (!stunned && rate <= 0.0f)
	{
		return;
	}

	int sw = 0;
	int sh = 0;
	GetDrawScreenSize(&sw, &sh);
	const float s = sw / BASE_WIDTH;

	if (rate < 0.0f) { rate = 0.0f; }
	if (rate > 1.0f) { rate = 1.0f; }

	// EnemyHud と同じ式で、HPバーの右端と下端を求める
	const float iconR = ICON_RADIUS * s;
	const float iconX = sw - MARGIN_X * s - iconR - 10.0f * s;
	const float iconY = MARGIN_Y * s + iconR;

	const float barRight = iconX - iconR * 0.35f;
	const float hpBarY = iconY - 20.0f * s;

	const float w = BAR_WIDTH * s;
	const float h = BAR_HEIGHT * s;
	const float x = barRight - w;
	const float y = hpBarY + HP_BAR_HEIGHT * s + OFFSET_BELOW_HP * s;

	// 影
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
	DrawBoxAA(x - 2.0f * s, y - 2.0f * s, barRight + 2.0f * s, y + h + 2.0f * s,
		GetColor(10, 14, 26), TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	// 空の部分
	DrawBoxAA(x, y, barRight, y + h, GetColor(28, 30, 34), TRUE);

	// 色: たまっている間は紫、スタン中は黄色く点滅
	int r = 190;
	int g = 110;
	int b = 255;

	if (stunned)
	{
		const float pulse = 0.5f + 0.5f * sinf(time * 14.0f);
		r = 255;
		g = 190 + (int)(60.0f * pulse);
		b = 40 + (int)(60.0f * pulse);
	}

	// 右から左へ向かって満ちる(HPバーと同じ向き)
	if (rate > 0.0f)
	{
		DrawBoxAA(barRight - w * rate, y, barRight, y + h, GetColor(r, g, b), TRUE);

		// 上半分に光沢
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 75);
		DrawBoxAA(barRight - w * rate, y, barRight, y + h * 0.45f, GetColor(255, 255, 255), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	}

	DrawBoxAA(x, y, barRight, y + h, GetColor(165, 135, 70), FALSE, 1.5f * s);

	// 文字(ゲージの左側)
	const char* text = stunned ? "STUN!" : "STUN";
	const int tw = GetDrawStringWidth(text, (int)strlen(text));

	DrawString((int)(x - tw - 8.0f * s), (int)(y - 2.0f * s), text,
		stunned ? GetColor(255, 230, 90) : GetColor(210, 190, 255));
}