#pragma once
#include "GameProgress.h"
#include "../Object/Enemy/definition/EnemyData.h"

// ステージごとの設定(ここの数字を変えるだけで、難しさや報酬を調整できる)
namespace StageData
{
	// 1ステージに出せる敵の数の上限
	constexpr int SPAWN_MAX = 4;

	// 出す敵1体ぶんの設定
	struct Spawn
	{
		EnemyData::TYPE type;	// 敵の種類(EnemyData.h)
		int hp;					// このステージでのHP(0 なら、種類ごとの基本HP)
		float x, y, z;			// 出す位置
	};

	struct Info
	{
		const char* name;		// ステージ名
		int recommendLv;		// 推奨レベル(これ未満でも挑戦はできる)
		int expReward;			// クリアでもらえる経験値
		int spawnCount;			// 出す敵の数
		Spawn spawns[SPAWN_MAX];	// 出す敵(spawnCount 体ぶんだけ使う)
	};

	inline const Info& Get(int stage)
	{
		static const Info table[GameProgress::STAGE_MAX] =
		{
			// ステージ名       推奨Lv  経験値  数   { 種類,                 HP,   x,    y,      z }
			{ "野本の家",       1,    120,   1, { { EnemyData::TYPE::MELEE,  6000, 0.0f, 1000.0f, 300.0f } } },
			{ "野本の部屋",             3,    200,   1, { { EnemyData::TYPE::MELEE, 10000, 0.0f, 1000.0f, 300.0f } } },
			{ "野本の寝床",           4,    300,   1, { { EnemyData::TYPE::MELEE, 20000, 0.0f, 1000.0f, 300.0f } } },
		};

		if (stage < 0) { stage = 0; }
		if (stage >= GameProgress::STAGE_MAX) { stage = GameProgress::STAGE_MAX - 1; }

		return table[stage];
	}
}
