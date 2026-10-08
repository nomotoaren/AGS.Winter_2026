#pragma once
#include <DxLib.h>

// 敵の範囲攻撃(爆発する球)
//   最初に「警告」として赤い球が出て、時間が来ると爆発する。プレイヤーはその間に範囲の外へ逃げる
//   当たったかの判定とダメージは、出した敵(MeleeEnemy)が行う
class EnemyAreaBlast
{
public:

	EnemyAreaBlast(void);
	~EnemyAreaBlast(void) = default;

	// pos: 中心 / radius: 爆発の半径 / warnTime: 警告から爆発までの時間(秒)
	void Init(const VECTOR& pos, float radius, float warnTime);

	void Update(float deltaTime);
	void Draw(void) const;

	// 爆発した瞬間に1回だけ true を返す(ダメージ判定をするタイミング)
	bool ConsumeExplode(void);

	bool IsDead(void) const { return state_ == State::DEAD; }
	const VECTOR& GetPos(void) const { return pos_; }
	float GetRadius(void) const { return radius_; }

private:

	enum class State
	{
		WARN,		// 警告中
		EXPLODE,	// 爆発の見た目を出している
		DEAD
	};

	// 爆発の見た目が残る時間(秒)
	static constexpr float EXPLODE_TIME = 0.35f;

	VECTOR pos_;
	float radius_;
	float warnTime_;
	float timer_;
	State state_;
	bool explodeFlag_;
};