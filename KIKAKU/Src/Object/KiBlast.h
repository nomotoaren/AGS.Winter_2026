#pragma once
#include <DxLib.h>

class KiBlast
{
public:

	// homing: true なら、SetTarget で教えられた位置へ曲がる
	KiBlast(VECTOR pos, VECTOR dir, bool homing = false);
	~KiBlast(void);

	// 追尾する位置を教える(毎フレーム、Update の前に呼ぶ)
	void SetTarget(const VECTOR& targetPos, bool hasTarget);

	void Update(void);
	void Draw(void)const;

	bool IsDead(void) const;

	VECTOR GetPos(void) const;
	void Hit(void);

private:

	// 向きを、目標の向きへ上限つきで曲げる
	void SteerToTarget(float deltaTime);

	VECTOR pos_ = {};
	VECTOR dir_ = {};

	float speed_ = 0.0f;
	float lifeTime_ = 0.0f;
	float age_ = 0.0f;

	bool isDead_ = false;

	bool homing_ = false;
	bool lostTarget_ = false;	// 追い越したら、追尾をやめる
	bool hasTarget_ = false;
	VECTOR targetPos_ = {};
};