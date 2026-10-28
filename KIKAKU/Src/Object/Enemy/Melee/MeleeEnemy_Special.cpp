#include <DxLib.h>
#include <cmath>
#include <algorithm>
#include "MeleeEnemy.h"
#include "../../../Application.h"
#include "../../Player/Player.h"
#include "../../../Manager/Camera.h"
#include "../../../Manager/EffekseerEffect.h"
#include "../../../Manager/ResourceManager.h"
#include "../../../Manager/SceneManager.h"
#include "../../../Utility/AsoUtility.h"
#include "MeleeEnemy_Internal.h"

using namespace MeleeEnemyInternal;

// 気を溜めている間の処理。溜め終わったら気弾を撃つ
bool MeleeEnemy::UpdateKiCharge(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::KI_CHARGE)
	{
		return false;
	}

	animationController_->Play((int)ANIM_TYPE::IDLE);

	LookAtPlayerWithPitchLimit(playerPos);

	aiTimer_ += deltaTime;

	if (aiTimer_ >= kKiChargeTime)
	{
		FireKiBlast();

		kiCoolTimer_ = kKiCoolTime;

		// 撃ったあとに少し隙を作る
		SetAIState(AI_STATE::WAIT);
		aiTimer_ = -kRecoverTime;
	}

	transform_.Update();
	return true;
}

// プレイヤーへ向けて気弾を撃つ
void MeleeEnemy::FireKiBlast(void)
{
	const VECTOR forward = transform_.quaRot.GetForward();
	const VECTOR origin = VAdd(GetChestPos(), VScale(forward, 50.0f));

	const VECTOR target =
		VAdd(player_.GetTransform().pos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	EnemyKiBlast blast;
	blast.Init(origin, VSub(target, origin), kKiSpeed, kKiRadius, kKiLife);

	blasts_.push_back(blast);
}

// 撃った気弾を動かして、プレイヤーに当たったか調べる
void MeleeEnemy::UpdateBlasts(float deltaTime)
{
	const VECTOR playerCenter =
		VAdd(player_.GetTransform().pos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	for (auto& blast : blasts_)
	{
		blast.Update(deltaTime);

		if (blast.IsDead() || blast.IsFrozen())
		{
			continue;
		}

		const float distance = VSize(VSub(blast.GetPos(), playerCenter));

		if (distance > blast.GetRadius() + kBlastHitMargin)
		{
			continue;
		}

		// 当たったら消す(かわされたときは、そのまま飛び続ける)
		if (ApplyHitToPlayer(blast.GetPos(), blast.GetDamage(), blast.GetKnock(), 25.0f, blast.GetKnock() * 0.25f))
		{
			blast.Kill();
		}
	}

	// 消えた気弾を片付ける
	blasts_.erase(
		std::remove_if(blasts_.begin(), blasts_.end(),
			[](const EnemyKiBlast& b) { return b.IsDead(); }),
		blasts_.end());
}

// ビーム(相殺勝負用)
bool MeleeEnemy::GetBeam(VECTOR& origin, VECTOR& dir, float& radius, int& damage) const
{
	if (aiState_ != AI_STATE::BEAM_FIRE)
	{
		return false;
	}

	origin = beamOrigin_;
	dir = beamDir_;
	radius = kBeamRadius;
	damage = kBeamDamage;
	return true;
}

void MeleeEnemy::SetBeamClash(bool clash)
{
	beamClash_ = clash;
	clashLock_ = clash;
}

void MeleeEnemy::EndBeam(void)
{
	beamClash_ = false;
	clashLock_ = false;
	beamLocked_ = false;
	beamCoolTimer_ = kBeamCoolTime;

	// 撃ったあとと同じ、大きな隙
	SetAIState(AI_STATE::WAIT);
	aiTimer_ = -kBeamRecoverTime;
}

// 大きな気弾(相殺勝負用)
bool MeleeEnemy::GetBigBlast(VECTOR& pos, float& radius, int& damage) const
{
	for (const auto& blast : blasts_)
	{
		if (!blast.IsDead() && blast.IsBig())
		{
			pos = blast.GetPos();
			radius = blast.GetRadius();
			damage = blast.GetDamage();
			return true;
		}
	}

	return false;
}

void MeleeEnemy::SetBigBlastFrozen(bool frozen)
{
	clashLock_ = frozen;

	for (auto& blast : blasts_)
	{
		if (blast.IsBig())
		{
			blast.SetFrozen(frozen);
		}
	}
}

void MeleeEnemy::KillBigBlast(void)
{
	clashLock_ = false;

	for (auto& blast : blasts_)
	{
		if (blast.IsBig())
		{
			blast.SetFrozen(false);
			blast.Kill();
		}
	}
}

void MeleeEnemy::SetBigBlastPos(const VECTOR& pos)
{
	for (auto& blast : blasts_)
	{
		if (!blast.IsDead() && blast.IsBig())
		{
			blast.SetPos(pos);
		}
	}
}

void MeleeEnemy::CancelAttacksForHeatUp(void)
{
	// すでに撃っている気弾と、範囲攻撃を全て消す(停止させる)
	blasts_.clear();
	areaBlasts_.clear();

	// 技の途中だった場合に備えて、状態を待機状態に戻しておく
	EndBeam();
}

//----------------------------------------------------------------------
// 突進
//----------------------------------------------------------------------

// 溜め → 警告の球を出す
bool MeleeEnemy::UpdateBigCharge(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::BIG_CHARGE)
	{
		return false;
	}

	animationController_->Play((int)ANIM_TYPE::IDLE);

	LookAtPlayerWithPitchLimit(playerPos);

	aiTimer_ += deltaTime;

	// 溜めている間は、ゆっくり浮き上がる
	transform_.pos.y += kBigRiseSpeed * deltaTime * 60.0f;

	if (transform_.pos.y > kMaxHeight)
	{
		transform_.pos.y = kMaxHeight;
	}

	if (aiTimer_ >= kBigChargeTime)
	{
		LaunchAreaBlasts(playerPos);

		bigCoolTimer_ = kBigCoolTime;

		// 撃ったあとの隙
		SetAIState(AI_STATE::WAIT);
		aiTimer_ = -kBigRecoverTime;
	}

	transform_.Update();
	return true;
}

// プレイヤーの今いる場所に警告の球を出す。HPが半分以下なら、左右にもずらして出す
void MeleeEnemy::LaunchAreaBlasts(const VECTOR& playerPos)
{
	const bool enraged = (hp_ * 2 <= maxHp_);
	const int count = enraged ? 3 : 1;

	const VECTOR center = VAdd(playerPos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	const VECTOR toPlayer =
		HorizontalDirOrForward(VSub(playerPos, transform_.pos), transform_.quaRot);
	const VECTOR side = VGet(toPlayer.z, 0.0f, -toPlayer.x);

	for (int i = 0; i < count; i++)
	{
		// 0番目はプレイヤーの位置、1・2番目は左右
		float offset = 0.0f;

		if (i == 1) { offset = kBigSpread; }
		if (i == 2) { offset = -kBigSpread; }

		EnemyAreaBlast blast;
		blast.Init(
			VAdd(center, VScale(side, offset)),
			kBigRadius,
			kBigWarnTime + kBigStagger * (float)i,
			kBigDamage);

		areaBlasts_.push_back(blast);
	}
}

// 範囲攻撃を進めて、爆発したときにプレイヤーが範囲内ならダメージを与える
void MeleeEnemy::UpdateAreaBlasts(float deltaTime)
{
	const VECTOR playerCenter =
		VAdd(player_.GetTransform().pos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	for (auto& blast : areaBlasts_)
	{
		blast.Update(deltaTime);

		if (!blast.ConsumeExplode())
		{
			continue;
		}

		mainCamera.StartShake(0.2f, 6.0f);

		const float distance = VSize(VSub(blast.GetPos(), playerCenter));

		if (blast.GetDamage() > 0 && distance <= blast.GetRadius())
		{
			ApplyHitToPlayer(blast.GetPos(), blast.GetDamage(), 35.0f, 80.0f, 8.0f);
		}
	}

	areaBlasts_.erase(
		std::remove_if(areaBlasts_.begin(), areaBlasts_.end(),
			[](const EnemyAreaBlast& b) { return b.IsDead(); }),
		areaBlasts_.end());
}

//----------------------------------------------------------------------
// ガード崩し(投げ)
//----------------------------------------------------------------------

// ビームの出る位置(胸の前)
VECTOR MeleeEnemy::GetBeamOrigin(void) const
{
	return VAdd(GetChestPos(), VScale(transform_.quaRot.GetForward(), kBeamBallOffset));
}

// 溜め(途中で狙いを固定) → 発射
bool MeleeEnemy::UpdateBeam(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::BEAM_CHARGE && aiState_ != AI_STATE::BEAM_FIRE)
	{
		return false;
	}

	animationController_->Play((int)ANIM_TYPE::IDLE);

	// 相殺勝負中は、ビームも時間も止める
	if (beamClash_ && aiState_ == AI_STATE::BEAM_FIRE)
	{
		transform_.Update();
		return true;
	}

	aiTimer_ += deltaTime;

	const VECTOR playerCenter = VAdd(playerPos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	if (aiState_ == AI_STATE::BEAM_CHARGE)
	{
		if (!beamLocked_)
		{
			// 狙いを固定するまでは、プレイヤーを追って見る
			LookAtPlayerWithPitchLimit(playerPos);

			if (aiTimer_ >= kBeamLockTime)
			{
				beamOrigin_ = GetBeamOrigin();

				VECTOR dir = VSub(playerCenter, beamOrigin_);
				beamDir_ = (VSize(dir) > kEpsilon) ? VNorm(dir) : VGet(0.0f, 0.0f, 1.0f);

				transform_.quaRot = Quaternion::LookRotation(beamDir_);
				beamLocked_ = true;
			}
		}

		if (aiTimer_ >= kBeamChargeTime)
		{
			// 発射
			if (!beamLocked_)
			{
				beamOrigin_ = GetBeamOrigin();
				beamDir_ = transform_.quaRot.GetForward();
			}

			SetAIState(AI_STATE::BEAM_FIRE);
			beamHitCool_ = 0.0f;

			mainCamera.StartShake(kBeamFireTime, 4.0f);
		}
	}
	else
	{
		// 発射中は、狙いがプレイヤーを少しだけ追う(その場に居続けると当たる)
		VECTOR desired = VSub(playerCenter, beamOrigin_);

		if (VSize(desired) > kEpsilon)
		{
			desired = VNorm(desired);

			const VECTOR blended =
				VAdd(beamDir_, VScale(VSub(desired, beamDir_), Saturate(kBeamTrackRate * deltaTime)));

			if (VSize(blended) > kEpsilon)
			{
				beamDir_ = VNorm(blended);
			}
		}

		// ビームの帯の中にプレイヤーがいれば当たる(当たり続けると、一定間隔でダメージ)
		CountDown(beamHitCool_, deltaTime);

		if (beamHitCool_ <= 0.0f)
		{
			const VECTOR beamEnd = VAdd(beamOrigin_, VScale(beamDir_, kBeamLength));

			if (DistanceToSegment(playerCenter, beamOrigin_, beamEnd) <= kBeamRadius + kBeamHitMargin)
			{
				if (ApplyHitToPlayer(beamOrigin_, kBeamDamage, kBeamKnock, 120.0f, 6.0f))
				{
					beamHitCool_ = kBeamTickTime;
				}
			}
		}

		if (aiTimer_ >= kBeamFireTime)
		{
			beamCoolTimer_ = kBeamCoolTime;
			beamLocked_ = false;

			// 撃ったあとは大きな隙
			SetAIState(AI_STATE::WAIT);
			aiTimer_ = -kBeamRecoverTime;
		}
	}

	transform_.Update();
	return true;
}

//----------------------------------------------------------------------
// 気弾の連射
//----------------------------------------------------------------------

// 上へ昇りながら溜めてから、短い間隔で何発も撃つ
bool MeleeEnemy::UpdateKiBarrage(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::KI_BARRAGE)
	{
		return false;
	}

	animationController_->Play((int)ANIM_TYPE::IDLE);

	LookAtPlayerWithPitchLimit(playerPos);

	aiTimer_ += deltaTime;

	// 溜め: 赤いオーラをまとって、上へ昇る
	if (aiTimer_ < kBarrageChargeTime)
	{
		transform_.pos.y += kBarrageRiseSpeed * deltaTime;

		if (transform_.pos.y > kMaxHeight) { transform_.pos.y = kMaxHeight; }

		transform_.Update();
		return true;
	}

	barrageShotTimer_ -= deltaTime;

	if (barrageShotsLeft_ > 0 && barrageShotTimer_ <= 0.0f)
	{
		FireBarrageShot();

		barrageShotsLeft_--;
		barrageShotTimer_ = kBarrageInterval;
	}

	// 連射が終わったら、大きな気弾を溜めて撃つ
	if (barrageShotsLeft_ <= 0 && barrageShotTimer_ <= 0.0f)
	{
		if (!barrageBigPhase_)
		{
			barrageBigPhase_ = true;
			ShowSkillName(kNameBarrageBig);
			barrageBigTimer_ = 0.0f;
		}

		barrageBigTimer_ += deltaTime;

		if (barrageBigTimer_ >= kBarrageBigChargeTime)
		{
			FireBarrageBig();

			barrageBigPhase_ = false;
			barrageCoolTimer_ = kBarrageCoolTime;

			SetAIState(AI_STATE::WAIT);
			aiTimer_ = -kBarrageRecoverTime;
		}
	}

	transform_.Update();
	return true;
}

// 1発撃つ。花火のように、敵の周りから外へ散らして撃ち、あとから曲がって降ってくる
//   プレイヤーを追いかける弾は一部だけ。ほとんどは「撃った時のプレイヤー周辺」へ降るので、動けば避けられる
void MeleeEnemy::FireBarrageShot(void)
{
	const float os = kBarrageOriginSpread;

	// 弾が出る位置: 敵の周りでばらつかせる
	const VECTOR origin = VAdd(
		GetChestPos(),
		VGet(
			(float)(GetRand((int)(os * 2)) - (int)os),
			(float)GetRand((int)os),
			(float)(GetRand((int)(os * 2)) - (int)os)));

	// 最初に飛ぶ向き: 敵を中心に外側・やや上へ放射状に散らす
	const float yaw = (float)GetRand(359) * DX_PI_F / 180.0f;
	const float up = 0.25f + (float)GetRand(75) / 100.0f;
	const VECTOR launchDir = VGet(sinf(yaw), up, cosf(yaw));

	EnemyKiBlast blast;
	blast.Init(origin, launchDir, kBarrageSpeed, kKiRadius * 1.3f, kKiLife);
	blast.SetDamage(kBarrageDamage);
	blast.SetKnock(0.0f);	// 連射は押し出さない(数が多く、押されすぎて抜けられなくなるため)

	if (GetRand(99) < kBarrageDirectChance)
	{
		// プレイヤーを追いかける弾(少しだけ)
		blast.SetHoming(&player_.GetTransform().pos, kPlayerCenterHeight, kBarrageHomingTurn, kBarrageHomingTime);
		blast.SetHomingDelay(kBarrageHomingDelay);
	}
	else
	{
		// 撃った時のプレイヤー周辺のどこかへ降る弾(追わない)
		const int spread = (int)kBarrageSpread;
		VECTOR goal = VAdd(
			VAdd(player_.GetTransform().pos, VGet(0.0f, kPlayerCenterHeight, 0.0f)),
			VGet(
				(float)(GetRand(spread * 2) - spread),
				(float)(GetRand(spread * 2) - spread) * 0.6f,
				(float)(GetRand(spread * 2) - spread)));

		const float floorY = player_.GetTransform().pos.y + 20.0f;
		if (goal.y < floorY) { goal.y = floorY; }

		blast.SetFixedGoal(goal, kBarrageHomingDelay, kBarrageHomingTurn, kBarrageHomingTime);
	}

	blasts_.push_back(blast);
}

// 大きな気弾を撃つ(遅いが大きく、追尾する)
void MeleeEnemy::FireBarrageBig(void)
{
	const VECTOR forward = transform_.quaRot.GetForward();
	const VECTOR origin = VAdd(GetChestPos(), VScale(forward, 80.0f));

	const VECTOR target = VAdd(player_.GetTransform().pos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	EnemyKiBlast blast;
	blast.Init(origin, VSub(target, origin), kBarrageBigSpeed, kBarrageBigRadius, kKiLife + 1.0f);
	blast.SetDamage(kBarrageBigDamage);
	blast.SetKnock(4.0f);
	blast.SetBig(true);
	blast.SetHoming(&player_.GetTransform().pos, kPlayerCenterHeight, kBarrageBigHomingTurn, kBarrageBigHomingTime);

	blasts_.push_back(blast);
}

//----------------------------------------------------------------------
// 急降下(上昇 → 狙いを固定 → 急降下 → 衝撃波)
//----------------------------------------------------------------------
bool MeleeEnemy::UpdateSlam(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::SLAM_RISE && aiState_ != AI_STATE::SLAM_DIVE)
	{
		return false;
	}

	aiTimer_ += deltaTime;

	const VECTOR playerCenter = VAdd(playerPos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	if (aiState_ == AI_STATE::SLAM_RISE)
	{
		animationController_->Play((int)ANIM_TYPE::IDLE);

		if (!slamLocked_)
		{
			// 上昇して溜める
			float targetY = playerPos.y + kSlamHeight;

			if (targetY > kMaxHeight) { targetY = kMaxHeight; }

			if (transform_.pos.y < targetY)
			{
				transform_.pos.y += kSlamRiseSpeed * deltaTime * 60.0f;

				if (transform_.pos.y > targetY) { transform_.pos.y = targetY; }
			}

			LookAtPlayerWithPitchLimit(playerPos);

			if (aiTimer_ >= kSlamChargeTime)
			{
				// 狙いを固定(ここからは、プレイヤーを追わない)
				slamTarget_ = playerCenter;
				slamLocked_ = true;
			}
		}
		else
		{
			VECTOR dir = VSub(slamTarget_, transform_.pos);

			if (VSize(dir) > kEpsilon)
			{
				transform_.quaRot = Quaternion::LookRotation(VNorm(dir));
			}

			if (aiTimer_ >= kSlamChargeTime + kSlamLockTime)
			{
				slamDir_ = (VSize(dir) > kEpsilon) ? VNorm(dir) : VGet(0.0f, -1.0f, 0.0f);

				SetAIState(AI_STATE::SLAM_DIVE);
				animationController_->Play((int)ANIM_TYPE::FAST_RUN);
			}
		}

		transform_.Update();
		return true;
	}

	// 急降下
	transform_.pos = VAdd(transform_.pos, VScale(slamDir_, kSlamDiveSpeed * deltaTime * 60.0f));

	if (transform_.pos.y < kMinHeight) { transform_.pos.y = kMinHeight; }

	transform_.quaRot = Quaternion::LookRotation(slamDir_);

	// 降下中にプレイヤーへ直接当たったか
	bool directHit = false;

	if (VSize(VSub(playerCenter, transform_.pos)) <= kSlamHitRange)
	{
		directHit = ApplyHitToPlayer(transform_.pos, kSlamDamage, 40.0f, 100.0f, 7.0f);
	}

	// 狙った場所に着いた / 通り過ぎた / 時間切れ → 衝撃波
	const VECTOR toTarget = VSub(slamTarget_, transform_.pos);
	const bool arrived =
		VDot(toTarget, slamDir_) <= 0.0f ||
		VSize(toTarget) <= kSlamArrive ||
		transform_.pos.y <= kMinHeight;

	if (directHit || arrived || aiTimer_ >= kSlamDiveMaxTime)
	{
		SlamImpact(directHit);
	}

	transform_.Update();
	return true;
}

// 衝撃波を出して、様子見に戻る(直接当てたときは、衝撃波のダメージは入れない)
void MeleeEnemy::SlamImpact(bool directHit)
{
	EnemyAreaBlast blast;
	blast.Init(transform_.pos, kSlamImpactRadius, 0.01f, directHit ? 0 : kSlamImpactDamage);
	areaBlasts_.push_back(blast);

	mainCamera.StartShake(0.3f, 10.0f);

	slamCoolTimer_ = kSlamCoolTime;
	slamLocked_ = false;

	SetAIState(AI_STATE::WAIT);
	aiTimer_ = -kSlamRecoverTime;

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

// 大技・突進などの最中か
bool MeleeEnemy::IsDoingSpecial(void) const
{
	return aiState_ == AI_STATE::KI_CHARGE ||
		aiState_ == AI_STATE::RUSH_READY ||
		aiState_ == AI_STATE::RUSH ||
		aiState_ == AI_STATE::BIG_CHARGE ||
		aiState_ == AI_STATE::GRAB_READY ||
		aiState_ == AI_STATE::CHASE_VANISH ||
		aiState_ == AI_STATE::CHASE_APPEAR ||
		aiState_ == AI_STATE::BEAM_CHARGE ||
		aiState_ == AI_STATE::BEAM_FIRE ||
		aiState_ == AI_STATE::KI_BARRAGE ||
		aiState_ == AI_STATE::SLAM_RISE ||
		aiState_ == AI_STATE::SLAM_DIVE ||
		aiState_ == AI_STATE::BOOST_ATTACK;
}
