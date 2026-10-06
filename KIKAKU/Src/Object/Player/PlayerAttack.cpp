#include <cfloat>
#include <cmath>
#include "PlayerConfig.h"
#include "PlayerAttack.h"

namespace cfg = PlayerConfig;

namespace
{
	constexpr float EPSILON = 0.001f;

	VECTOR ToHorizontal(const VECTOR& v)
	{
		return VGet(v.x, 0.0f, v.z);
	}
}

PlayerAttack::PlayerAttack(void)
	:
	isChasing_(false),
	chaseTimer_(0.0f),
	chaseStuckTimer_(0.0f),
	chaseLastDistance_(0.0f),
	isAttack_(false),
	combo_(0),
	nextRequested_(false),
	attackTimer_(0.0f),
	hasHit_(false),
	trigger_(false)
{
}

//------------------------------------------------------------
// 追撃
//------------------------------------------------------------

void PlayerAttack::StartChase(void)
{
	isChasing_ = true;
	chaseTimer_ = 0.0f;
	chaseStuckTimer_ = 0.0f;
	chaseLastDistance_ = FLT_MAX;
}

void PlayerAttack::CancelChase(void)
{
	EndChase();
}

void PlayerAttack::EndChase(void)
{
	isChasing_ = false;
	chaseTimer_ = 0.0f;
	chaseStuckTimer_ = 0.0f;
}

PlayerAttack::ChaseResult PlayerAttack::UpdateChase(const Context& ctx)
{
	ChaseResult result;

	if (!isChasing_)
	{
		return result;
	}

	if (!ctx.hasTarget)
	{
		EndChase();
		result.end = ChaseEnd::CANCEL;
		return result;
	}

	chaseTimer_ += ctx.deltaTime;

	const VECTOR toTarget = VSub(ctx.targetPos, ctx.playerPos);
	const float distance = VSize(toTarget);
	const float horizontalDistance = VSize(ToHorizontal(toTarget));
	const float verticalDistance = std::fabs(toTarget.y);

	// 飛びこめる時間を使い切ったら、届いていなくてもその場で攻撃する(空振りになる)
	if (chaseTimer_ >= cfg::Attack::LUNGE_TIME)
	{
		if (distance > EPSILON)
		{
			result.faceDir = VNorm(toTarget);
		}

		EndChase();
		result.end = ChaseEnd::LUNGE;
		return result;
	}

	// 到着判定
	// 3D距離だけだと、敵が地形の上下にいて近づけないときに永遠に到着しないので、
	// 水平距離+高さの許容でも判定する
	const float arriveDistance = cfg::Chase::STOP_DISTANCE + cfg::Chase::ARRIVE_MARGIN;
	const bool arrived =
		distance <= arriveDistance ||
		(horizontalDistance <= arriveDistance &&
			verticalDistance <= cfg::Chase::HEIGHT_TOLERANCE);

	if (arrived)
	{
		EndChase();
		result.end = ChaseEnd::ATTACK;
		return result;
	}

	// 前に進めていない(地形に引っかかっている)状態を検出
	if (chaseLastDistance_ - distance < cfg::Chase::STUCK_MIN_MOVE)
	{
		chaseStuckTimer_ += ctx.deltaTime;
	}
	else
	{
		chaseStuckTimer_ = 0.0f;
	}
	chaseLastDistance_ = distance;

	// 引っかかった / 時間切れ → 近ければそのまま攻撃、遠ければ中断
	if (chaseStuckTimer_ >= cfg::Chase::STUCK_TIME || chaseTimer_ >= cfg::Chase::MAX_TIME)
	{
		const bool isClose =
			horizontalDistance <= cfg::Chase::STOP_DISTANCE * 3.0f &&
			verticalDistance <= cfg::Chase::HEIGHT_TOLERANCE * 2.0f;

		EndChase();
		result.end = isClose ? ChaseEnd::ATTACK : ChaseEnd::CANCEL;
		return result;
	}

	const VECTOR dir = VNorm(toTarget);

	result.faceDir = dir;

	// 敵を通り抜けないようにする
	float moveDistance = distance - cfg::Chase::STOP_DISTANCE;

	if (moveDistance > cfg::Chase::SPEED)
	{
		moveDistance = cfg::Chase::SPEED;
	}

	result.move = VScale(dir, moveDistance);

	return result;
}

bool PlayerAttack::CalcSnapPos(const Context& ctx, VECTOR& outPos)
{
	VECTOR enemyDir = ToHorizontal(VSub(ctx.targetPos, ctx.playerPos));

	if (VSize(enemyDir) <= EPSILON)
	{
		return false;
	}

	enemyDir = VNorm(enemyDir);

	outPos = ctx.playerPos;
	outPos.x = ctx.targetPos.x - enemyDir.x * cfg::Attack::DISTANCE;
	outPos.z = ctx.targetPos.z - enemyDir.z * cfg::Attack::DISTANCE;

	return true;
}

//------------------------------------------------------------
// コンボ
//------------------------------------------------------------

void PlayerAttack::StartAttack(void)
{
	isAttack_ = true;
	combo_ = 1;
	nextRequested_ = false;

	attackTimer_ = 0.0f;
	hasHit_ = false;
	trigger_ = true;
}

void PlayerAttack::Reset(void)
{
	isAttack_ = false;
	combo_ = 0;
	nextRequested_ = false;
	attackTimer_ = 0.0f;
	hasHit_ = false;
}

PlayerAttack::ComboResult PlayerAttack::UpdateCombo(const Context& ctx, float playRate, bool animEnd)
{
	ComboResult result;

	if (!isAttack_)
	{
		return result;
	}

	attackTimer_ += ctx.deltaTime;

	CalcFollow(ctx, result);

	const bool wantNext = nextRequested_ && combo_ < cfg::Attack::MAX_COMBO;

	// 今の段のヒット判定が終わるまでは、次の段へ進まない
	// (連打で判定が出る前に次の段へ進むと、その段が一度も当たらなくなる)
	bool hitWindowPassed = true;

	if (combo_ >= 1 && combo_ <= cfg::Attack::MAX_COMBO)
	{
		hitWindowPassed = attackTimer_ >= cfg::Attack::HIT_WINDOWS[combo_ - 1].end;
	}

	if (animEnd ||
		(wantNext && hitWindowPassed &&
			playRate >= cfg::Attack::NEXT_PLAY_RATE_THRESHOLD))
	{
		if (wantNext)
		{
			result.advance = true;
		}
		else
		{
			result.finish = true;
		}
	}

	return result;
}

// 攻撃中、敵の方を向き、通常コンボ中は一定距離を保つ
void PlayerAttack::CalcFollow(const Context& ctx, ComboResult& result) const
{
	if (!ctx.hasTarget)
	{
		return;
	}

	const VECTOR toTarget = VSub(ctx.targetPos, ctx.playerPos);

	if (VSize(toTarget) <= EPSILON)
	{
		return;
	}

	// 向きは横方向だけ
	result.faceDir = VNorm(toTarget);

	// 8段目は途中から追従しない
	bool keepAttackPosition = (combo_ >= 1 && combo_ <= 7);

	if (combo_ == cfg::Attack::MAX_COMBO && attackTimer_ < cfg::Attack::COMBO8_FOLLOW_END)
	{
		keepAttackPosition = true;
	}

	if (!keepAttackPosition)
	{
		return;
	}

	// 遠くへ飛ばされた敵には吸い付かない
	if (VSize(ToHorizontal(toTarget)) > cfg::Attack::FOLLOW_MAX_DISTANCE)
	{
		return;
	}

	VECTOR enemyDir = ToHorizontal(toTarget);

	if (VSize(enemyDir) <= EPSILON)
	{
		return;
	}

	enemyDir = VNorm(enemyDir);

	const VECTOR targetPos =
		VSub(ctx.targetPos, VScale(enemyDir, cfg::Attack::DISTANCE));

	result.hasFollow = true;
	result.follow = VScale(VSub(targetPos, ctx.playerPos), cfg::Attack::FOLLOW_RATE);
}

// 次のコンボへ進む(2~8段目)
PlayerAttack::ComboStep PlayerAttack::AdvanceCombo(const Context& ctx)
{
	combo_++;
	nextRequested_ = false;

	attackTimer_ = 0.0f;
	hasHit_ = false;
	trigger_ = true;

	ComboStep step;
	step.combo = combo_;

	if (!ctx.hasTarget)
	{
		return step;
	}

	const VECTOR toEnemy = ToHorizontal(VSub(ctx.targetPos, ctx.playerPos));

	switch (combo_)
	{
	case 4:	// 敵の横へ移動
		if (VSize(toEnemy) > EPSILON)
		{
			const VECTOR dir = VNorm(toEnemy);
			const VECTOR sideDir = VGet(-dir.z, 0.0f, dir.x);

			step.warp = true;
			step.setAfterImagePos = true;
			step.warpPos = VAdd(ctx.targetPos, VScale(sideDir, cfg::Attack::WARP_DISTANCE));
		}
		break;

	case 6:	// 敵の反対側へ移動
		if (VSize(toEnemy) > EPSILON)
		{
			const VECTOR dir = VNorm(toEnemy);
			const VECTOR sideDir = VGet(dir.z, 0.0f, -dir.x);

			step.warp = true;
			step.warpPos = VAdd(ctx.targetPos, VScale(sideDir, cfg::Attack::WARP_DISTANCE));
		}
		break;

	case 7:	// 敵の後ろへ移動
	{
		VECTOR dir = VSub(ctx.targetPos, ctx.playerPos);

		if (VSize(dir) > EPSILON)
		{
			dir = VNorm(dir);

			step.warp = true;
			step.warpPos = VAdd(ctx.targetPos, VScale(dir, cfg::Attack::WARP_DISTANCE));
		}
		break;
	}

	case 8:	// 敵の斜め上へ移動
		if (VSize(toEnemy) > EPSILON)
		{
			const VECTOR dir = VNorm(toEnemy);

			step.warp = true;
			step.lookPitch = true;
			step.warpPos = VAdd(ctx.targetPos, VScale(dir, -cfg::Attack::WARP_BACK_DISTANCE));
			step.warpPos.y += cfg::Attack::WARP_HEIGHT;
		}
		break;

	default:
		break;
	}

	return step;
}

bool PlayerAttack::IsHitTiming(void) const
{
	// 範囲外のコンボは既定値(0.2~0.3秒)
	cfg::Attack::HitWindow window = { 0.2f, 0.3f };

	if (combo_ >= 1 && combo_ <= cfg::Attack::MAX_COMBO)
	{
		window = cfg::Attack::HIT_WINDOWS[combo_ - 1];
	}

	return isAttack_ &&
		attackTimer_ >= window.start &&
		attackTimer_ <= window.end;
}