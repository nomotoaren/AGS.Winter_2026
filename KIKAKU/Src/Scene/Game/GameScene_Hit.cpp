#include <DxLib.h>
#include <cmath>
#include <cfloat>
#include <vector>
#include <cstdio>
#include "../../Application.h"
#include "../../Utility/AsoUtility.h"
#include "../../Utility/DamageCalc.h"
#include "../../Manager/SceneManager.h"
#include "../../Manager/Camera/Camera.h"
#include "../../Manager/InputManager.h"
#include "../../Manager/PadInput.h"
#include "../../Manager/EffekseerEffect.h"
#include "../../Manager/AppFlow.h"
#include "../../Manager/PlayRecord.h"
#include "../../Manager/GameProgress.h"
#include "../../Manager/StageData.h"
#include "../../Common/MenuUI.h"
#include "../../Object/Common/Capsule.h"
#include "../../Object/Common/Collider.h"
#include "../../Object/SkyDome.h"
#include "../../Object/Stage.h"
#include "../../Object/Player/Player.h"
#include "../../Object/Enemy/EnemyBase.h"
#include "../../Object/Enemy/BreakEffect.h"
#include "../../Object/Planet.h"
#include "GameScene.h"
#include "ClashSystem.h"
#include "GameScene_Internal.h"

using namespace GameSceneInternal;

//-----------------------------------------------------------------------------
// 敵と体
//-----------------------------------------------------------------------------

void GameScene::UpdateEnemies(float dt)
{
	bool updateEnemy = true;

	// ジャスト回避のスロー中は、数フレームに1回だけ更新する
	if (justSlowTimer_ > 0.0f)
	{
		justSlowTimer_ -= dt;
		justSlowFrame_++;

		updateEnemy = (justSlowFrame_ % PlayerConfig::JustDodge::SLOW_DIV) == 0;
	}

	// ヒートアップ発動の演出中は、敵も止まる
	if (player_->IsHeatActivating())
	{
		updateEnemy = false;
	}

	if (!updateEnemy)
	{
		return;
	}

	for (auto& enemy : enemies_)
	{
		if (enemy && !enemy->IsDead())
		{
			enemy->Update();
		}
	}
}

// プレイヤーと敵が重ならないように押し出す
void GameScene::PushOutBodies(void)
{
	for (auto& enemy : enemies_)
	{
		if (!enemy || enemy->IsDead())
		{
			continue;
		}

		const VECTOR playerPos = player_->GetTransform().pos;
		const VECTOR enemyPos = enemy->GetTransform().pos;

		// 上や下に離れているときは重なりとみなさない
		if (std::fabs(enemyPos.y - playerPos.y) > BODY_HEIGHT_RANGE)
		{
			continue;
		}

		// 敵 -> プレイヤー の水平方向
		VECTOR diff = VSub(playerPos, enemyPos);
		diff.y = 0.0f;

		const float dist = VSize(diff);

		if (dist >= BODY_DISTANCE)
		{
			continue;
		}

		VECTOR dir;

		if (dist > 0.001f)
		{
			dir = VScale(diff, 1.0f / dist);
		}
		else
		{
			// ぴったり重なっているときは、プレイヤーの後ろ側へ逃がす
			VECTOR forward = player_->GetForward();
			forward.y = 0.0f;

			dir = VScale(NormalizeOr(forward, VGet(0.0f, 0.0f, 1.0f)), -1.0f);
		}

		float overlap = BODY_DISTANCE - dist;

		if (overlap > BODY_MAX_PUSH)
		{
			overlap = BODY_MAX_PUSH;
		}

		player_->PushOut(VScale(dir, overlap * BODY_PLAYER_RATE));
		enemy->PushOut(VScale(dir, -overlap * (1.0f - BODY_PLAYER_RATE)));
	}
}

//-----------------------------------------------------------------------------
// 当たり判定
//-----------------------------------------------------------------------------

// 近接攻撃
void GameScene::UpdateMeleeHit(void)
{
	if (!player_->IsAttackHitTiming() || player_->HasAttackHit())
	{
		return;
	}

	const int combo = player_->GetCombo();
	const bool isFinish = (combo == FINISH_COMBO);

	// 形態に応じたダメージ(変身すると倍率が変わる)
	const int attackDamage = player_->GetAttackDamage(Player::ATTACK_DAMAGE);

	// 攻撃範囲(水平距離)と高さの許容範囲
	const float attackRange = isFinish ? FINISH_RANGE : ActorBase::ATTACK_RANGE;
	const float attackHeight = isFinish ? FINISH_HEIGHT_RANGE : ATTACK_HEIGHT_RANGE;

	const VECTOR playerPos = player_->GetTransform().pos;

	VECTOR forward = player_->GetForward();
	forward.y = 0.0f;
	const bool hasForward = VSize(forward) > 0.001f;

	if (hasForward)
	{
		forward = VNorm(forward);
	}

	for (auto& enemy : enemies_)
	{
		// 回避中(無敵)の敵には当たらない
		if (!enemy || enemy->IsDead() || enemy->IsInvincible())
		{
			continue;
		}

		const VECTOR toEnemy = VSub(enemy->GetTransform().pos, playerPos);

		// 水平距離と高さの差を別々に見る
		// (3D距離だと、敵が上下にいるだけで届かなくなるため)
		VECTOR horizontal = toEnemy;
		horizontal.y = 0.0f;

		const float horizontalDist = VSize(horizontal);

		if (horizontalDist > attackRange ||
			std::fabs(toEnemy.y) > attackHeight)
		{
			continue;
		}

		// 向き判定(真上・真下など水平距離がほぼ0のときは判定しない)
		if (hasForward && horizontalDist > ATTACK_DOT_MIN_DISTANCE)
		{
			const float dot =
				VDot(forward, VScale(horizontal, 1.0f / horizontalDist));

			if (dot < ActorBase::ATTACK_DOT)
			{
				continue;
			}
		}

		// 敵がガード中(8段目はガードを崩すので貫通)
		if (enemy->IsGuard() && !isFinish)
		{
			// ガード耐久値を減らす(攻撃力が高いほど削れる)
			enemy->GuardDamage(GUARD_DAMAGE * player_->GetAttackRate());

			EffekseerEffect::GetInstance()->PlayHitEffect(HitEffectPos(*enemy), 0.0f);

			StartHitStop(0.03f);

			player_->SetAttackHit();
			return;
		}

		// ヒット
		enemy->LookAtPlayer();

		// フィニッシュは大きくスタン値を溜める
		DealDamage(*enemy, attackDamage, isFinish ? STUN_FINISH : STUN_NORMAL);

		VECTOR hitDir = NormalizeOr(toEnemy, toEnemy);

		// 8段目は下に叩き落とす
		if (isFinish)
		{
			hitDir = VNorm(VGet(forward.x, -0.8f, forward.z));

			enemy->StartSlamDown();

			// 8段目が当たったら高速追撃できる
			player_->SetCanChase(true);
		}

		const float knock = (combo >= 1 && combo <= FINISH_COMBO) ? KNOCK_POWER[combo] : 0.0f;
		enemy->AddKnockBack(hitDir, knock);

		EffekseerEffect::GetInstance()->PlayHitEffect(HitEffectPos(*enemy), 0.0f);

		if (isFinish)
		{
			ApplyHitImpact(0.10f, PlayerConfig::HitShake::FINISH_TIME, PlayerConfig::HitShake::FINISH_POWER);
		}
		else
		{
			ApplyHitImpact(0.05f, PlayerConfig::HitShake::MELEE_TIME, PlayerConfig::HitShake::MELEE_POWER);
		}

		// クリティカルは、少し長く止めて画面を揺らす
		if (player_->IsLastCritical())
		{
			hitStopTimer_ += DamageCalc::CRITICAL_HIT_STOP;
			mainCamera.StartShake(PlayerConfig::HitShake::CRITICAL_TIME, PlayerConfig::HitShake::CRITICAL_POWER);
		}

		player_->SetAttackHit();

		// 1攻撃で1体だけ
		return;
	}
}

// 気弾
void GameScene::UpdateKiBlastHit(void)
{
	for (auto& blast : player_->GetKiBlasts())
	{
		if (blast->IsDead())
		{
			continue;
		}

		for (auto& enemy : enemies_)
		{
			if (!enemy || enemy->IsDead())
			{
				continue;
			}

			if (VSize(VSub(blast->GetPos(), EnemyCenter(*enemy))) > ENEMY_HIT_RADIUS)
			{
				continue;
			}

			DealDamage(*enemy, player_->GetAttackDamage(KI_BLAST_DAMAGE_TYPE), KI_BLAST_STUN);

			mainCamera.StartShake(PlayerConfig::HitShake::KI_BLAST_TIME, PlayerConfig::HitShake::KI_BLAST_POWER);

			blast->Hit();
			break;
		}
	}
}

// 連続エネルギー弾: 着弾した弾の爆発と、枠の中の敵へのダメージ
void GameScene::UpdateBarrageHit(void)
{
	PlayerBarrage& barrage = player_->GetBarrage();

	const VECTOR aim = barrage.GetAimPoint();
	const float range = PlayerBarrage::RingRadius() * 1.5f;

	for (auto& shot : barrage.GetShots())
	{
		if (!shot.exploded || shot.handled)
		{
			continue;
		}

		shot.handled = true;

		// 着弾の光
		EffekseerEffect::GetInstance()->PlayHitEffect(shot.pos, 0.0f);

		// 枠の中にいる敵に当たる(枠の外に逃げられると外れる)
		for (auto& enemy : enemies_)
		{
			if (!enemy || enemy->IsDead())
			{
				continue;
			}

			if (VSize(VSub(EnemyCenter(*enemy), aim)) > range)
			{
				continue;
			}

			DealDamage(*enemy, player_->GetAttackDamage(KI_BLAST_DAMAGE_TYPE),
				PlayerConfig::Barrage::STUN);

			mainCamera.StartShake(PlayerConfig::HitShake::BARRAGE_TIME, PlayerConfig::HitShake::BARRAGE_POWER);

			// 当たった敵をのけぞらせる(連射の間は、怯み続けて動けない)
			VECTOR knockDir = VSub(EnemyCenter(*enemy), player_->GetTransform().pos);
			knockDir.y = 0.0f;
			enemy->AddKnockBack(NormalizeOr(knockDir, player_->GetForward()), 3.0f);

			break;
		}
	}
}

// フィニッシュ気弾
void GameScene::UpdateFinishBlastHit(void)
{
	for (auto& finish : player_->GetFinishBlasts())
	{
		if (finish->IsDead())
		{
			continue;
		}

		for (auto& enemy : enemies_)
		{
			if (!enemy || enemy->IsDead())
			{
				continue;
			}

			const VECTOR center = EnemyCenter(*enemy);

			if (VSize(VSub(finish->GetPos(), center)) > finish->GetRadius() + ENEMY_HIT_RADIUS)
			{
				continue;
			}

			DealDamage(*enemy,
				player_->GetAttackDamage(PlayerConfig::Finish::DAMAGE),
				PlayerConfig::Finish::STUN);

			enemy->AddKnockBack(finish->GetDir(), PlayerConfig::Finish::KNOCK);

			EffekseerEffect::GetInstance()->PlayHitEffect(center, 0.0f);

			ApplyHitImpact(
				FINISH_BLAST_HIT_STOP,
				FINISH_BLAST_SHAKE_TIME,
				FINISH_BLAST_SHAKE_POWER);

			finish->Kill();

			// (追撃はブレイクしたときだけ)
			break;
		}
	}
}

// かめはめ波
void GameScene::UpdateKamehameHit(float dt)
{
	if (!player_->IsKamehameBeam())
	{
		// ビームが終わったら、敵の「ビームを受けている」状態を戻す
		kamehameDamageTimer_ = 0.0f;

		for (auto& enemy : enemies_)
		{
			if (enemy)
			{
				enemy->SetKamehameHit(false);
			}
		}

		return;
	}

	kamehameDamageTimer_ += dt;

	// 相殺勝負の最中は、ビームのダメージは入らない
	if (clash_->IsActive())
	{
		return;
	}

	const VECTOR beamStart = player_->GetKamehameStartPos();
	const VECTOR beamVec = VSub(player_->GetKamehameEndPos(), beamStart);
	const float beamLengthSq = VDot(beamVec, beamVec);

	if (beamLengthSq <= 0.001f)
	{
		return;
	}

	const float hitRadius = player_->GetKamehameRadius() + ENEMY_HIT_RADIUS;

	const VECTOR pushDir = NormalizeOr(player_->GetForward(), player_->GetForward());

	for (auto& enemy : enemies_)
	{
		if (!enemy || enemy->IsDead())
		{
			continue;
		}

		// 最初は「ビームに当たっていない」にする
		enemy->SetKamehameHit(false);

		// ビームの線(始点から終点)の上で、敵の中心に一番近い点
		const VECTOR center = EnemyCenter(*enemy);

		float t = VDot(VSub(center, beamStart), beamVec) / beamLengthSq;

		if (t < 0.0f) { t = 0.0f; }
		if (t > 1.0f) { t = 1.0f; }

		const VECTOR closest = VAdd(beamStart, VScale(beamVec, t));

		// ビームに当たっていない
		if (VSize(VSub(center, closest)) > hitRadius)
		{
			continue;
		}

		// ビームに当たっている
		enemy->SetKamehameHit(true, pushDir);

		// ダメージだけ一定間隔ごと
		if (kamehameDamageTimer_ >= BEAM_DAMAGE_INTERVAL)
		{
			DealDamage(*enemy, player_->GetAttackDamage(BEAM_DAMAGE_TYPE), BEAM_STUN);

			mainCamera.StartShake(PlayerConfig::HitShake::BEAM_TIME, PlayerConfig::HitShake::BEAM_POWER);

			kamehameDamageTimer_ = 0.0f;
		}
	}
}

//-----------------------------------------------------------------------------
// 共通の部品
//-----------------------------------------------------------------------------

void GameScene::DealDamage(EnemyBase& enemy, int damage, float stun)
{
	// レベルが高いほど、与えるダメージが増える
	int scaled = static_cast<int>(damage * GameProgress::AttackRate() + 0.5f);

	if (damage > 0 && scaled < 1)
	{
		scaled = 1;
	}

	enemy.Damage(scaled);
	enemy.AddStun(stun);

	player_->AddDamagePopup(PopupPos(enemy), scaled);
}

void GameScene::StartHitStop(float time)
{
	isHitStop_ = true;
	hitStopTimer_ = time;
}

void GameScene::ApplyHitImpact(float stopTime, float shakeTime, float shakePower)
{
	StartHitStop(stopTime);
	mainCamera.StartShake(shakeTime, shakePower);
}

bool GameScene::IsAllEnemiesDead(void) const
{
	// 敵が1体もいないステージ(作りかけのステージ)は、クリアにしない
	if (enemies_.empty())
	{
		return false;
	}

	for (const auto& enemy : enemies_)
	{
		if (enemy && !enemy->IsDead())
		{
			return false;
		}
	}

	return true;
}
