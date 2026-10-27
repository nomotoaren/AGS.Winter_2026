#include <cmath>
#include <cstring>
#include <algorithm>
#include <DxLib.h>
#include "HudDraw.h"

namespace
{
	// ‘Ì—ÍƒQ[ƒW‚Ì‘w‚Ì”
	constexpr int HP_LAYERS = 5;

	// ’l v ‚ª‰½”Ô–Ú‚Ì‘w‚É‚ ‚é‚©(0 ‚ªÅ‰º‘w)
	int HpLayerOf(float v, float layerHp)
	{
		if (v <= 0.0f) { return 0; }

		int layer = (int)std::ceil(v / layerHp) - 1;
		if (layer < 0) { layer = 0; }
		if (layer > HP_LAYERS - 1) { layer = HP_LAYERS - 1; }
		return layer;
	}

	// ‘w‚Ì’†‚Å‚ÌŠ„‡(0~1)
	float HpLayerFrac(float v, int layer, float layerHp)
	{
		return std::clamp((v - layer * layerHp) / layerHp, 0.0f, 1.0f);
	}
}

void HudDraw::DrawStringRight(int rightX, int y, const char* str, unsigned int color, int font)
{
	const int w = GetDrawStringWidthToHandle(str, (int)std::strlen(str), font);
	DrawStringToHandle(rightX - w, y, str, color, font);
}

// ‘w‚ÌF(‰º‚©‚ç‡)B1‘w–Ú(ÅŒã‚Ì1‘w)‚ªÔA5‘w–Ú(‚¢‚¿‚Î‚ñã)‚ª”Z‚¢—Î
unsigned int HudDraw::HpLayerColor(int layer)
{
	switch (layer)
	{
	case 0:  return GetColor(190, 35, 45);		// 1‘w–Ú: Ô
	case 1:  return GetColor(250, 235, 120);	// 2‘w–Ú: ”–‚¢‰©F
	case 2:  return GetColor(225, 175, 20);		// 3‘w–Ú: ”Z‚¢‰©F
	case 3:  return GetColor(140, 225, 110);	// 4‘w–Ú: ”–‚¢—Î
	default: return GetColor(25, 150, 55);		// 5‘w–Ú: ”Z‚¢—Î
	}
}

int HudDraw::DrawLayeredHp(float x, float y, float w, float h, float s,
	float hp, float delayedHp, float maxHp,
	unsigned int frameColor, float frameThick)
{
	const float layerHp = maxHp / (float)HP_LAYERS;
	const float right = x + w;

	const int curL = HpLayerOf(hp, layerHp);
	const float frac = (hp <= 0.0f) ? 0.0f : HpLayerFrac(hp, curL, layerHp);

	// ‰e
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
	DrawBoxAA(x - 3.0f * s, y - 3.0f * s, right + 3.0f * s, y + h + 3.0f * s,
		GetColor(10, 10, 14), TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	// ‹ó‚Ì•”•ª / ‰º‚Ì‘w(‚·‚®‰º‚Ì‘w‚ÌF‚ªA‘S•‚ÅŒ©‚¦‚Ä‚¢‚é)
	DrawBoxAA(x, y, right, y + h, GetColor(28, 30, 34), TRUE);

	if (curL > 0)
	{
		DrawBoxAA(x, y, right, y + h, HpLayerColor(curL - 1), TRUE);
	}

	// ƒ_ƒ[ƒW‚ÅŒ¸‚Á‚½•ª: –¾‚é‚¢Ô‚Ìƒo[‚ª’x‚ê‚Äk‚Ş
	if (delayedHp > hp)
	{
		const int dL = HpLayerOf(delayedHp, layerHp);
		const float dFrac = (dL == curL) ? HpLayerFrac(delayedHp, dL, layerHp) : 1.0f;

		if (dFrac > frac)
		{
			DrawBoxAA(right - w * dFrac, y, right - w * frac, y + h, GetColor(255, 85, 70), TRUE);
		}
	}

	// ¡‚Ì‘w
	if (frac > 0.0f)
	{
		DrawBoxAA(right - w * frac, y, right, y + h, HpLayerColor(curL), TRUE);

		// ã”¼•ª‚ÉŒõ‘ò
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 75);
		DrawBoxAA(right - w * frac, y, right, y + h * 0.45f, GetColor(255, 255, 255), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	}

	DrawBoxAA(x, y, right, y + h, frameColor, FALSE, frameThick);

	return (hp <= 0.0f) ? 0 : curL + 1;
}