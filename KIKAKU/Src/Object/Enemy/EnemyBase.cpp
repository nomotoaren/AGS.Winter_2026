#include "EnemyBase.h"
#include "../../Utility/AsoUtility.h"

namespace
{
	// 「ゼロ扱い」にする長さ
	constexpr float kEpsilon = 0.001f;
}

EnemyBase::EnemyBase(void)
	:
	knockBackPow_(AsoUtility::VECTOR_ZERO),
	hp_(1),
	isDead_(false),
	hitRadius_(50.0f),
	pendingDamage_(0),
	isSyncReady_(false),
	syncTimer_(0.0f)
{
}

EnemyBase::~EnemyBase(void)
{
}

//------------------------------------------------------------
// ダメージ・HP
//------------------------------------------------------------
void EnemyBase::Damage(int damage)
{
	if (isDead_)
	{
		return;
	}

	hp_ -= damage;

	if (hp_ <= 0)
	{
		hp_ = 0;
		isDead_ = true;
	}
}

bool EnemyBase::IsDead(void) const
{
	return isDead_;
}

int EnemyBase::GetHp(void) const
{
	return hp_;
}

float EnemyBase::GetHitRadius(void) const
{
	return hitRadius_;
}

void EnemyBase::AddKnockBack(VECTOR dir, float power)
{
	if (isDead_ || VSize(dir) <= kEpsilon)
	{
		return;
	}

	knockBackPow_ = VScale(VNorm(dir), power);
}

//------------------------------------------------------------
// SYNC ATTACK
//------------------------------------------------------------
void EnemyBase::StartSyncWindow(void)
{
	isSyncReady_ = true;
	syncTimer_ = 0.0f;
}

void EnemyBase::UpdateSyncWindow(float deltaTime)
{
	if (!isSyncReady_)
	{
		return;
	}

	syncTimer_ += deltaTime;

	// 受付時間を過ぎたら終了
	if (syncTimer_ >= SYNC_TIME)
	{
		EndSyncWindow();
	}
}

bool EnemyBase::IsSyncReady(void) const
{
	return isSyncReady_;
}

void EnemyBase::EndSyncWindow(void)
{
	isSyncReady_ = false;
	syncTimer_ = 0.0f;
}

// 受付開始時が 1.0、受付終了時が 0.0 になる残り時間の割合
float EnemyBase::GetSyncRate(void) const
{
	if (!isSyncReady_)
	{
		return 0.0f;
	}

	float rate = 1.0f - syncTimer_ / SYNC_TIME;

	if (rate < 0.0f) { rate = 0.0f; }
	if (rate > 1.0f) { rate = 1.0f; }

	return rate;
}
