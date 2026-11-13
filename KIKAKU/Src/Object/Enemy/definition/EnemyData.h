#pragma once

namespace EnemyData
{
	// 敵の種類
	enum class TYPE
	{
		MELEE,		// 格闘・気弾・ビームなどを使う敵(いまの MeleeEnemy)
		MAX
	};

	struct Info
	{
		const char* name;		// HPバーに出す名前
		int hp;					// 基本のHP
		float modelScale;		// モデルの大きさ
		float hitRadius;		// 攻撃を受ける半径
		float damageRate;		// この敵がプレイヤーに与えるダメージの倍率(1.0 が基準)
	};

	inline const Info& Get(TYPE type)
	{
		// TYPE の並びと同じ順に書く
		static const Info table[(int)TYPE::MAX] =
		{
			{ "ENEMY", 10000, 0.8f, 50.0f, 1.0f },	// MELEE
		};

		int index = (int)type;

		if (index < 0 || index >= (int)TYPE::MAX)
		{
			index = 0;
		}

		return table[index];
	}
}
