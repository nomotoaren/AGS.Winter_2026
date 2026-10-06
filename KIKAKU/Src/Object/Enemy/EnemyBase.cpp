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

void EnemyBase::PushOut(const VECTOR& offset)
{
	if (isDead_)
	{
		return;
	}

	transform_.pos = VAdd(transform_.pos, offset);
}

//------------------------------------------------------------
// HPバー(HUD)
//------------------------------------------------------------

// 各敵の Init の最後で1回呼ぶ。maxHp はその時点の hp_ を渡すとよい
void EnemyBase::InitHud(const std::string& name, int maxHp)
{
	hud_.Init();
	hud_.SetName(name);
	hud_.SetIconText(name.substr(0, 1));
	hud_.SetMaxHp(maxHp);
}

// 各敵の Update で、isDead_ チェックの直後に呼ぶ
void EnemyBase::UpdateHud(void)
{
	hud_.Update(hp_);
}

// 各敵の Draw の最後で呼ぶ(2D描画なので3D描画のあと)
void EnemyBase::DrawHud(void)
{
	if (isDead_)
	{
		return;
	}

	hud_.Draw();
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