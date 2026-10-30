#include <DxLib.h>
#include <cmath>
#include <algorithm>
#include "MeleeEnemy.h"
#include "../../../Application.h"
#include "../../Player/Player.h"
#include "../../../Manager/Camera/Camera.h"
#include "../../../Manager/EffekseerEffect.h"
#include "../../../Manager/ResourceManager.h"
#include "../../../Manager/SceneManager.h"
#include "../../../Utility/AsoUtility.h"
#include "MeleeEnemy_Internal.h"

using namespace MeleeEnemyInternal;

// 反撃と気: 毎フレーム呼ぶ(スタン中は反撃も大技も出さない)
void MeleeEnemy::UpdateCounterAndKi(float deltaTime, bool isStunned)
{
	CountDown(hitGate_, deltaTime);
	CountDown(burstCool_, deltaTime);
	CountDown(burstFx_, deltaTime);

	if (hitStreakTimer_ > 0.0f)
	{
		hitStreakTimer_ -= deltaTime;

		if (hitStreakTimer_ <= 0.0f)
		{
			hitStreak_ = 0;
		}
	}

	// 気は自動で回復する
	enemyKi_ += kEnemyKiRegen * deltaTime;

	if (enemyKi_ > kEnemyKiMax)
	{
		enemyKi_ = kEnemyKiMax;
	}

	if (isStunned || isDead_ || clashLock_ || isKamehameHit_ || IsDoingSpecial())
	{
		return;
	}

	// 殴られ続けたら、プレイヤーを吹き飛ばす
	if (hitStreak_ >= kBurstHits && burstCool_ <= 0.0f)
	{
		DoCounterBurst();
		return;
	}

	// 気が溜まったら、殴られていてもひるまずに大技を出す
	if (enemyKi_ >= kEnemyKiMax)
	{
		ForceSpecial();
	}
}

// 反撃: 衝撃波でプレイヤーを吹き飛ばす(ガードバースト)
void MeleeEnemy::DoCounterBurst(void)
{
	hitStreak_ = 0;
	hitStreakTimer_ = 0.0f;
	burstCool_ = kBurstCoolTime;

	// 怯みをやめて、こちらから動く
	isDamage_ = false;
	damageTimer_ = 0.0f;
	knockBackPow_ = AsoUtility::VECTOR_ZERO;
	ResetAttack();
	CancelBoostChase();
	ReturnToWait();
	if (attackCoolTimer_ < 1.5f)
	{
		attackCoolTimer_ = 1.5f;
	}

	LookAtPlayer();

	const VECTOR toPlayer = VSub(player_.GetTransform().pos, transform_.pos);

	if (VSize(toPlayer) > kBurstRange)
	{
		return;
	}

	burstFx_ = kBurstFxTime;

	ApplyHitToPlayer(transform_.pos, kBurstDamage, kBurstKnock, 60.0f, 8.0f);

	// 吹き飛ばされたあと、少しの間動けない
	if (lastHitDamaged_)
	{
		player_.ExtendDamageStun(kBurstStun);
	}

	EffekseerEffect::GetInstance()->PlayHitEffect(VAdd(transform_.pos, VGet(0.0f, 90.0f, 0.0f)), 0.0f);

	// 離れたところで、すぐに大技の溜めに入る
	ForceSpecial();
}

// 気が溜まった: 大技を出す(使える中からランダム。出せるものが無ければ何もしない)
bool MeleeEnemy::ForceSpecial(void)
{
	int options[3] = {};
	int count = 0;

	if (bigCoolTimer_ <= 0.0f) { options[count++] = 0; }
	if (beamCoolTimer_ <= 0.0f) { options[count++] = 1; }
	if (barrageCoolTimer_ <= 0.0f) { options[count++] = 2; }

	if (count == 0)
	{
		return false;
	}

	const int pick = options[GetRand(count - 1)];

	// 今の行動(怯み・ガード・近接攻撃)をやめる
	isDamage_ = false;
	damageTimer_ = 0.0f;
	isGuard_ = false;
	guardTimer_ = 0.0f;
	knockBackPow_ = AsoUtility::VECTOR_ZERO;
	ResetAttack();
	CancelBoostChase();

	switch (pick)
	{
	case 0:
		SetAIState(AI_STATE::BIG_CHARGE);
		break;

	case 1:
		StartBeamCharge();
		break;

	default:
		StartBarrage();
		break;
	}

	return true;
}

// ビームの溜めを始める
void MeleeEnemy::StartBeamCharge(void)
{
	SetAIState(AI_STATE::BEAM_CHARGE);
	beamLocked_ = false;
}

// 気弾の連射を始める(HPが半分以下なら弾数が増える)
void MeleeEnemy::StartBarrage(void)
{
	const bool enraged = (hp_ * 2 <= maxHp_);

	SetAIState(AI_STATE::KI_BARRAGE);
	barrageShotsLeft_ = enraged ? kBarrageCountEnraged : kBarrageCount;
	barrageShotTimer_ = 0.0f;
	barrageBigPhase_ = false;
	barrageBigTimer_ = 0.0f;
}

//----------------------------------------------------------------------
// AI
//----------------------------------------------------------------------
void MeleeEnemy::UpdateAI(const AIContext& ctx)
{
	switch (aiState_)
	{
	case AI_STATE::WAIT:
		UpdateAIWait(ctx);
		break;

	case AI_STATE::SIDE_MOVE:
		UpdateAISideMove(ctx);
		break;

	case AI_STATE::BOOST_ATTACK:
		UpdateAIBoostAttack(ctx);
		break;

	case AI_STATE::ATTACK:
		UpdateAIAttack(ctx);
		break;

	default:
		// KI_CHARGE などの技は、Update の前半で処理している
		break;
	}
}

// 様子見する。時間が経ったら、次の行動を選ぶ
void MeleeEnemy::UpdateAIWait(const AIContext& ctx)
{
	aiTimer_ += ctx.deltaTime;

	animationController_->Play((int)ANIM_TYPE::IDLE);

	// 隙の間(aiTimer_ がマイナス)は動かない。それ以外は、プレイヤーの周りをゆっくり漂う
	if (aiTimer_ >= 0.0f)
	{
		float radius = ctx.distance;

		if (radius < kWaitMinRadius) { radius = kWaitMinRadius; }
		if (radius > kWaitMaxRadius) { radius = kWaitMaxRadius; }

		OrbitPlayer(ctx, kWaitDrift, radius,
			kHoverBobAmp * sinf(hoverTime_ * kHoverBobSpeed));
	}

	// 吹き飛ばした直後なら、追撃に入る
	if (pendingChase_ && aiTimer_ >= kChaseStartDelay)
	{
		StartChase();
		return;
	}

	if (aiTimer_ < kWaitTime)
	{
		return;
	}

	DecideAction(ctx);
}

// 距離とHPから、次の行動を選ぶ
//   近い: 近接攻撃が中心 / 中くらい: 気弾・突進 / 遠い: 高速接近・気弾
//   HPが半分以下になると、気弾と突進を多く使う
void MeleeEnemy::DecideAction(const AIContext& ctx)
{
	const bool enraged = (hp_ * 2 <= maxHp_);

	const bool canAttack = (attackCoolTimer_ <= 0.0f);
	const bool canKi = (kiCoolTimer_ <= 0.0f);
	const bool canRush = (rushCoolTimer_ <= 0.0f);
	const bool canBig = (bigCoolTimer_ <= 0.0f);
	const bool canBeam = (beamCoolTimer_ <= 0.0f);
	const bool canBarrage = (barrageCoolTimer_ <= 0.0f);
	const bool canSlam = (slamCoolTimer_ <= 0.0f);

	// ガードされ続けているなら、ガード不能の投げを選ぶ
	const bool wantGrab =
		guardWatchTimer_ >= kGuardWatchTrigger &&
		grabCoolTimer_ <= 0.0f &&
		ctx.distance < kGuardWatchRange + 100.0f;

	int weights[ACT_MAX] = {};

	if (ctx.distance < kNearDistance)
	{
		// 近くでずっと殴り続けない: 横移動や気弾を混ぜ、離れて間合いを取り直す
		weights[ACT_ATTACK] = canAttack ? 2 : 0;
		weights[ACT_SIDE] = 3;
		weights[ACT_KI] = canKi ? 2 : 0;
		// 近距離でも、構えて溜めてからの強い攻撃(突進)を出す
		weights[ACT_RUSH] = canRush ? (enraged ? 5 : 4) : 0;
		weights[ACT_BIG] = canBig ? (enraged ? 2 : 1) : 0;
		weights[ACT_SLAM] = canSlam ? 1 : 0;
	}
	else if (ctx.distance < kFarDistance)
	{
		weights[ACT_KI] = canKi ? (enraged ? 5 : 3) : 0;
		weights[ACT_RUSH] = canRush ? (enraged ? 5 : 3) : 0;
		weights[ACT_SIDE] = 2;
		weights[ACT_BOOST] = 3;
		weights[ACT_BIG] = canBig ? (enraged ? 4 : 2) : 0;
		weights[ACT_BEAM] = canBeam ? (enraged ? 3 : 2) : 0;
		weights[ACT_BARRAGE] = canBarrage ? (enraged ? 4 : 3) : 0;
		weights[ACT_SLAM] = canSlam ? (enraged ? 3 : 2) : 0;
	}
	else
	{
		// 遠いときは詰めてくるのを優先(遠くから攻撃だけで粘らせない)
		weights[ACT_BOOST] = 12;
		weights[ACT_KI] = canKi ? (enraged ? 3 : 2) : 0;
		weights[ACT_BIG] = canBig ? (enraged ? 2 : 1) : 0;
		weights[ACT_BEAM] = canBeam ? (enraged ? 2 : 1) : 0;
		weights[ACT_BARRAGE] = canBarrage ? (enraged ? 3 : 2) : 0;
	}

	if (wantGrab)
	{
		weights[ACT_GRAB] = 15;
	}

	int pick = PickWeighted(weights, ACT_MAX);

	// 選べるものが無ければ、横に動いて時間を稼ぐ
	if (pick < 0)
	{
		pick = ACT_SIDE;
	}

	switch (pick)
	{
	case ACT_ATTACK:
		// 間合いを取り直さず、そのまま殴りに行く
		SetAIState(AI_STATE::ATTACK);
		break;

	case ACT_SIDE:
		SetAIState(AI_STATE::SIDE_MOVE);

		// 旋回の向き・半径・高さを決める(同じ向きに続けることもある)
		if (GetRand(99) < 70)
		{
			sideMoveDir_ *= -1.0f;
		}

		strafeRadius_ = kOrbitMinRadius + (float)GetRand(kOrbitRadiusRange);
		strafeHeight_ = kOrbitHeightMin + (float)GetRand(kOrbitHeightRange);
		break;

	case ACT_KI:
		SetAIState(AI_STATE::KI_CHARGE);
		break;

	case ACT_RUSH:
		SetAIState(AI_STATE::RUSH_READY);
		break;

	case ACT_BOOST:
		SetAIState(AI_STATE::BOOST_ATTACK);
		break;

	case ACT_GRAB:
		SetAIState(AI_STATE::GRAB_READY);
		break;

	case ACT_BIG:
		SetAIState(AI_STATE::BIG_CHARGE);
		break;

	case ACT_BEAM:
		StartBeamCharge();
		break;

	case ACT_BARRAGE:
		StartBarrage();
		break;

	case ACT_SLAM:
		SetAIState(AI_STATE::SLAM_RISE);
		slamLocked_ = false;
		break;
	}
}

// 空中の旋回(プレイヤーの周りを、距離と高さを変えながら回る)
void MeleeEnemy::UpdateAISideMove(const AIContext& ctx)
{
	aiTimer_ += ctx.deltaTime;

	OrbitPlayer(ctx, 1.0f, strafeRadius_, strafeHeight_);

	animationController_->Play((int)ANIM_TYPE::RUN);

	// 旋回が終わったら、また次の行動を選ぶ
	if (aiTimer_ >= kSideMoveTime)
	{
		SetAIState(AI_STATE::WAIT);
	}
}

// プレイヤーを中心に、角度を進めた位置へ向かって移動する
void MeleeEnemy::OrbitPlayer(const AIContext& ctx, float speedScale, float radius, float heightOffset)
{
	const VECTOR rel = ToHorizontal(VSub(transform_.pos, ctx.playerPos));
	const float currentRadius = VSize(rel);

	float angle = 0.0f;

	if (currentRadius > kEpsilon)
	{
		angle = atan2f(rel.z, rel.x);
	}

	// 角度を進める
	angle += sideMoveDir_ * kOrbitAngularSpeed * speedScale * ctx.deltaTime;

	// 半径と高さは、目標へ少しずつ寄せる
	const float frameRate = ctx.deltaTime * 60.0f;
	const float r = currentRadius + (radius - currentRadius) * Saturate(kOrbitRadiusLerp * frameRate);

	const float targetY = ctx.playerPos.y + heightOffset;
	const float y = transform_.pos.y + (targetY - transform_.pos.y) * Saturate(kOrbitHeightLerp * frameRate);

	const VECTOR desired = VGet(
		ctx.playerPos.x + cosf(angle) * r,
		y,
		ctx.playerPos.z + sinf(angle) * r);

	// 速すぎないように、1フレームの移動量を制限する
	VECTOR move = VSub(desired, transform_.pos);
	const float maxStep = kOrbitMaxSpeed * frameRate;

	if (VSize(move) > maxStep)
	{
		move = VScale(VNorm(move), maxStep);
	}

	transform_.pos = VAdd(transform_.pos, move);

	if (transform_.pos.y < kMinHeight) { transform_.pos.y = kMinHeight; }
	if (transform_.pos.y > kMaxHeight) { transform_.pos.y = kMaxHeight; }
}

// 高速接近(この行動を選んだ時だけ動く)
void MeleeEnemy::UpdateAIBoostAttack(const AIContext& ctx)
{
	if (!isBoostChase_)
	{
		isBoostChase_ = true;
		boostChaseTimer_ = 0.0f;

		animationController_->Play((int)ANIM_TYPE::FAST_RUN);
	}

	boostChaseTimer_ += ctx.deltaTime;

	VECTOR boostTarget =
		VSub(ctx.playerPos, VScale(ctx.horizontalDir, kBoostStopDistance));

	VECTOR boostDir = VSub(boostTarget, transform_.pos);
	float boostDistance = VSize(boostDir);

	if (boostDistance > kEpsilon)
	{
		boostDir = VNorm(boostDir);
	}

	// 近づいたか、時間切れなら攻撃へ
	if (boostDistance <= kArriveDistance ||
		boostChaseTimer_ >= BOOST_CHASE_TIME)
	{
		CancelBoostChase();

		aiState_ = AI_STATE::ATTACK;
		aiTimer_ = 0.0f;

		animationController_->Play((int)ANIM_TYPE::IDLE);
		return;
	}

	float boostMove = boostDistance;
	const float boostMax = BOOST_CHASE_SPEED * ctx.deltaTime * 60.0f;

	if (boostMove > boostMax)
	{
		boostMove = boostMax;
	}

	transform_.pos = VAdd(transform_.pos, VScale(boostDir, boostMove));

	animationController_->Play((int)ANIM_TYPE::FAST_RUN);
}

// 攻撃距離まで近づいて、4連撃を開始する
void MeleeEnemy::UpdateAIAttack(const AIContext& ctx)
{
	aiTimer_ += ctx.deltaTime;

	// 追っても追いつけないときは諦めて、別の行動(気弾・突進・横移動など)へ移る
	if (aiTimer_ >= kAttackGiveUpTime)
	{
		attackCoolTimer_ = kAttackGiveUpCool;
		ReturnToWait();
		return;
	}

	VECTOR attackPos =
		VSub(ctx.playerPos, VScale(ctx.horizontalDir, kAttackDistance));

	VECTOR attackMoveDir = VSub(attackPos, transform_.pos);
	float attackMoveDistance = VSize(attackMoveDir);

	if (attackMoveDistance > kEpsilon)
	{
		attackMoveDir = VNorm(attackMoveDir);
	}

	if (attackMoveDistance > kAttackStartSlack)
	{
		float moveAmount = attackMoveDistance;
		const float approachMax = kAttackApproachSpeed * ctx.deltaTime * 60.0f;

		if (moveAmount > approachMax)
		{
			moveAmount = approachMax;
		}

		transform_.pos = VAdd(transform_.pos, VScale(attackMoveDir, moveAmount));

		animationController_->Play((int)ANIM_TYPE::RUN);
		return;
	}

	// クールタイム中なら待機へ戻る
	if (attackCoolTimer_ > 0.0f)
	{
		SetAIState(AI_STATE::WAIT);

		animationController_->Play((int)ANIM_TYPE::IDLE);
		return;
	}

	// 攻撃距離まで来たので攻撃開始
	StartMeleeAttack();
}
