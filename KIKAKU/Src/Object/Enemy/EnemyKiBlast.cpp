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
	isDead_(true)
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

	// 外側の光(半透明)
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90);
	DrawSphere3D(pos_, radius_ * 1.8f * wobble, 16, GetColor(170, 90, 255), GetColor(170, 90, 255), TRUE);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
	DrawSphere3D(pos_, radius_ * 1.3f * wobble, 16, GetColor(210, 150, 255), GetColor(210, 150, 255), TRUE);

	// 芯
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	DrawSphere3D(pos_, radius_ * 0.8f, 16, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);

	SetUseLighting(TRUE);
}