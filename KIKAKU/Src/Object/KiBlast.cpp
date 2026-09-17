#include "KiBlast.h"

KiBlast::KiBlast(VECTOR pos, VECTOR dir)
{
	pos_ = pos;
	dir_ = dir;

	speed_ = 25.0f;
	lifeTime_ = 3.0f;

	isDead_ = false;

	if (VSize(dir_) > 0.001f)
	{
		dir_ = VNorm(dir_);
	}
}

KiBlast::~KiBlast(void)
{
}

void KiBlast::Update(void)
{
	if (isDead_)
	{
		return;
	}

	pos_ =
		VAdd(
			pos_,
			VScale(
				dir_,
				speed_
			)
		);

	lifeTime_ -=
		1.0f / 60.0f;

	if (lifeTime_ <= 0.0f)
	{
		isDead_ = true;
	}
}

void KiBlast::Draw(void)
{
	if (isDead_)
	{
		return;
	}

	DrawSphere3D(
		pos_,
		12.0f,
		16,
		GetColor(80, 180, 255),
		GetColor(255, 255, 255),
		true
	);
}

bool KiBlast::IsDead(void) const
{
	return isDead_;
}

VECTOR KiBlast::GetPos(void) const
{
	return pos_;
}

void KiBlast::Hit(void)
{
	isDead_ = true;
}