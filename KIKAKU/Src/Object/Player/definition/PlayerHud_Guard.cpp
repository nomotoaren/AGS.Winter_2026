// プレイヤーHUD: ガードゲージ(耐久値の表示と、ガードブレイク中の表示)
#include <cmath>
#include <algorithm>
#include <DxLib.h>
#include "../../../Manager/SceneManager.h"
#include "../Player.h"
#include "PlayerConfig.h"
#include "../../Common/HudDraw.h"
#include "PlayerHud.h"

using HudDraw::DrawStringRight;

// ガードゲージ
//   ガード中・耐久値が減っている間・ガードブレイク中だけ出す(満タンで構えていない時は出さない)
//   ブレイク中は、動けるようになるまでの残り時間を、赤いゲージで減らしていく
void PlayerHud::DrawGuardGauge(const Player& player, float x, float y, float w, float h, float s) const
{
	const bool isBreak = player.IsGuardBreak();
	const float hpRate = std::clamp(player.GetGuardRate(), 0.0f, 1.0f);

	if (!isBreak && !player.IsGuard() && hpRate >= 1.0f)
	{
		return;
	}

	float rate = hpRate;
	unsigned int color = GetColor(90, 200, 255);
	unsigned int labelColor = GetColor(200, 235, 255);
	const char* label = "ガード";

	if (isBreak)
	{
		// ガードブレイク中: 赤く点滅。ゲージは動けるまでの残り時間
		const float blink = 0.55f + 0.45f * sinf(time_ * 14.0f);

		rate = std::clamp(player.GetGuardBreakRate(), 0.0f, 1.0f);
		color = GetColor((int)(255.0f * blink), 60, 50);
		labelColor = GetColor(255, (int)(120.0f + 100.0f * blink), (int)(100.0f * blink));
		label = "ガードブレイク!";
	}
	else if (hpRate <= 0.25f)
	{
		// 壊れそう
		const float blink = 0.6f + 0.4f * sinf(time_ * 10.0f);

		color = GetColor(255, (int)(120.0f + 60.0f * blink), 40);
		labelColor = GetColor(255, 200, 120);
	}
	else if (hpRate <= 0.5f)
	{
		color = GetColor(255, 215, 80);
	}

	// ラベル(ゲージの上、右寄せ)
	DrawStringRight((int)(x + w), (int)(y - 24.0f * s), label, labelColor, fontSmall_);

	// 影
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
	DrawBoxAA(x - 3.0f * s, y - 3.0f * s, x + w + 3.0f * s, y + h + 3.0f * s,
		GetColor(10, 10, 14), TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	// 空の部分
	DrawBoxAA(x, y, x + w, y + h, GetColor(28, 30, 34), TRUE);

	// 現在値(右から左へ減る。気ゲージと同じ向き)
	if (rate > 0.0f)
	{
		const float right = x + w;

		DrawBoxAA(right - w * rate, y, right, y + h, color, TRUE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 75);
		DrawBoxAA(right - w * rate, y, right, y + h * 0.45f, GetColor(255, 255, 255), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	}

	DrawBoxAA(x, y, x + w, y + h, isBreak ? GetColor(255, 90, 70) : GetColor(165, 135, 70), FALSE, 2.0f * s);
}