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

	if (state_ == State::EXPLODE && timer_ >= GetExplodeTime())
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
	SetWriteZBuffer3D(FALSE);

	if (state_ == State::WARN)
	{
		// 0.0(åxçêÇ™èoÇΩíºå„) Å® 1.0(îöî≠Ç∑ÇÈíºëO)
		const float rate = timer_ / warnTime_;

		// îöî≠Ç™ãﬂÇ¢ÇŸÇ«ë¨Ç≠ì_ñ≈Ç∑ÇÈ
		const float pulse = 0.5f + 0.5f * sinf(timer_ * (10.0f + 30.0f * rate));

		// îÕàÕÇÃíÜÇÇ§Ç¡Ç∑ÇÁìhÇÈ(ÉJÉÅÉâÇ™íÜÇ…ì¸Ç¡ÇƒÇ‡âÊñ Ç™ñÑÇ‹ÇÁÇ»Ç¢îZÇ≥)
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 28 + (int)(22.0f * pulse));
		DrawSphere3D(pos_, radius_, 24, GetColor(255, 40, 20), GetColor(255, 40, 20), TRUE);

		// íÜÇ©ÇÁçLÇ™ÇÈãÖ(Ç±ÇÍÇ™äOògÇ…ìÕÇ≠Ç∆îöî≠)
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 60 + (int)(40.0f * pulse));
		DrawSphere3D(pos_, radius_ * rate, 24, GetColor(255, 120, 30), GetColor(255, 120, 30), TRUE);

		// îöî≠ÇÃíºëOÇÕîíÇ≠åıÇÈ(é„Çﬂ)
		if (rate > 0.8f)
		{
			SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(50.0f * (rate - 0.8f) / 0.2f));
			DrawSphere3D(pos_, radius_ * rate, 24, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);
		}
	}
	else
	{
		// îöî≠: çLÇ™ÇËÇ»Ç™ÇÁè¡Ç¶ÇÈ
		const float t = timer_ / GetExplodeTime();

		if (isHuge_)
		{
			// í¥ãêëÂ: àÍãCÇ…ñcÇÁÇ›(ç≈èâÇÃñÒ3ï™ÇÃ1ÇÃéûä‘Ç≈)ÅAÇªÇÃÇ†Ç∆îñÇÍÇ»Ç™ÇÁè≠ÇµÇ∏Ç¬çLÇ™ÇÈ
			float g = t * 3.0f;
			if (g > 1.0f) { g = 1.0f; }
			const float e = 1.0f - (1.0f - g) * (1.0f - g);

			const float r = radius_ * (0.25f + 0.85f * e);
			const int alpha = (int)(230.0f * (1.0f - t));

			// íxÇÍÇƒçLÇ™ÇÈê‘Ç¢äOäk
			if (t > 0.15f)
			{
				const float t2 = (t - 0.15f) / 0.85f;
				SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(110.0f * (1.0f - t2)));
				DrawSphere3D(pos_, radius_ * (0.6f + 0.7f * t2), 24, GetColor(255, 60, 20), GetColor(255, 60, 20), TRUE);
			}

			SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
			DrawSphere3D(pos_, r, 28, GetColor(255, 140, 40), GetColor(255, 140, 40), TRUE);

			SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
			DrawSphere3D(pos_, r * 0.78f, 28, GetColor(255, 220, 90), GetColor(255, 220, 90), TRUE);

			// íÜêSÇÃîíÇ¢åı(í∑Ç≠écÇÈ)
			SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(255.0f * (1.0f - t * 0.8f)));
			DrawSphere3D(pos_, r * 0.5f, 24, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);
		}
		else
		{
			const float r = radius_ * (0.5f + 0.6f * t);
			const int alpha = (int)(210.0f * (1.0f - t));

			SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
			DrawSphere3D(pos_, r, 20, GetColor(255, 170, 60), GetColor(255, 170, 60), TRUE);

			SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
			DrawSphere3D(pos_, r * 0.6f, 20, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);
		}
	}

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	SetWriteZBuffer3D(TRUE);
	SetUseLighting(TRUE);
}