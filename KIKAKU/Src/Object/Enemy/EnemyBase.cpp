#include "EnemyBase.h"
#include "../../Utility/AsoUtility.h"

EnemyBase::EnemyBase(void)
    :
    hp_(1),
    isDead_(false),
    hitRadius_(50.0f),
    pendingDamage_(0),
    isSyncReady_(false),
    syncTimer_(0.0f)
{
    knockBackPow_ =
        AsoUtility::VECTOR_ZERO;
}

EnemyBase::~EnemyBase(void)
{
}

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

void EnemyBase::AddPendingDamage(int damage)
{
    if (isDead_)
    {
        return;
    }

    pendingDamage_ += damage;
}

void EnemyBase::ApplyPendingDamage(void)
{
    if (isDead_)
    {
        pendingDamage_ = 0;
        return;
    }

    if (pendingDamage_ <= 0)
    {
        return;
    }

    Damage(pendingDamage_);

    pendingDamage_ = 0;
}

int EnemyBase::GetPendingDamage(void) const
{
    return pendingDamage_;
}

void EnemyBase::AddKnockBack(VECTOR dir,  float power)
{
    if (isDead_)
    {
        return;
    }

    dir.y = 0.0f;

    if (VSize(dir) <= 0.001f)
    {
        return;
    }

    dir =
        VNorm(dir);

    knockBackPow_ =
        VScale(
            dir,
            power
        );
}

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

float EnemyBase::GetSyncRate(void) const
{
    if (!isSyncReady_)
    {
        return 0.0f;
    }

    float rate =
        1.0f -
        syncTimer_ / SYNC_TIME;

    if (rate < 0.0f)
    {
        rate = 0.0f;
    }

    if (rate > 1.0f)
    {
        rate = 1.0f;
    }

    return rate;
}