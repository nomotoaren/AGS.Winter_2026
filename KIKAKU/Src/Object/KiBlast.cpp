#include "../Manager/SceneManager.h"
#include "KiBlast.h"

namespace
{
	// 1ƒtƒŒ[ƒ€‚ ‚½‚è‚ÌˆÚ“®—Ê‚ÉŠ|‚¯‚é”{—¦
	//   (’e‚Ì‘¬‚³‚Í 144fps ‚Ì1ƒtƒŒ[ƒ€•ª‚ÅŒˆ‚ß‚Ä‚ ‚é‚Ì‚ÅA60fps ‚È‚ç–ñ2.4”{‚É‚µ‚Ä“¯‚¶‘¬‚³‚É‚·‚é)
	float FrameScale(void)
	{
		float s = SceneManager::GetInstance().GetFrameScale();

		// ˆ——Ž‚¿‚µ‚½1ƒtƒŒ[ƒ€‚ÅA”ò‚Ñ‚·‚¬‚È‚¢‚æ‚¤‚É‚·‚é
		if (s > 4.0f)
		{
			s = 4.0f;
		}

		return s;
	}
}

KiBlast::KiBlast(VECTOR pos, VECTOR dir)
{
	pos_ = pos;
	dir_ = dir;

	speed_ = 25.0f;

	// ”ò‚ñ‚Å‚¢‚éŽžŠÔ(•b)
	lifeTime_ = 1.25f;

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
				speed_ * FrameScale()
			)
		);

	lifeTime_ -=
		SceneManager::GetInstance().GetDeltaTime();

	if (lifeTime_ <= 0.0f)
	{
		isDead_ = true;
	}
}

void KiBlast::Draw(void)const
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