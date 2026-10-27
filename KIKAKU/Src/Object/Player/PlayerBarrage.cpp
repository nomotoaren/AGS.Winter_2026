#include <algorithm>
#include <cmath>
#include "PlayerConfig.h"
#include "PlayerBarrage.h"

namespace cfg = PlayerConfig;

namespace
{
	VECTOR NormalizeOr(const VECTOR& v, const VECTOR& fallback)
	{
		return (VSize(v) > 0.001f) ? VNorm(v) : fallback;
	}

	// -1.0 ~ 1.0 のランダム
	float Rnd(void)
	{
		return (GetRand(2000) - 1000) / 1000.0f;
	}

	// 爆発してから、ゲームシーンが処理するまでの猶予(秒)
	constexpr float EXPLODE_GRACE = 0.2f;
}

PlayerBarrage::PlayerBarrage(void)
	:
	state_(STATE::IDLE),
	timer_(0.0f),
	firedCount_(0),
	firedFlag_(false),
	modelId_(-1),
	rightHandFrame_(-1),
	leftHandFrame_(-1),
	aimPoint_{ 0.0f, 0.0f, 0.0f },
	aimOffsetX_(0.0f),
	aimOffsetY_(0.0f),
	aimYaw_(0.0f),
	aimPitch_(0.0f)
{
}

PlayerBarrage::~PlayerBarrage(void)
{
}

void PlayerBarrage::SetModel(int modelId)
{
	modelId_ = modelId;
	rightHandFrame_ = MV1SearchFrame(modelId_, "mixamorig:RightHand");
	leftHandFrame_ = MV1SearchFrame(modelId_, "mixamorig:LeftHand");
}

void PlayerBarrage::Start(void)
{
	state_ = STATE::CHARGE;
	timer_ = 0.0f;
	firedCount_ = 0;
	firedFlag_ = false;
	aimOffsetX_ = 0.0f;
	aimOffsetY_ = 0.0f;
	aimYaw_ = 0.0f;
	aimPitch_ = 0.0f;
}

void PlayerBarrage::Cancel(void)
{
	state_ = STATE::IDLE;
	timer_ = 0.0f;
	firedFlag_ = false;
}

bool PlayerBarrage::ConsumeFired(void)
{
	const bool fired = firedFlag_;
	firedFlag_ = false;
	return fired;
}

float PlayerBarrage::RingRadius(void)
{
	return cfg::Barrage::RING_RADIUS;
}

bool PlayerBarrage::Update(const Context& ctx)
{
	bool finished = false;

	if (state_ != STATE::IDLE)
	{
		UpdateAim(ctx);

		timer_ += ctx.deltaTime;

		switch (state_)
		{
		case STATE::CHARGE:
			// 構えが終わったら、連射を始める
			if (timer_ >= cfg::Barrage::START_DELAY)
			{
				state_ = STATE::FIRE;
				timer_ = 0.0f;
				firedCount_ = 0;
			}
			break;

		case STATE::FIRE:
			// 一定間隔で1発ずつ(最初の1発は、連射に入った瞬間)
			if (firedCount_ < cfg::Barrage::SHOT_COUNT &&
				timer_ >= cfg::Barrage::INTERVAL * firedCount_)
			{
				FireShot(ctx);
			}

			if (firedCount_ >= cfg::Barrage::SHOT_COUNT)
			{
				state_ = STATE::END;
				timer_ = 0.0f;
			}
			break;

		case STATE::END:
			// 撃ち終わって少し待ったら終了
			if (timer_ >= cfg::Barrage::END_DELAY)
			{
				state_ = STATE::IDLE;
				timer_ = 0.0f;
				finished = true;
			}
			break;

		default:
			break;
		}
	}

	// 飛んでいる弾は、構えが終わっても動かし続ける
	UpdateShots(ctx.deltaTime);

	return finished;
}

// 枠の中心を決める: 最初は敵の中心。スティックで、カメラから見て上下左右にずらせる
void PlayerBarrage::UpdateAim(const Context& ctx)
{
	aimOffsetX_ += ctx.stickX * cfg::Barrage::AIM_SPEED * ctx.deltaTime;
	aimOffsetY_ += ctx.stickY * cfg::Barrage::AIM_SPEED * ctx.deltaTime;

	if (aimOffsetX_ > cfg::Barrage::AIM_MAX) { aimOffsetX_ = cfg::Barrage::AIM_MAX; }
	if (aimOffsetX_ < -cfg::Barrage::AIM_MAX) { aimOffsetX_ = -cfg::Barrage::AIM_MAX; }
	if (aimOffsetY_ > cfg::Barrage::AIM_MAX) { aimOffsetY_ = cfg::Barrage::AIM_MAX; }
	if (aimOffsetY_ < -cfg::Barrage::AIM_MAX) { aimOffsetY_ = -cfg::Barrage::AIM_MAX; }

	VECTOR base;

	if (ctx.hasTarget)
	{
		base = VAdd(ctx.targetPos, VGet(0.0f, cfg::Barrage::TARGET_HEIGHT, 0.0f));
	}
	else
	{
		// ロックオンしていないときは、前方
		base = VAdd(ctx.playerPos,
			VAdd(VScale(ctx.forward, cfg::Barrage::FORWARD_AIM_DIST), VGet(0.0f, 100.0f, 0.0f)));
	}

	// ずらす向きは、プレイヤーから見た「横」と「上」(カメラが回っても揺れない)
	VECTOR dir = VSub(base, ctx.playerPos);
	dir.y = 0.0f;
	const float dist = VSize(dir);
	dir = NormalizeOr(dir, ctx.forward);
	const VECTOR right = VCross(VGet(0.0f, 1.0f, 0.0f), dir);

	aimPoint_ = VAdd(base,
		VAdd(VScale(right, aimOffsetX_), VGet(0.0f, aimOffsetY_, 0.0f)));

	const float d = (dist > 100.0f) ? dist : 100.0f;
	aimYaw_ = atan2f(aimOffsetX_, d);
	aimPitch_ = atan2f(aimOffsetY_, d);
}

VECTOR PlayerBarrage::GetHandPos(int index, const VECTOR& fallback) const
{
	// 右手 -> 左手 -> 右手 ... と交互に
	const int frame = (index % 2 == 0) ? rightHandFrame_ : leftHandFrame_;

	if (modelId_ == -1 || frame == -1)
	{
		return fallback;
	}

	return MV1GetFramePosition(modelId_, frame);
}

void PlayerBarrage::FireShot(const Context& ctx)
{
	const VECTOR start = GetHandPos(firedCount_, VAdd(ctx.playerPos, VGet(0.0f, 100.0f, 0.0f)));

	// 枠の中のランダムな点(球の中)
	VECTOR offset = VGet(Rnd(), Rnd(), Rnd());
	const float len = VSize(offset);

	if (len > 1.0f)
	{
		offset = VScale(offset, 1.0f / len);
	}

	offset = VScale(offset, cfg::Barrage::RING_RADIUS * cfg::Barrage::AIM_RATE);

	const VECTOR target = VAdd(aimPoint_, offset);
	const float distance = VSize(VSub(target, start));

	float flight = distance / cfg::Barrage::SPEED;

	if (flight < cfg::Barrage::MIN_FLIGHT)
	{
		flight = cfg::Barrage::MIN_FLIGHT;
	}

	Shot s{};
	s.start = start;
	s.offset = offset;
	s.pos = start;
	s.dir = NormalizeOr(VSub(target, start), ctx.forward);
	s.age = 0.0f;
	s.flightTime = flight;

	// 軌道の膨らみ(カメラから見て、横と上下へゆるく)
	s.arc = VScale(
		VAdd(VScale(GetCameraRightVector(), Rnd()), VScale(GetCameraUpVector(), Rnd() * 0.5f)),
		distance * cfg::Barrage::ARC);

	s.life = cfg::Barrage::LIFE;
	s.exploded = false;
	s.handled = false;
	s.dead = false;

	shots_.push_back(s);

	firedCount_++;
	firedFlag_ = true;
}

void PlayerBarrage::UpdateShots(float deltaTime)
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

		// 着弾済み(ゲームシーンが処理するまで少し残す)
		if (s.exploded)
		{
			continue;
		}

		s.age += deltaTime;

		float e = s.age / s.flightTime;
		if (e > 1.0f) { e = 1.0f; }

		// 着弾点は、枠の中心が動いても追いかける
		const VECTOR target = VAdd(aimPoint_, s.offset);

		// 撃った位置から着弾点まで一直線。途中だけ arc の分だけ膨らむ
		const VECTOR line = VAdd(s.start, VScale(VSub(target, s.start), e));
		const VECTOR newPos = VAdd(line, VScale(s.arc, sinf(DX_PI_F * e)));

		s.dir = NormalizeOr(VSub(newPos, s.pos), s.dir);
		s.pos = newPos;

		if (e >= 1.0f)
		{
			s.pos = target;
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
		// 着弾した弾は、爆発のエフェクトに任せて描かない
		if (s.dead || s.exploded)
		{
			continue;
		}

		// 尾
		for (int i = 4; i >= 1; i--)
		{
			const float k = static_cast<float>(i);
			const VECTOR p = VSub(s.pos, VScale(s.dir, k * cfg::Barrage::TAIL_STEP));
			const float r = cfg::Barrage::RADIUS * (1.0f - k * 0.18f);

			DrawSphere3D(p, r, 12, GetColor(255, 170, 30), GetColor(255, 120, 0), TRUE);
		}

		// 本体
		DrawSphere3D(s.pos, cfg::Barrage::RADIUS * 1.6f, 16,
			GetColor(255, 200, 60), GetColor(255, 140, 0), TRUE);
		DrawSphere3D(s.pos, cfg::Barrage::RADIUS, 16,
			GetColor(255, 255, 255), GetColor(255, 255, 200), TRUE);
	}

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	SetWriteZBuffer3D(TRUE);
}
