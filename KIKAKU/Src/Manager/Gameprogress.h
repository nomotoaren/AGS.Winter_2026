#pragma once
#include <fstream>

// プレイヤーの成長とステージの進み具合(シーンを切り替えても残る)
//   ・レベル / 経験値
//   ・クリアしたステージ(クリアすると次のステージが選べる)
//   ・今選んでいるステージ
//   ファイル(save.dat)へ保存・読み込みする
namespace GameProgress
{
	// ステージの数
	constexpr int STAGE_MAX = 3;

	// レベルの上限
	constexpr int LEVEL_MAX = 10;

	// レベルが1上がるごとに、攻撃力が増える割合(0.10 = 10%)
	constexpr float ATTACK_RATE_PER_LEVEL = 0.10f;

	// 保存先
	constexpr const char* SAVE_FILE = "save.dat";

	struct Data
	{
		int level = 1;
		int exp = 0;					// 今のレベルの中で溜まっている経験値
		bool cleared[STAGE_MAX] = {};
		int selectedStage = 0;			// 選んだステージ(0 始まり)
	};

	// 全体で1つ
	inline Data& Get(void)
	{
		static Data data;
		return data;
	}

	// 次のレベルまでに必要な経験値
	inline int ExpToNext(int level)
	{
		return 100 * level;
	}

	// 攻撃力の倍率(レベル1で 1.0)
	inline float AttackRate(void)
	{
		return 1.0f + ATTACK_RATE_PER_LEVEL * static_cast<float>(Get().level - 1);
	}

	// そのステージを選べるか(最初のステージか、1つ前をクリアしていれば選べる)
	inline bool IsUnlocked(int stage)
	{
		if (stage < 0 || stage >= STAGE_MAX)
		{
			return false;
		}

		return stage == 0 || Get().cleared[stage - 1];
	}

	// 経験値を足す。上がったレベルの数を返す
	inline int AddExp(int amount)
	{
		Data& d = Get();
		int up = 0;

		d.exp += amount;

		while (d.level < LEVEL_MAX && d.exp >= ExpToNext(d.level))
		{
			d.exp -= ExpToNext(d.level);
			d.level++;
			up++;
		}

		// 上限に着いたら、溜めても意味がない
		if (d.level >= LEVEL_MAX)
		{
			d.exp = 0;
		}

		return up;
	}

	// ステージをクリアしたとき(経験値も足す)
	inline int OnStageCleared(int stage, int expReward)
	{
		if (stage >= 0 && stage < STAGE_MAX)
		{
			Get().cleared[stage] = true;
		}

		return AddExp(expReward);
	}

	// 保存
	inline void Save(void)
	{
		std::ofstream file(SAVE_FILE);

		if (!file)
		{
			return;
		}

		const Data& d = Get();

		file << d.level << " " << d.exp;

		for (int i = 0; i < STAGE_MAX; i++)
		{
			file << " " << (d.cleared[i] ? 1 : 0);
		}

		file << "\n";
	}

	// 読み込み(ファイルがなければ、最初の状態のまま)
	inline void Load(void)
	{
		Data d;

		std::ifstream file(SAVE_FILE);

		if (!file)
		{
			Get() = d;
			return;
		}

		file >> d.level >> d.exp;

		for (int i = 0; i < STAGE_MAX; i++)
		{
			int c = 0;
			file >> c;
			d.cleared[i] = (c != 0);
		}

		// 壊れたファイルでも、おかしな値にならないようにする
		if (!file)
		{
			d = Data();
		}

		if (d.level < 1) { d.level = 1; }
		if (d.level > LEVEL_MAX) { d.level = LEVEL_MAX; }
		if (d.exp < 0) { d.exp = 0; }

		Get() = d;
	}

	// 最初からやり直す(セーブも消える)
	inline void Reset(void)
	{
		Get() = Data();
		Save();
	}
}