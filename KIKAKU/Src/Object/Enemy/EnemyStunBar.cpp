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
	// 満タンのときは出さない(うるさくならないように)
	if (!stunned && rate >= 1.0f)
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

	// 色: 削られている間は紫、スタン中(回復待ち)は黄色く点滅
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

// 画面中央に、黄色い放射状の光と「BREAK!」を出す(カカロットのブレイク演出)
void EnemyStunBar::DrawBreak(float elapsed) const
{
	if (elapsed < 0.0f || elapsed >= BREAK_TIME)
	{
		return;
	}

	int sw = 0;
	int sh = 0;
	GetDrawScreenSize(&sw, &sh);

	const float cx = sw * 0.5f;
	const float cy = sh * 0.42f;
	const float s = sw / BASE_WIDTH;

	// 最後の 0.4 秒で薄くなる
	float fade = 1.0f;
	const float fadeStart = BREAK_TIME - 0.4f;
	if (elapsed > fadeStart)
	{
		fade = 1.0f - (elapsed - fadeStart) / 0.4f;
	}

	// 放射状の光: 最初の 0.2 秒で一気に伸びる
	float grow = elapsed / 0.2f;
	if (grow > 1.0f) { grow = 1.0f; }
	const float len = (260.0f + 700.0f * grow) * s;

	const int RAYS = 14;
	const float PI2 = 6.2831853f;
	const float spin = elapsed * 0.25f;

	SetDrawBlendMode(DX_BLENDMODE_ADD, (int)(200.0f * fade));
	for (int i = 0; i < RAYS; i++)
	{
		// 太さと長さを少しずらして、ギザギザに見せる
		const float a = spin + PI2 * i / RAYS;
		const float w = (i % 2 == 0) ? 0.10f : 0.06f;
		const float l = (i % 2 == 0) ? len : len * 0.7f;

		DrawTriangleAA(
			cx, cy,
			cx + cosf(a - w) * l, cy + sinf(a - w) * l,
			cx + cosf(a + w) * l, cy + sinf(a + w) * l,
			GetColor(255, 215, 80), TRUE);
	}

	// 中心の光の玉
	DrawCircleAA(cx, cy, (120.0f + 40.0f * grow) * s, 48, GetColor(255, 240, 170), TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	// 衝撃波の輪
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(220.0f * fade * (1.0f - grow * 0.6f)));
	DrawCircleAA(cx, cy, (80.0f + 420.0f * grow) * s, 64, GetColor(255, 245, 200), FALSE, 6.0f * s);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	// 「BREAK!」: 大きく出て、すぐ標準の大きさに戻る
	const char* text = "BREAK!";
	float pop = 1.0f;
	if (elapsed < 0.15f)
	{
		pop = 2.2f - 1.2f * (elapsed / 0.15f);
	}

	const int prevSize = GetFontSize();
	const int fontSize = (int)(72.0f * s);
	SetFontSize(fontSize);

	const int tw = GetDrawStringWidth(text, (int)strlen(text));
	const double scale = pop;
	const int drawW = (int)(tw * scale);
	const int drawH = (int)(fontSize * scale);
	const int tx = (int)(cx - drawW * 0.5f);
	const int ty = (int)(cy - drawH * 0.5f);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(255.0f * fade));

	// 縁取り(濃い橙)→ 本体(黄色)
	const int o = (int)(4.0f * s) + 1;
	for (int dy = -o; dy <= o; dy += o)
	{
		for (int dx = -o; dx <= o; dx += o)
		{
			if (dx == 0 && dy == 0) { continue; }
			DrawExtendString(tx + dx, ty + dy, scale, scale, text, GetColor(150, 70, 0));
		}
	}
	DrawExtendString(tx, ty, scale, scale, text, GetColor(255, 225, 60));

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	SetFontSize(prevSize);
}