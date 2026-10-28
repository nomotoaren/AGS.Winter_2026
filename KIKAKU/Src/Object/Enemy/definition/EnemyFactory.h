#pragma once
#include <memory>
#include "../EnemyBase.h"
#include "EnemyData.h"

class Player;

// 敵を作る係
//   敵の種類(EnemyData::TYPE)から、どのクラスを作るかを決める。
//   新しい敵クラスを作ったら、EnemyFactory.cpp の switch に1行足す
namespace EnemyFactory
{
	// 敵を作って、Init まで済ませて返す
	//   hp    : 0 より大きければ、種類ごとの基本HPの代わりにこのHPにする(ステージごとの強さ調整用)
	//   pos   : 出す位置(nullptr なら、その敵クラスの初期位置)
	std::unique_ptr<EnemyBase> Create(EnemyData::TYPE type, Player& player,
		int hp = 0, const VECTOR* pos = nullptr);
}
