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
// 技: バニッシュアタック / 追撃 / 相殺 / ガードバースト
//-----------------------------------------------------------------------------

// バニッシュアタック: ガード中に×で、敵の斜め後ろへ瞬間移動し、少し間をおいて叩き落とす
void GameScene::UpdateVanish(float dt)
{
	if (player_->ConsumeVanish())
	{
		StartVanish();
	}

	if (vanishTarget_ == nullptr)
	{
		return;
	}

	// 少し間をおいてから叩き落とす(消えて、現れて、叩く、が目で追えるように)
	vanishTimer_ -= dt;

	if (vanishTarget_->IsDead())
	{
		vanishTarget_ = nullptr;
		return;
	}

	if (vanishTimer_ > 0.0f)
	{
		return;
	}

	EnemyBase* target = vanishTarget_;
	vanishTarget_ = nullptr;

	const int damage = static_cast<int>(
		player_->GetAttackDamage(Player::ATTACK_DAMAGE) * PlayerConfig::Vanish::DAMAGE_RATE);

	DealDamage(*target, damage, PlayerConfig::Vanish::STUN);

	target->LookAtPlayer();
	target->StartSlamDown();

	const VECTOR slamDir = VNorm(VGet(vanishDir_.x, -0.8f, vanishDir_.z));
	target->AddKnockBack(slamDir, PlayerConfig::Vanish::KNOCK);

	EffekseerEffect::GetInstance()->PlayHitEffect(HitEffectPos(*target), 0.0f);

	ApplyHitImpact(
		PlayerConfig::Vanish::HIT_STOP,
		PlayerConfig::Vanish::SHAKE_TIME,
		PlayerConfig::Vanish::SHAKE_POWER);
}

void GameScene::StartVanish(void)
{
	// ロックオン中の敵。いなければ、範囲内で一番近い敵
	EnemyBase* target = PickTarget(PlayerConfig::Vanish::MAX_RANGE);

	if (target == nullptr)
	{
		return;
	}

	const VECTOR enemyPos = target->GetTransform().pos;

	// 敵の向きの後ろ側(水平)
	VECTOR back = target->GetTransform().quaRot.GetForward();
	back.y = 0.0f;
	back = NormalizeOr(back, VGet(0.0f, 0.0f, 1.0f));

	const VECTOR warpPos =
		CalcVanishPos(enemyPos, back, player_->GetTransform().pos);

	player_->VanishTo(warpPos, enemyPos);

	vanishTarget_ = target;
	vanishTimer_ = PlayerConfig::Vanish::HIT_DELAY;
	vanishDir_ = back;
}

// 追撃: ふっ飛ばした敵のところまで飛んでいって、追い打ちする
void GameScene::UpdatePursuit(float dt)
{
	if (player_->ConsumePursuit())
	{
		StartPursuit();
	}

	if (pursuitTarget_ == nullptr)
	{
		return;
	}

	pursuitTimer_ -= dt;

	if (pursuitTarget_->IsDead())
	{
		pursuitTarget_ = nullptr;
		return;
	}

	namespace pu = PlayerConfig::Pursuit;

	// 敵の手前まで、決まった速さで近づく
	const VECTOR enemyNow = pursuitTarget_->GetTransform().pos;
	const VECTOR playerNow = player_->GetTransform().pos;

	VECTOR flat = VSub(enemyNow, playerNow);
	flat.y = 0.0f;
	pursuitDir_ = NormalizeOr(flat, pursuitDir_);

	VECTOR stop = VSub(enemyNow, VScale(pursuitDir_, pu::DISTANCE));
	stop.y = enemyNow.y + pu::HEIGHT;

	if (stop.y < pu::MIN_HEIGHT)
	{
		stop.y = pu::MIN_HEIGHT;
	}

	const VECTOR toStop = VSub(stop, playerNow);
	const float remain = VSize(toStop);
	const float step = pu::DASH_SPEED * dt;

	const bool arrived = (remain <= step) || (pursuitTimer_ <= 0.0f);

	if (arrived)
	{
		player_->DashStepTo(stop, enemyNow);
	}
	else
	{
		player_->DashStepTo(VAdd(playerNow, VScale(toStop, step / remain)), enemyNow);
		return;
	}

	// 到着: 追い打ち
	EnemyBase* target = pursuitTarget_;
	pursuitTarget_ = nullptr;

	const int damage = static_cast<int>(
		player_->GetAttackDamage(Player::ATTACK_DAMAGE) * pu::DAMAGE_RATE);

	DealDamage(*target, damage, pu::STUN);

	target->LookAtPlayer();

	// また遠くへふっ飛ばす
	target->AddKnockBack(VNorm(VGet(pursuitDir_.x, 0.35f, pursuitDir_.z)), pu::KNOCK);

	EffekseerEffect::GetInstance()->PlayHitEffect(HitEffectPos(*target), 0.0f);

	// (追撃はブレイク後の最初の一発だけ。続けての追撃はなし)
	ApplyHitImpact(pu::HIT_STOP, pu::SHAKE_TIME, pu::SHAKE_POWER);
}

void GameScene::StartPursuit(void)
{
	// ロックオン中の敵。いなければ、一番近い敵
	EnemyBase* target = PickTarget(FLT_MAX);

	if (target == nullptr)
	{
		return;
	}

	// 瞬間移動はせず、敵のところまで速く飛んでいく(UpdatePursuit で毎コマ動かす)
	pursuitTarget_ = target;
	pursuitTimer_ = PlayerConfig::Pursuit::HIT_DELAY;
	pursuitDir_ = VGet(0.0f, 0.0f, 1.0f);
}

// 相殺勝負: かめはめ波が敵の大きな気弾・ビームにぶつかったら、連打で押し合う
void GameScene::UpdateClash(float dt)
{
	if (clash_->Update(dt, *player_, enemies_) == ClashSystem::RESULT::WIN)
	{
		// 押し勝った瞬間に、少し止める
		StartHitStop(ClashSystem::WIN_HIT_STOP);
	}
}

void GameScene::UpdateGuardBurst(void)
{
	if (!player_->IsGuardBurstTrigger())
	{
		return;
	}

	mainCamera.StartShake(BURST_SHAKE_TIME, BURST_SHAKE_POWER);

	const VECTOR playerPos = player_->GetTransform().pos;

	for (auto& enemy : enemies_)
	{
		if (!enemy || enemy->IsDead())
		{
			continue;
		}

		VECTOR dir = VSub(enemy->GetTransform().pos, playerPos);
		const float distance = VSize(dir);

		// バーストの範囲外、またはぴったり重なっているとき
		if (distance > BURST_RANGE || distance <= 0.001f)
		{
			continue;
		}

		enemy->GuardBurst();

		enemy->AddKnockBack(VNorm(dir), BURST_KNOCK);
	}
}

//-----------------------------------------------------------------------------
// 演出: ジャスト回避 / ブレイク / ヒートアップ
//-----------------------------------------------------------------------------

// ジャスト回避が成立したら、しばらく敵だけをスローモーションにする
//   (敵を数フレームに1回だけ更新する。プレイヤーは普通に動けるので、反撃できる)
void GameScene::UpdateJustDodge(void)
{
	if (player_->ConsumeJustDodge())
	{
		justSlowTimer_ = PlayerConfig::JustDodge::SLOW_TIME;
		justSlowFrame_ = 0;
	}
}

// ブレイク(スタン)した瞬間: カカロットのように全体を止めて演出を見せる
void GameScene::UpdateBreakFreeze(void)
{
	for (auto& enemy : enemies_)
	{
		if (!enemy || enemy->IsDead())
		{
			continue;
		}

		const bool now = enemy->IsStunned();
		bool& prev = prevStun_[enemy.get()];

		// スタンしていなかった敵が、スタンした瞬間
		if (now && !prev)
		{
			// 全体を止めて、ブレイク演出(自前の時計)を再生する
			breakFreeze_ = PlayerConfig::Break::FREEZE_TIME;

			const VECTOR hitPos = HitEffectPos(*enemy);
			BreakEffect::Play(hitPos, PlayerConfig::Break::FREEZE_TIME);
			mainCamera.StartShake(PlayerConfig::HitShake::BREAK_TIME, PlayerConfig::HitShake::BREAK_POWER);
			EffekseerEffect::GetInstance()->PlayHitEffect(hitPos, 0.0f);
		}

		prev = now;
	}
}

// ヒートアップ発動の演出カメラ(光の柱 -> はじける -> 拳と顔のアップ -> 引き)
void GameScene::UpdateHeatUpCamera(void)
{
	namespace hu = PlayerConfig::HeatUp;

	const bool activating = player_->IsHeatActivating();

	if (!activating)
	{
		if (heatWasActivating_)
		{
			mainCamera.EndCinematic();
		}

		heatWasActivating_ = false;
		return;
	}

	// 発動した瞬間
	if (!heatWasActivating_)
	{
		heatShookBurst_ = false;
		heatShookClose_ = false;

		// 敵の飛び道具・技を消す(止まった弾が残らないように)
		for (auto& enemy : enemies_)
		{
			if (!enemy || enemy->IsDead())
			{
				continue;
			}

			enemy->CancelAttacksForHeatUp();
		}

		using Key = Camera::CineKey;

		// 座標は プレイヤーからの相対: x=右 y=上 z=前(プレイヤーの向き)
		const std::vector<Key> keys =
		{
			// 正面の少し低い位置から、昇っていく光を見上げる
			{ 0.0f,                   { 110.0f,  20.0f, 460.0f }, { 0.0f, 110.0f, 0.0f }, 60.0f, false },
			{ hu::ACTIVATE_FLASH,     { 110.0f, -10.0f, 420.0f }, { 0.0f, 230.0f, 0.0f }, 62.0f, false },
			// 光がはじける: 少し引いて画角を広げる
			{ hu::ACTIVATE_BURST_END, {  90.0f,  40.0f, 470.0f }, { 0.0f, 150.0f, 0.0f }, 72.0f, false },
			// 拳と顔のアップ(カット)。画角を狭め、ゆっくり寄る
			{ hu::ACTIVATE_BURST_END, {  25.0f, 135.0f, 150.0f }, { 0.0f, 120.0f, 0.0f }, 38.0f, true },
			{ hu::ACTIVATE_CLOSE_END, {  25.0f, 135.0f, 115.0f }, { 0.0f, 120.0f, 0.0f }, 34.0f, false },
			// 引いて、金色をまとった全身を見せる(カット -> 少しずつ引く)
			{ hu::ACTIVATE_CLOSE_END, {  40.0f, 110.0f, 180.0f }, { 0.0f, 100.0f, 0.0f }, 55.0f, true },
			{ hu::ACTIVATE_TIME,      {  40.0f, 110.0f, 340.0f }, { 0.0f, 100.0f, 0.0f }, 60.0f, false },
		};

		mainCamera.StartCinematic(keys);
	}

	heatWasActivating_ = true;

	mainCamera.UpdateCinematicPivot(player_->GetTransform().pos, player_->GetForward());

	// 光がはじける瞬間と、アップになる瞬間に画面を揺らす
	const float t = player_->GetHeatActivateElapsed();

	if (!heatShookBurst_ && t >= hu::ACTIVATE_FLASH)
	{
		heatShookBurst_ = true;
		mainCamera.StartShake(0.4f, 7.0f);
	}

	if (!heatShookClose_ && t >= hu::ACTIVATE_BURST_END)
	{
		heatShookClose_ = true;
		mainCamera.StartShake(0.7f, 3.0f);
	}
}
