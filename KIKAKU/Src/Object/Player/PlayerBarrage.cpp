#include <algorithm>
#include <cmath>
#include "../../Application.h"
#include "PlayerConfig.h"
#include "PlayerBarrage.h"

namespace cfg = PlayerConfig;

namespace
{
	VECTOR NormalizeOr(const VECTOR& v, const VECTOR& fallback)
	{
		return (VSize(v) > 0.001f) ? VNorm(v) : fallback;
	}

	// îöî≠ÇµÇƒÇ©ÇÁÅAÉQÅ[ÉÄÉVÅ[ÉìÇ™èàóùÇ∑ÇÈÇ‹Ç≈ÇÃóPó\(ïb)
	constexpr float EXPLODE_GRACE = 0.2f;
}

PlayerBarrage::PlayerBarrage(void)
{
}

PlayerBarrage::~PlayerBarrage(void)
{
}

void PlayerBarrage::Fire(const VECTOR& pos, const VECTOR& target, const VECTOR& arc)
{
	Shot s{};
	s.start = pos;
	s.target = target;
	s.arc = arc;
	s.pos = pos;
	s.dir = NormalizeOr(VSub(target, pos), VGet(0.0f, 0.0f, 1.0f));
	s.age = 0.0f;

	const float distance = VSize(VSub(target, pos));

	float flight = distance / cfg::Barrage::SPEED;

	if (flight < cfg::Barrage::MIN_FLIGHT)
	{
		flight = cfg::Barrage::MIN_FLIGHT;
	}

	s.flightTime = flight;
	s.life = cfg::Barrage::LIFE;
	s.exploded = false;
	s.handled = false;
	s.dead = false;
	shots_.push_back(s);
}

void PlayerBarrage::Update(float deltaTime)
{
	for (auto& s : shots_)
	{
		if (s.dead)
		{
			continue;
		}

		s.life -= deltaTime;

		if (s.life <= 0.0f)
		{
			s.dead = true;
			continue;
		}

		// íÖíeçœÇ›(ÉQÅ[ÉÄÉVÅ[ÉìÇ™èàóùÇ∑ÇÈÇ‹Ç≈è≠ÇµécÇ∑)
		if (s.exploded)
		{
			continue;
		}

		s.age += deltaTime;

		float e = s.age / s.flightTime;
		if (e > 1.0f) { e = 1.0f; }

		// åÇÇ¡ÇΩà íuÇ©ÇÁíÖíeì_Ç‹Ç≈àÍíºê¸ÅBìríÜÇæÇØ arc ÇÃï™ÇæÇØñcÇÁÇﬁ
		const VECTOR line = VAdd(s.start, VScale(VSub(s.target, s.start), e));
		const VECTOR newPos = VAdd(line, VScale(s.arc, sinf(DX_PI_F * e)));

		s.dir = NormalizeOr(VSub(newPos, s.pos), s.dir);
		s.pos = newPos;

		if (e >= 1.0f)
		{
			s.pos = s.target;
			s.exploded = true;
			s.life = EXPLODE_GRACE;
		}
	}

	shots_.erase(
		std::remove_if(shots_.begin(), shots_.end(),
			[](const Shot& s) { return s.dead; }),
		shots_.end());
}

void PlayerBarrage::Draw(void) const
{
	if (shots_.empty())
	{
		return;
	}

	SetWriteZBuffer3D(FALSE);
	SetDrawBlendMode(DX_BLENDMODE_ADD, 255);

	for (const auto& s : shots_)
	{
		// íÖíeÇµÇΩíeÇÕÅAîöî≠ÇÃÉGÉtÉFÉNÉgÇ…îCÇπÇƒï`Ç©Ç»Ç¢
		if (s.dead || s.exploded)
		{
			continue;
		}

		for (int i = 4; i >= 1; i--)
		{
			const float k = static_cast<float>(i);
			const VECTOR p = VSub(s.pos, VScale(s.dir, k * cfg::Barrage::TAIL_STEP));
			const float r = cfg::Barrage::RADIUS * (1.0f - k * 0.18f);

			DrawSphere3D(p, r, 12, GetColor(255, 170, 30), GetColor(255, 120, 0), TRUE);
		}

		DrawSphere3D(s.pos, cfg::Barrage::RADIUS * 1.6f, 16,
			GetColor(255, 200, 60), GetColor(255, 140, 0), TRUE);
		DrawSphere3D(s.pos, cfg::Barrage::RADIUS, 16,
			GetColor(255, 255, 255), GetColor(255, 255, 200), TRUE);
	}

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	SetWriteZBuffer3D(TRUE);
}

VECTOR PlayerBarrage::RingCenter(void)
{
	return VGet(
		Application::SCREEN_SIZE_X * cfg::Barrage::RING_CENTER_X_RATE,
		Application::SCREEN_SIZE_Y * cfg::Barrage::RING_CENTER_Y_RATE,
		0.0f);
}

float PlayerBarrage::RingRadius(void)
{
	return Application::SCREEN_SIZE_Y * cfg::Barrage::RING_RADIUS_RATE;
}