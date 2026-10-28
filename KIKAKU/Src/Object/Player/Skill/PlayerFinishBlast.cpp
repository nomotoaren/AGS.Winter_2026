#include <cmath>
#include "../definition/PlayerConfig.h"
#include "PlayerFinishBlast.h"

namespace cfg = PlayerConfig;

PlayerFinishBlast::PlayerFinishBlast(void)
	:
	pos_(VGet(0.0f, 0.0f, 0.0f)),
	dir_(VGet(0.0f, 0.0f, 1.0f)),
	time_(0.0f),
	isDead_(true)
{
}

void PlayerFinishBlast::Init(const VECTOR& pos, const VECTOR& dir)
{
	pos_ = pos;
	dir_ = (VSize(dir) > 0.001f) ? VNorm(dir) : VGet(0.0f, 0.0f, 1.0f);
	time_ = 0.0f;
	isDead_ = false;
}

float PlayerFinishBlast::GetRadius(void) const
{
	// èoénÇﬂÇÕè¨Ç≥Ç≠ÅAÇ∑ÇÆëÂÇ´Ç≠Ç»ÇÈ
	float rate = time_ / 0.2f;
	if (rate > 1.0f) { rate = 1.0f; }

	return cfg::Finish::RADIUS * (0.4f + 0.6f * rate);
}

void PlayerFinishBlast::Update(float deltaTime, const VECTOR* aimPos)
{
	if (isDead_)
	{
		return;
	}

	time_ += deltaTime;

	if (time_ >= cfg::Finish::LIFE)
	{
		isDead_ = true;
		return;
	}

	// ë_Ç¡ÇƒÇ¢ÇÈìGÇÃï˚Ç÷è≠ÇµÇ∏Ç¬ã»Ç™ÇÈ
	if (aimPos != nullptr && time_ < cfg::Finish::HOMING_TIME)
	{
		const VECTOR toAim = VSub(*aimPos, pos_);

		if (VSize(toAim) > 0.001f)
		{
			float rate = cfg::Finish::TURN * deltaTime;
			if (rate > 1.0f) { rate = 1.0f; }

			const VECTOR mixed = VAdd(dir_, VScale(VSub(VNorm(toAim), dir_), rate));

			if (VSize(mixed) > 0.001f)
			{
				dir_ = VNorm(mixed);
			}
		}
	}

	pos_ = VAdd(pos_, VScale(dir_, cfg::Finish::SPEED * deltaTime * 60.0f));
}

void PlayerFinishBlast::Draw(void) const
{
	if (isDead_)
	{
		return;
	}

	const float r = GetRadius();
	const float wobble = 1.0f + 0.10f * sinf(time_ * 32.0f);

	SetUseLighting(FALSE);

	// îˆ
	{
		const VECTOR tail = VSub(pos_, VScale(dir_, cfg::Finish::SPEED * 10.0f));

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 150);
		DrawCapsule3D(pos_, tail, r * 0.9f, 8, GetColor(60, 150, 255), GetColor(60, 150, 255), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 210);
		DrawCapsule3D(pos_, tail, r * 0.4f, 8, GetColor(220, 245, 255), GetColor(220, 245, 255), TRUE);
	}

	// äOë§ÇÃåı
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 150);
	DrawSphere3D(pos_, r * 2.0f * wobble, 16, GetColor(40, 120, 255), GetColor(40, 120, 255), TRUE);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 220);
	DrawSphere3D(pos_, r * 1.4f * wobble, 16, GetColor(110, 190, 255), GetColor(110, 190, 255), TRUE);

	// êc
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	DrawSphere3D(pos_, r * 0.85f, 16, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);

	SetUseLighting(TRUE);
}