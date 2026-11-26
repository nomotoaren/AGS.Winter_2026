#include <cmath>
#include "../Manager/SceneManager.h"
#include "Player/definition/PlayerConfig.h"
#include "KiBlast.h"

namespace
{
	// 1フレームあたりの移動量に掛ける倍率
	//   (弾の速さは 144fps の1フレーム分で決めてあるので、60fps なら約2.4倍にして同じ速さにする)
	float FrameScale(void)
	{
		float s = SceneManager::GetInstance().GetFrameScale();

		// 処理落ちした1フレームで、飛びすぎないようにする
		if (s > 4.0f)
		{
			s = 4.0f;
		}

		return s;
	}
}

KiBlast::KiBlast(VECTOR pos, VECTOR dir, bool homing)
{
	pos_ = pos;
	dir_ = dir;
	homing_ = homing;

	speed_ = 25.0f;

	// 飛んでいる時間(秒)
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

void KiBlast::SetTarget(const VECTOR& targetPos, bool hasTarget)
{
	targetPos_ = targetPos;
	hasTarget_ = hasTarget;
}

// 向きを、目標の向きへ上限つきで曲げる
void KiBlast::SteerToTarget(float deltaTime)
{
	const VECTOR toTarget = VSub(targetPos_, pos_);
	const float dist = VSize(toTarget);

	if (dist < 0.001f)
	{
		return;
	}

	const VECTOR want = VScale(toTarget, 1.0f / dist);

	float dot = VDot(dir_, want);
	if (dot > 1.0f) { dot = 1.0f; }
	if (dot < -1.0f) { dot = -1.0f; }

	// 敵を追い越した(後ろに回った)ら、そのまま飛んでいく(周りを回り続けない)
	if (dot < 0.0f)
	{
		lostTarget_ = true;
		return;
	}

	const float angle = acosf(dot);
	const float maxStep = PlayerConfig::KiShot::HOMING_TURN_SPEED * deltaTime;

	if (angle <= maxStep)
	{
		dir_ = want;
		return;
	}

	// 上限の分だけ、目標の向きへ寄せる
	const float t = maxStep / angle;
	const VECTOR mixed = VAdd(dir_, VScale(VSub(want, dir_), t));

	if (VSize(mixed) > 0.001f)
	{
		dir_ = VNorm(mixed);
	}
}

void KiBlast::Update(void)
{
	if (isDead_)
	{
		return;
	}

	const float dt = SceneManager::GetInstance().GetDeltaTime();

	age_ += dt;

	if (homing_ && hasTarget_ && !lostTarget_ && age_ >= PlayerConfig::KiShot::HOMING_DELAY)
	{
		SteerToTarget(dt);
	}

	pos_ =
		VAdd(
			pos_,
			VScale(
				dir_,
				speed_ * FrameScale()
			)
		);

	lifeTime_ -= dt;

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