#include "EnemyFactory.h"
#include "../Melee/MeleeEnemy.h"

namespace EnemyFactory
{
	std::unique_ptr<EnemyBase> Create(EnemyData::TYPE type, Player& player,
		int hp, const VECTOR* pos)
	{
		std::unique_ptr<EnemyBase> enemy;

		switch (type)
		{
		case EnemyData::TYPE::MELEE:
		default:
			enemy = std::make_unique<MeleeEnemy>(player);
			break;
		}

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
