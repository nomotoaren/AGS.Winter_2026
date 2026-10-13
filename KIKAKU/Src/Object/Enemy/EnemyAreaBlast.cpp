#include <cmath>
#include "EnemyAreaBlast.h"

EnemyAreaBlast::EnemyAreaBlast(void)
	:
	pos_(VGet(0.0f, 0.0f, 0.0f)),
	radius_(100.0f),
	warnTime_(1.0f),
	timer_(0.0f),
	state_(State::DEAD),
	explodeFlag_(false),
	damage_(6)
{
}

void EnemyAreaBlast::Init(const VECTOR& pos, float radius, float warnTime, int damage)
{
	pos_ = pos;
	radius_ = radius;
	warnTime_ = (warnTime > 0.01f) ? warnTime : 0.01f;
	timer_ = 0.0f;
	state_ = State::WARN;
	explodeFlag_ = false;
	damage_ = damage;
}

void EnemyAreaBlast::Update(float deltaTime)
{
	if (state_ == State::DEAD)
	{
		return;
	}

	timer_ += deltaTime;

	if (state_ == State::WARN && timer_ >= warnTime_)
	{
		state_ = State::EXPLODE;
		timer_ = 0.0f;
		explodeFlag_ = true;
		return;
	}

	if (state_ == State::EXPLODE && timer_ >= EXPLODE_TIME)
	{
		state_ = State::DEAD;
	}
}

bool EnemyAreaBlast::ConsumeExplode(void)
{
	const bool result = explodeFlag_;
	explodeFlag_ = false;
	return result;
}

void EnemyAreaBlast::Draw(void) const
{
	if (state_ == State::DEAD)
	{
		return;
	}

	SetUseLighting(FALSE);

	if (state_ == State::WARN)
	{
		// 0.0(åxçêÇ™èoÇΩíºå„) Å® 1.0(îöî≠Ç∑ÇÈíºëO)
		const float rate = timer_ / warnTime_;

		// îöî≠Ç™ãﬂÇ¢ÇŸÇ«ë¨Ç≠ì_ñ≈Ç∑ÇÈ
		const float pulse = 0.5f + 0.5f * sinf(timer_ * (10.0f + 30.0f * rate));

		// îÕàÕÇÃòg(ÉèÉCÉÑÅ[)
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
		DrawSphere3D(pos_, radius_, 20, GetColor(255, 230, 0), GetColor(255, 230, 0), FALSE);

		// îÕàÕÇÃíÜÇîñÇ≠ìhÇÈ
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90 + (int)(50.0f * pulse));
		DrawSphere3D(pos_, radius_, 20, GetColor(255, 0, 0), GetColor(255, 0, 0), TRUE);

		// íÜÇ©ÇÁçLÇ™ÇÈãÖ(Ç±ÇÍÇ™äOògÇ…ìÕÇ≠Ç∆îöî≠)
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170 + (int)(60.0f * pulse));
		DrawSphere3D(pos_, radius_ * rate, 20, GetColor(255, 100, 0), GetColor(255, 100, 0), TRUE);

		// îöî≠ÇÃíºëOÇÕîíÇ≠åıÇÈ
		if (rate > 0.8f)
		{
			SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(120.0f * (rate - 0.8f) / 0.2f));
			DrawSphere3D(pos_, radius_, 20, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);
		}
	}
	else
	{
		// îöî≠: çLÇ™ÇËÇ»Ç™ÇÁè¡Ç¶ÇÈ
		const float t = timer_ / EXPLODE_TIME;
		const float r = radius_ * (0.5f + 0.6f * t);
		const int alpha = (int)(210.0f * (1.0f - t));

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
		DrawSphere3D(pos_, r, 20, GetColor(255, 170, 60), GetColor(255, 170, 60), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
		DrawSphere3D(pos_, r * 0.6f, 20, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);
	}

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	SetUseLighting(TRUE);
}