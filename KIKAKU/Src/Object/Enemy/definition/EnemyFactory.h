#pragma once
#include <memory>
#include "../EnemyBase.h"
#include "EnemyData.h"

class Player;

namespace EnemyFactory
{
		std::unique_ptr<EnemyBase> Create(EnemyData::TYPE type, Player& player,
		int hp = 0, const VECTOR* pos = nullptr);
}
