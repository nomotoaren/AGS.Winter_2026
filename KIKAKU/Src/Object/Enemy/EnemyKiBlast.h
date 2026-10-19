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

	// 追尾させる(target: 追う位置へのポインタ / offsetY: その位置に足す高さ / turnRate: 曲がる強さ(1秒あたり) / homingTime: 追尾する時間(秒))
	//   追尾時間が過ぎたら、その時の向きのまま直進する(避けられる)
	void SetHoming(const VECTOR* target, float offsetY, float turnRate, float homingTime);

	// 固定した位置へ曲がる(point: 狙う位置 / startDelay: 曲がり始めるまでの秒 / turnRate / endTime: 曲がるのをやめる時刻(秒))
	//   プレイヤーを追わないので、動いていれば当たらない
	void SetFixedGoal(const VECTOR& point, float startDelay, float turnRate, float endTime);
	// 追尾の開始を遅らせる(秒)
	void SetHomingDelay(float delay) { homingDelay_ = delay; }

	void Update(float deltaTime);
	void Draw(void) const;

	// 相殺勝負の対象になる大きな気弾か
	void SetBig(bool big) { isBig_ = big; }
	bool IsBig(void) const { return isBig_; }

	// 止める(相殺勝負中。動かず、消えず、描かれず、当たりもしない)
	void SetFrozen(bool frozen) { isFrozen_ = frozen; }
	bool IsFrozen(void) const { return isFrozen_; }

	// 当たったときなどに消す
	void Kill(void) { isDead_ = true; }

	// 当たったときのダメージ(省略すると3)
	void SetDamage(int damage) { damage_ = damage; }
	int GetDamage(void) const { return damage_; }

	// 当たったときにプレイヤーを押す強さ(省略すると12。連射の弾は0にして、押し出さない)
	void SetKnock(float knock) { knock_ = knock; }
	float GetKnock(void) const { return knock_; }

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
	float knock_ = 12.0f;
	bool isBig_ = false;
	bool isFrozen_ = false;

	// 追尾
	const VECTOR* homingTarget_ = nullptr;
	float homingOffsetY_ = 0.0f;
	float homingTurnRate_ = 0.0f;
	float homingTime_ = 0.0f;
	float homingDelay_ = 0.0f;
	bool useFixedGoal_ = false;
	VECTOR fixedGoal_ = VGet(0.0f, 0.0f, 0.0f);
};