#include <cmath>
#include "EnemyKiBlast.h"

EnemyKiBlast::EnemyKiBlast(void)
	:
	pos_(VGet(0.0f, 0.0f, 0.0f)),
	dir_(VGet(0.0f, 0.0f, 1.0f)),
	speed_(0.0f),
	radius_(10.0f),
	life_(0.0f),
	time_(0.0f),
	isDead_(true),
	damage_(3)
{
}

void EnemyKiBlast::Init(const VECTOR& pos, const VECTOR& dir, float speed, float radius, float life)
{
	pos_ = pos;
	dir_ = (VSize(dir) > 0.001f) ? VNorm(dir) : VGet(0.0f, 0.0f, 1.0f);
	speed_ = speed;
	radius_ = radius;
	life_ = life;
	time_ = 0.0f;
	isDead_ = false;
}

void EnemyKiBlast::SetHoming(const VECTOR* target, float offsetY, float turnRate, float homingTime)
{
	homingTarget_ = target;
	homingOffsetY_ = offsetY;
	homingTurnRate_ = turnRate;
	homingTime_ = homingTime;
}

void EnemyKiBlast::SetFixedGoal(const VECTOR& point, float startDelay, float turnRate, float endTime)
{
	useFixedGoal_ = true;
	fixedGoal_ = point;
	homingDelay_ = startDelay;
	homingTurnRate_ = turnRate;
	homingTime_ = endTime;
}

void EnemyKiBlast::Update(float deltaTime)
{
	if (isDead_)
	{
		return;
	}

	time_ += deltaTime;

	if (time_ >= life_)
	{
		isDead_ = true;
		return;
	}

	// 追尾: 遅れて始まり、追尾時間のあいだ向きを目標へ少しずつ曲げる(固定位置を狙う弾もある)
	const bool homing = (homingTarget_ != nullptr || useFixedGoal_);

	if (homing && time_ >= homingDelay_ && time_ < homingTime_)
	{
		const VECTOR goal = useFixedGoal_
			? fixedGoal_
			: VAdd(*homingTarget_, VGet(0.0f, homingOffsetY_, 0.0f));
		const VECTOR toGoal = VSub(goal, pos_);

		if (VSize(toGoal) > 0.001f)
		{
			float rate = homingTurnRate_ * deltaTime;
			if (rate > 1.0f) { rate = 1.0f; }

			const VECTOR mixed = VAdd(dir_, VScale(VSub(VNorm(toGoal), dir_), rate));

			if (VSize(mixed) > 0.001f)
			{
				dir_ = VNorm(mixed);
			}
		}
	}

	// 速さは「1/60秒あたり」なので、フレーム時間に合わせて直す
	pos_ = VAdd(pos_, VScale(dir_, speed_ * deltaTime * 60.0f));
}

void EnemyKiBlast::Draw(void) const
{
	if (isDead_)
	{
		return;
	}

	// ゆらゆらと大きさを変えて、エネルギーの塊に見せる
	const float wobble = 1.0f + 0.12f * sinf(time_ * 30.0f);

	SetUseLighting(FALSE);

	// 尾(進んできた方向へ伸びる光の筋)。速い弾ほど長く見える
	{
		const VECTOR tail = VSub(pos_, VScale(dir_, speed_ * 9.0f));

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 150);
		DrawCapsule3D(pos_, tail, radius_ * 0.9f, 8, GetColor(230, 60, 255), GetColor(230, 60, 255), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 200);
		DrawCapsule3D(pos_, tail, radius_ * 0.4f, 8, GetColor(255, 220, 255), GetColor(255, 220, 255), TRUE);
	}

	// 外側の光(半透明)
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
	DrawSphere3D(pos_, radius_ * 1.8f * wobble, 16, GetColor(150, 30, 255), GetColor(150, 30, 255), TRUE);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 230);
	DrawSphere3D(pos_, radius_ * 1.3f * wobble, 16, GetColor(210, 90, 255), GetColor(210, 90, 255), TRUE);

	// 芯
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	DrawSphere3D(pos_, radius_ * 0.8f, 16, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);

	SetUseLighting(TRUE);
}