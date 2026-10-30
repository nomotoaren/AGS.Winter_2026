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

// プレイヤーが攻撃を始めたときに、1回だけ「かわす / ガードする / 何もしない」を決める
bool MeleeEnemy::TryReactToPlayerAttack(float distance)
{
	// 必殺技の最中は、ガードや回避に切り替えない
	if (IsSuperArmor())
	{
		return false;
	}

	if (distance > kReactDistance)
	{
		return false;
	}

	const int roll = GetRand(99);

	if (dodgeCoolTimer_ <= 0.0f && roll < kDodgeChance)
	{
		StartDodge();
		return true;
	}

	if (guardCoolTimer_ <= 0.0f && roll < kDodgeChance + kGuardChance)
	{
		StartGuard();
		return true;
	}

	return false;
}

void MeleeEnemy::StartGuard(void)
{
	isGuard_ = true;
	guardTimer_ = kGuardTime;

	ResetAttack();
	CancelBoostChase();
	CancelSpecials();

	SetAIState(AI_STATE::WAIT);

	animationController_->Play((int)ANIM_TYPE::GUARD, true, 0.0f, -1.0f, true, true);

	transform_.Update();
}

// プレイヤーの背後へ瞬間移動して、すぐ殴り返す
void MeleeEnemy::StartDodge(void)
{
	dodgeCoolTimer_ = kDodgeCoolTime;
	invincibleTimer_ = kDodgeInvincibleTime;
	justDodged_ = true;

	ResetAttack();
	CancelBoostChase();
	CancelSpecials();

	// プレイヤーの背後へ
	const VECTOR playerPos = player_.GetTransform().pos;
	const VECTOR forward = HorizontalDirOrForward(player_.GetForward(), transform_.quaRot);

	VECTOR newPos = VSub(playerPos, VScale(forward, kDodgeBehindDistance));
	newPos.y = playerPos.y;

	if (newPos.y < kMinHeight)
	{
		newPos.y = kMinHeight;
	}

	transform_.pos = newPos;
	knockBackPow_ = AsoUtility::VECTOR_ZERO;

	LookAtPlayerWithPitchLimit(playerPos);

	// 反撃
	StartMeleeAttack();

	transform_.Update();
}

bool MeleeEnemy::ConsumeDodged(void)
{
	const bool result = justDodged_;
	justDodged_ = false;
	return result;
}

// 近接攻撃を1段目から始める
void MeleeEnemy::StartMeleeAttack(void)
{
	isAttack_ = true;
	attackCombo_ = 1;
	attackTimer_ = 0.0f;
	hasAttackHit_ = false;

	animationController_->Play((int)ANIM_TYPE::ATTACK01, false);
}

// 攻撃中の処理
bool MeleeEnemy::UpdateAttack(float deltaTime, const VECTOR& playerPos)
{
	if (!isAttack_)
	{
		return false;
	}

	attackTimer_ += deltaTime;

	// 1~3段目は攻撃中もプレイヤーについていく
	if (attackCombo_ >= 1 && attackCombo_ <= kFollowComboMax)
	{
		FollowPlayerWhileAttacking(playerPos);
	}

	// 当たり判定のタイミングで、プレイヤーに当たったか調べる
	if (IsAttackHitTiming() && !hasAttackHit_)
	{
		CheckMeleeHit();
	}

	// 次の攻撃へ
	if (attackTimer_ >= kComboInterval)
	{
		AdvanceCombo();
	}

	transform_.Update();
	return true;
}

// 近接攻撃がプレイヤーに当たるか調べて、当てる
void MeleeEnemy::CheckMeleeHit(void)
{
	const VECTOR toPlayer = VSub(player_.GetTransform().pos, transform_.pos);

	if (VSize(ToHorizontal(toPlayer)) > kMeleeHitRange ||
		fabsf(toPlayer.y) > kMeleeHitHeight)
	{
		return;
	}

	// 射程内なら、当たった・防がれた・かわされたのどれでも、この段の判定は終わり
	hasAttackHit_ = true;

	const bool isFinisher = (attackCombo_ >= kMaxCombo);

	ApplyHitToPlayer(
		transform_.pos,
		isFinisher ? kFinisherDamage : kComboDamage,
		isFinisher ? 30.0f : 5.0f,		// 吹き飛ばす強さ
		isFinisher ? 80.0f : 20.0f,		// ガードされたときの、ガード耐久値の減り
		isFinisher ? 5.0f : 1.5f);		// ガードされたときの、押し戻す強さ

	// 4連撃の締めで吹き飛ばしたら、追撃に行く
	if (isFinisher)
	{
		QueueChase();
	}
}

// プレイヤーに攻撃を当てる(かわされたら false)
//   回避・ガード・ダメージの判定は EnemyBase::HitPlayer。ここでは、この敵だけが使う記録を残す
bool MeleeEnemy::ApplyHitToPlayer(const VECTOR& fromPos, int damage, float knockPower,
	float guardDamage, float guardKnock)
{
	const HIT_RESULT result = HitPlayer(fromPos, damage, knockPower, guardDamage, guardKnock);

	lastHitDamaged_ = (result == HIT_RESULT::DAMAGED);

	// ガードされ続けているほど、ガード崩しの技を使いやすくなる
	if (result == HIT_RESULT::GUARDED)
	{
		guardWatchTimer_ += 0.6f;
	}

	return result != HIT_RESULT::DODGED;
}

// 攻撃中、プレイヤーの一定距離の位置へ近づきながら見る
void MeleeEnemy::FollowPlayerWhileAttacking(const VECTOR& playerPos)
{
	VECTOR horizontalDir =
		HorizontalDirOrForward(VSub(playerPos, transform_.pos), transform_.quaRot);

	VECTOR attackPos =
		VSub(playerPos, VScale(horizontalDir, kAttackDistance));

	VECTOR follow = VSub(attackPos, transform_.pos);

	transform_.pos =
		VAdd(transform_.pos, VScale(follow, kAttackFollowRate));

	LookAtPlayerWithPitchLimit(playerPos);
}

// 次の段の攻撃へ。最後まで終わったら待機に戻る
void MeleeEnemy::AdvanceCombo(void)
{
	attackTimer_ = 0.0f;
	hasAttackHit_ = false;

	attackCombo_++;

	if (attackCombo_ <= kMaxCombo)
	{
		// ATTACK01~04 は連番(2段目なら ATTACK02)
		const int animType = (int)ANIM_TYPE::ATTACK01 + (attackCombo_ - 1);
		animationController_->Play(animType, false);
		return;
	}

	// 攻撃終了
	ResetAttack();

	attackCoolTimer_ = ATTACK_COOL_TIME;

	ReturnToWait();
}

// プレイヤーを見る(上下の角度は制限する)
void MeleeEnemy::LookAtPlayerWithPitchLimit(const VECTOR& playerPos)
{
	VECTOR lookDir = VSub(playerPos, transform_.pos);

	float lookDistance = VSize(ToHorizontal(lookDir));

	if (lookDistance > kEpsilon)
	{
		float maxY = lookDistance * kLookPitchRate;

		if (lookDir.y > maxY) { lookDir.y = maxY; }
		if (lookDir.y < -maxY) { lookDir.y = -maxY; }
	}

	if (VSize(lookDir) > kEpsilon)
	{
		transform_.quaRot = Quaternion::LookRotation(VNorm(lookDir));
	}
}

// 構え → 突進
bool MeleeEnemy::UpdateRush(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::RUSH_READY && aiState_ != AI_STATE::RUSH)
	{
		return false;
	}

	aiTimer_ += deltaTime;

	// 構え: その場でプレイヤーを見て溜める(この間は避けやすい)
	if (aiState_ == AI_STATE::RUSH_READY)
	{
		animationController_->Play((int)ANIM_TYPE::IDLE);

		LookAtPlayerWithPitchLimit(playerPos);

		if (aiTimer_ >= kRushReadyTime)
		{
			// 突進する向きは、構えが終わった瞬間のプレイヤーの位置で決まる
			const VECTOR target =
				VAdd(playerPos, VGet(0.0f, kPlayerCenterHeight * 0.5f, 0.0f));

			rushDir_ = VSub(target, transform_.pos);
			rushDir_ = (VSize(rushDir_) > kEpsilon) ? VNorm(rushDir_) : VGet(0.0f, 0.0f, 1.0f);

			aiState_ = AI_STATE::RUSH;
			aiTimer_ = 0.0f;

			animationController_->Play((int)ANIM_TYPE::FAST_RUN);
		}

		transform_.Update();
		return true;
	}

	// 突進中
	transform_.pos = VAdd(transform_.pos, VScale(rushDir_, kRushSpeed * deltaTime * 60.0f));

	if (transform_.pos.y < kMinHeight) { transform_.pos.y = kMinHeight; }
	if (transform_.pos.y > kMaxHeight) { transform_.pos.y = kMaxHeight; }

	transform_.quaRot = Quaternion::LookRotation(rushDir_);

	bool finished = (aiTimer_ >= kRushTime);

	// プレイヤーに当たったら終わり(かわされたら、そのまま通り過ぎる)
	if (VSize(VSub(playerPos, transform_.pos)) <= kRushHitRange)
	{
		if (ApplyHitToPlayer(transform_.pos, kRushDamage, 28.0f, 40.0f, 6.0f))
		{
			finished = true;
			QueueChase();
		}
	}

	if (finished)
	{
		rushCoolTimer_ = kRushCoolTime;

		// 突進のあとは大きな隙になる
		SetAIState(AI_STATE::WAIT);
		aiTimer_ = -kRecoverTime;

		animationController_->Play((int)ANIM_TYPE::IDLE);
	}

	transform_.Update();
	return true;
}

//----------------------------------------------------------------------
// 範囲大技
//----------------------------------------------------------------------

// プレイヤーがガードしている時間を数える(ガードをやめると、ゆっくり減る)
void MeleeEnemy::UpdateGuardWatch(float deltaTime, float distance)
{
	if (player_.IsGuard() && distance < kGuardWatchRange)
	{
		guardWatchTimer_ += deltaTime;
		return;
	}

	CountDown(guardWatchTimer_, deltaTime * 0.5f);
}

// 構え → 接近 → 掴む。ガードしていても防げないが、避けられる
bool MeleeEnemy::UpdateGrab(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::GRAB_READY)
	{
		return false;
	}

	aiTimer_ += deltaTime;

	LookAtPlayerWithPitchLimit(playerPos);

	// 構えの間に、プレイヤーへ素早く近づく
	const VECTOR toPlayer = VSub(playerPos, transform_.pos);
	const float distance = VSize(toPlayer);

	if (distance > kGrabHitRange * 0.6f)
	{
		float step = kGrabApproachSpeed * deltaTime * 60.0f;

		if (step > distance)
		{
			step = distance;
		}

		transform_.pos = VAdd(transform_.pos, VScale(VNorm(toPlayer), step));

		animationController_->Play((int)ANIM_TYPE::FAST_RUN);
	}
	else
	{
		animationController_->Play((int)ANIM_TYPE::IDLE);
	}

	if (aiTimer_ >= kGrabReadyTime)
	{
		DoGrab();
	}

	transform_.Update();
	return true;
}

// 掴む(ガードを割ってダメージ。回避中・離れているときは空振り)
void MeleeEnemy::DoGrab(void)
{
	grabCoolTimer_ = kGrabCoolTime;
	guardWatchTimer_ = 0.0f;

	const VECTOR playerPos = player_.GetTransform().pos;
	const VECTOR toPlayer = VSub(playerPos, transform_.pos);

	bool hit = false;

	if (VSize(toPlayer) <= kGrabHitRange && !player_.IsDodging())
	{
		hit = true;

		VECTOR dir = (VSize(toPlayer) > kEpsilon) ? VNorm(toPlayer) : VGet(0.0f, 0.0f, 1.0f);

		// ガード中なら、ガードを一撃で崩す
		if (player_.IsGuard())
		{
			player_.GuardDamage(9999.0f);
		}

		player_.LookAtDamageEnemy(transform_.pos);
		player_.Damage(kGrabDamage);
		player_.AddKnockBack(dir, kGrabKnock);

		EffekseerEffect::GetInstance()->PlayHitEffect(
			VAdd(playerPos, VGet(0.0f, 80.0f, 0.0f)), 0.0f);
		mainCamera.StartShake(0.2f, 8.0f);
	}

	// 当たっても外しても隙ができる(外したほうが大きい)
	SetAIState(AI_STATE::WAIT);
	aiTimer_ = hit ? -kGrabRecover : -kGrabMissRecover;

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

//----------------------------------------------------------------------
// 追撃
//----------------------------------------------------------------------

// 直前の攻撃でダメージが入っていたら、追撃の予定を入れる
void MeleeEnemy::QueueChase(void)
{
	if (!lastHitDamaged_ || chaseCoolTimer_ > 0.0f)
	{
		return;
	}

	if (GetRand(99) >= kChaseChance)
	{
		return;
	}

	pendingChase_ = true;
	pendingChaseTimer_ = kChasePendingLimit;
}

// 姿を消す(吹き飛ぶ向きの先へ回り込むために)
void MeleeEnemy::StartChase(void)
{
	pendingChase_ = false;
	chaseCoolTimer_ = kChaseCoolTime;

	chaseDir_ = HorizontalDirOrForward(lastHitDir_, transform_.quaRot);
	vanishFrom_ = GetChestPos();

	ResetAttack();
	CancelBoostChase();

	isHidden_ = true;
	invincibleTimer_ = kChaseVanishTime + 0.1f;

	SetAIState(AI_STATE::CHASE_VANISH);
}

// 消える → 先回りして現れる → 叩き落とす
bool MeleeEnemy::UpdateChase(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::CHASE_VANISH && aiState_ != AI_STATE::CHASE_APPEAR)
	{
		return false;
	}

	aiTimer_ += deltaTime;

	if (aiState_ == AI_STATE::CHASE_VANISH)
	{
		if (aiTimer_ >= kChaseVanishTime)
		{
			// 吹き飛ばされているプレイヤーの、進む先へ現れる
			VECTOR newPos = VAdd(playerPos, VScale(chaseDir_, kChaseAppearDistance));
			newPos.y = playerPos.y + kChaseAppearHeight;

			if (newPos.y < kMinHeight) { newPos.y = kMinHeight; }
			if (newPos.y > kMaxHeight) { newPos.y = kMaxHeight; }

			transform_.pos = newPos;
			isHidden_ = false;

			SetAIState(AI_STATE::CHASE_APPEAR);

			LookAtPlayerWithPitchLimit(playerPos);
			animationController_->Play((int)ANIM_TYPE::ATTACK03, false);
		}
	}
	else
	{
		// 現れてから叩くまでの間は、ガードも回避もできる
		LookAtPlayerWithPitchLimit(playerPos);

		if (aiTimer_ >= kChaseWindupTime)
		{
			DoSmash();
		}
	}

	transform_.Update();
	return true;
}

// 上から叩き落とす
void MeleeEnemy::DoSmash(void)
{
	const VECTOR playerPos = player_.GetTransform().pos;

	if (VSize(VSub(playerPos, transform_.pos)) <= kSmashRange)
	{
		// 上から叩くので、プレイヤーは下へ飛ぶ
		ApplyHitToPlayer(
			VAdd(playerPos, VGet(0.0f, kSmashFromHeight, 0.0f)),
			kSmashDamage, 38.0f, 60.0f, 6.0f);
	}

	attackCoolTimer_ = ATTACK_COOL_TIME * 0.6f;

	SetAIState(AI_STATE::WAIT);
	aiTimer_ = -kChaseRecoverTime;

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

//----------------------------------------------------------------------
// 極太ビーム
//----------------------------------------------------------------------
