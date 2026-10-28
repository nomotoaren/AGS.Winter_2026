#include "EnemyFactory.h"
#include "../Melee/MeleeEnemy.h"

namespace EnemyFactory
{
	std::unique_ptr<EnemyBase> Create(EnemyData::TYPE type, Player& player,
		int hp, const VECTOR* pos)
	{
		std::unique_ptr<EnemyBase> enemy;

		// 新しい敵クラスは、ここに case を足す
		switch (type)
		{
		case EnemyData::TYPE::MELEE:
		default:
			enemy = std::make_unique<MeleeEnemy>(player);
			break;
		}

		// 種類ごとのデータを反映してから、ステージ用のHPで上書きする(Init の前に決める)
		enemy->ApplyInfo(EnemyData::Get(type));

		if (hp > 0)
		{
			enemy->SetMaxHp(hp);
		}

		enemy->Init();

		if (pos != nullptr)
		{
			enemy->SetPosition(*pos);
		}

		return enemy;
	}
}
