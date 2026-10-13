#pragma once
#include <DxLib.h>

// 敵が撃つ気弾
//   まっすぐ飛び、一定時間で消える。プレイヤーに当たったかの判定とダメージは、撃った敵(MeleeEnemy)が行う
class EnemyKiBlast
{
public:

	EnemyKiBlast(void);
	~EnemyKiBlast(void) = default;

	// pos: 出す位置 / dir: 飛ぶ向き / speed: 1/60秒あたりの移動量 / radius: 大きさ / life: 消えるまでの時間(秒)
	void Init(const VECTOR& pos, const VECTOR& dir, float speed, float radius, float life);

	void Update(float deltaTime);
	void Draw(void) const;

	// 当たったときなどに消す
	void Kill(void) { isDead_ = true; }

	// 当たったときのダメージ(省略すると3)
	void SetDamage(int damage) { damage_ = damage; }
	int GetDamage(void) const { return damage_; }

	bool IsDead(void) const { return isDead_; }
	const VECTOR& GetPos(void) const { return pos_; }
	float GetRadius(void) const { return radius_; }

private:

	VECTOR pos_;
	VECTOR dir_;
	float speed_;
	float radius_;
	float life_;
	float time_;
	bool isDead_;
	int damage_;
};