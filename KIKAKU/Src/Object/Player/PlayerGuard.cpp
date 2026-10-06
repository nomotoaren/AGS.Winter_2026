#include "PlayerConfig.h"
#include "PlayerGuard.h"

namespace cfg = PlayerConfig;

PlayerGuard::PlayerGuard(void)
	:
	isGuard_(false),
	hp_(cfg::Guard::MAX_HP),
	recoverTimer_(0.0f),
	isBreak_(false),
	breakTimer_(0.0f),
	isBurst_(false),
	burstTimer_(0.0f),
	burstTrigger_(false)
{
}

PlayerGuard::Result PlayerGuard::Update(const Context& ctx)
{
	Result result;

	burstTrigger_ = false;

	// ガードブレイク中
	if (isBreak_)
	{
		breakTimer_ -= ctx.deltaTime;

		if (breakTimer_ <= 0.0f)
		{
			breakTimer_ = 0.0f;
			isBreak_ = false;

			hp_ = cfg::Guard::MAX_HP;

			result.breakEnded = true;
		}

		return result;
	}

	// ガード耐久値の回復(被弾から一定時間後に回復開始)
	if (hp_ < cfg::Guard::MAX_HP)
	{
		if (recoverTimer_ > 0.0f)
		{
			recoverTimer_ -= ctx.deltaTime;
		}
		else
		{
			hp_ += cfg::Guard::RECOVER_SPEED * ctx.deltaTime;

			if (hp_ > cfg::Guard::MAX_HP)
			{
				hp_ = cfg::Guard::MAX_HP;
			}
		}
	}

	// ガード開始 / 解除
	if (ctx.canStart && !isGuard_ && !isBurst_ && ctx.hold)
	{
		isGuard_ = true;
		result.started = true;
	}
	else if (isGuard_ && !ctx.hold)
	{
		isGuard_ = false;
		result.released = true;
	}

	// バースト中
	if (isBurst_)
	{
		burstTimer_ -= ctx.deltaTime;

		if (burstTimer_ <= 0.0f)
		{
			burstTimer_ = 0.0f;
			isBurst_ = false;

			result.burstEnded = true;
		}

		return result;
	}

	// ガード中にバースト入力(気が足りるかは Player が確認する)
	if (isGuard_ && ctx.burstTrg)
	{
		result.burstRequested = true;
	}

	return result;
}

void PlayerGuard::StartBurst(void)
{
	isBurst_ = true;
	burstTrigger_ = true;
	burstTimer_ = cfg::Guard::BURST_TIME;

	isGuard_ = false;
}

bool PlayerGuard::Damage(float damage)
{
	if (isBreak_)
	{
		return false;
	}

	// 回復開始までの時間をリセット
	recoverTimer_ = cfg::Guard::RECOVER_DELAY;

	hp_ -= damage;

	if (hp_ <= 0.0f)
	{
		hp_ = 0.0f;

		isGuard_ = false;
		isBreak_ = true;
		breakTimer_ = cfg::Guard::BREAK_TIME;

		return true;
	}

	return false;
}