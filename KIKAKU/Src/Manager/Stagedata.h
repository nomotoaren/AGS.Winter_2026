#pragma once
#include "GameProgress.h"
#include "../Object/Enemy/definition/EnemyData.h"

// ステージごとの設定
namespace StageData
{
	// 1ステージに出せる敵の数の上限
	constexpr int SPAWN_MAX = 4;

	// 出す敵1体ぶんの設定
	struct Spawn
	{
		EnemyData::TYPE type;	// 敵の種類
		int hp;					// このステージでのHP
		float x, y, z;			// 出す位置
	};

	struct Info
	{
		const char* name;		// ステージ名
		int recommendLv;		// 推奨レベル
		int expReward;			// クリアでもらえる経験値
		int spawnCount;			// 出す敵の数
		Spawn spawns[SPAWN_MAX];	// 出す敵
	};

	inline const Info& Get(int stage)
	{
		static const Info table[GameProgress::STAGE_MAX] =
		{
			// ステージ名       推奨Lv  経験値  数   { 種類,					HP,   x,    y,      z }
			{ "ステージ01",       1,    120,   1, { { EnemyData::TYPE::MELEE,  6000, 0.0f, 1000.0f, 300.0f } } },
			{ "ステージ02",		  3,    200,   1, { { EnemyData::TYPE::MELEE, 10000, 0.0f, 1000.0f, 300.0f } } },
			{ "ステージ03",       4,    300,   1, { { EnemyData::TYPE::MELEE, 20000, 0.0f, 1000.0f, 300.0f } } },
		};

		if (stage < 0) { stage = 0; }
		if (stage >= GameProgress::STAGE_MAX) { stage = GameProgress::STAGE_MAX - 1; }

		return table[stage];
	}
}
