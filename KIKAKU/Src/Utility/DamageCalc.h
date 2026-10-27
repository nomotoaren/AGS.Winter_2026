#pragma once
#include <cmath>
#include <DxLib.h>

// ダメージのぶれ・クリティカルを決める(ヘッダだけで完結)
//   使い方:  const auto r = DamageCalc::Roll(基本ダメージ);
//            r.damage   = 最終的なダメージ(最低1)
//            r.critical = クリティカルだったか
namespace DamageCalc
{
	// ダメージのぶれ(0.10 = ±10%)。0 にすると毎回同じ数字になる
	constexpr float VARIANCE = 0.10f;

	// クリティカルが出る確率(0.10 = 10%)
	constexpr float CRITICAL_CHANCE = 0.10f;

	// クリティカルのダメージ倍率
	constexpr float CRITICAL_RATE = 1.5f;

	// クリティカル時に、ヒットストップへ足す時間
	constexpr float CRITICAL_HIT_STOP = 0.04f;

	// 数字を大きく見せるための倍率
	constexpr int DEALT_SCALE = 100;	// 敵に与えるダメージ
	constexpr int TAKEN_SCALE = 10;		// 自分が受けるダメージ

	struct Result
	{
		int damage;
		bool critical;
	};

	// 元のダメージ(倍率をかけ終わった値)に、ぶれとクリティカルをかける
	inline Result Roll(float baseDamage)
	{
		// -VARIANCE ～ +VARIANCE の間でランダムに選ぶ
		const float random01 = static_cast<float>(GetRand(10000)) / 10000.0f;
		const float variance = 1.0f + (random01 * 2.0f - 1.0f) * VARIANCE;

		// 0 ～ 9999 のうち、確率の分だけがクリティカル
		const bool critical =
			GetRand(9999) < static_cast<int>(CRITICAL_CHANCE * 10000.0f);

		float damage = baseDamage * static_cast<float>(DEALT_SCALE) * variance;

		if (critical)
		{
			damage *= CRITICAL_RATE;
		}

		Result result;
		result.damage = static_cast<int>(std::lround(damage));
		result.critical = critical;

		if (result.damage < 1)
		{
			result.damage = 1;
		}

		return result;
	}
}