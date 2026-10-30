#include <string>
#include <cmath>
#include <cfloat>
#include <algorithm>
#include <filesystem>
#include "../../Application.h"
#include "../../Utility/AsoUtility.h"
#include "../../Utility/DamageCalc.h"
#include "../../Manager/InputManager.h"
#include "../../Manager/PadInput.h"
#include "../../Manager/SceneManager.h"
#include "../../Manager/ResourceManager.h"
#include "../../Manager/Camera/Camera.h"
#include "../Common/AnimationController.h"
#include "../Common/Capsule.h"
#include "../Common/Collider.h"
#include "../Planet.h"
#include "Player.h"
#include "definition/PlayerConfig.h"
#include "../../Manager/EffekseerEffect.h"
#include "Player_Internal.h"

namespace cfg = PlayerConfig;
using namespace PlayerInternal;

void Player::AddTension(float amount)
{
	// ヒートアップ中は増えない
	if (heatUp_)
	{
		return;
	}

	tension_ += amount;

	if (tension_ > MAX_TENSION) { tension_ = MAX_TENSION; }
	if (tension_ < 0.0f) { tension_ = 0.0f; }
}

bool Player::CanHeatUp(void) const
{
	return !heatUp_ && tension_ >= MAX_TENSION && ki_ >= MAX_KI - 0.01f;
}

float Player::GetHeatHoldRate(void) const
{
	const float rate = heatHold_ / cfg::HeatUp::HOLD_TIME;
	return rate > 1.0f ? 1.0f : rate;
}

// ヒートアップ(テンション満タン+気力MAXで、T / △長押し)
void Player::UpdateHeatUp(void)
{
	auto& ins = InputManager::GetInstance();

	const bool hold = ins.IsNew(KEY_INPUT_T) || PadInput::IsChargeHold();

	if (!CanHeatUp() ||
		!hold ||
		attack_.IsAttack() ||
		attack_.IsChasing() ||
		kamehame_.IsActive() ||
		boostChase_.IsActive() ||
		isKiBlast_ ||
		isDodge_)
	{
		heatHold_ = 0.0f;
		return;
	}

	heatHold_ += scnMng_.GetDeltaTime();

	if (heatHold_ >= cfg::HeatUp::HOLD_TIME)
	{
		heatHold_ = 0.0f;
		heatUp_ = true;
		heatUpTimer_ = cfg::HeatUp::DURATION;
		tension_ = MAX_TENSION;
		ki_ = MAX_KI;

		// 気溜め中なら、そのモーションのまま演出に入る
		//   そうでなければ溜めモーションを流す
		const bool wasCharging = isCharging_;

		heatActivate_ = cfg::HeatUp::ACTIVATE_TIME;
		movePow_ = AsoUtility::VECTOR_ZERO;

		if (wasCharging)
		{
			isCharging_ = false;
			isChargeEnding_ = false;
			EffekseerEffect::GetInstance()->StopChargeEffect();
		}
		else
		{
			StopCharge();
			PlayChargeLoop();
		}
	}
}

// テンション・ヒートアップ・追撃・フィニッシュ気弾のタイマー
void Player::UpdateTensionTimers(float deltaTime)
{
	// バニッシュアタックの後、カメラをゆっくり追従にする残り時間
	if (vanishCalmTimer_ > 0.0f)
	{
		vanishCalmTimer_ -= deltaTime;
	}

	// ヒートアップ発動の演出
	if (heatActivate_ > 0.0f)
	{
		heatActivate_ -= deltaTime;

		if (heatActivate_ <= 0.0f)
		{
			heatActivate_ = 0.0f;
			animationController_->ClearEndLoop();
			animationController_->Play((int)ANIM_TYPE::IDLE);
		}
	}

	// ヒートアップ中: 気力は減らず、テンションが時間で減る
	if (heatUp_ && heatActivate_ <= 0.0f)
	{
		heatUpTimer_ -= deltaTime;
		ki_ = MAX_KI;

		const float rate = heatUpTimer_ / cfg::HeatUp::DURATION;
		tension_ = MAX_TENSION * (rate > 0.0f ? rate : 0.0f);

		if (heatUpTimer_ <= 0.0f)
		{
			heatUp_ = false;
			heatUpTimer_ = 0.0f;
			tension_ = 0.0f;
		}
	}

	// 追撃できる残り時間
	if (pursuitWindow_ > 0.0f)
	{
		pursuitWindow_ -= deltaTime;
		pursuitAge_ += deltaTime;

		if (pursuitWindow_ < 0.0f) { pursuitWindow_ = 0.0f; }
	}

	// 追撃のあと、待機に戻す
	if (pursuitRecover_ > 0.0f)
	{
		pursuitRecover_ -= deltaTime;

		if (pursuitRecover_ <= 0.0f)
		{
			pursuitRecover_ = 0.0f;

			if (!attack_.IsAttack() && !kamehame_.IsActive() && !IsMoveKeyDown(InputManager::GetInstance()))
			{
				animationController_->Play((int)ANIM_TYPE::IDLE);
			}
		}
	}

	// 気弾を続けて撃った回数は、間が空くと戻る
	if (kiBlastChainTimer_ > 0.0f)
	{
		kiBlastChainTimer_ -= deltaTime;

		if (kiBlastChainTimer_ <= 0.0f)
		{
			kiBlastChainTimer_ = 0.0f;
			kiBlastCount_ = 0;
		}
	}

	// フィニッシュ気弾(ロックオン中は敵の胸を狙う)
	{
		VECTOR aim = VAdd(attackTargetPos_, VGet(0.0f, Constants::EnemyCenterHeight, 0.0f));
		const bool hasAim = isLockOn_ && hasAttackTarget_;

		for (auto& blast : finishBlasts_)
		{
			blast->Update(deltaTime, hasAim ? &aim : nullptr);
		}

		finishBlasts_.erase(
			std::remove_if(finishBlasts_.begin(), finishBlasts_.end(),
				[](const std::unique_ptr<PlayerFinishBlast>& b) { return b->IsDead(); }),
			finishBlasts_.end());
	}
}

// 追撃できる時間を始める
void Player::OpenPursuit(bool fromPursuit)
{
	if (fromPursuit)
	{
		pursuitChain_++;

		const int limit = heatUp_ ? cfg::HeatUp::CHAIN_MAX : cfg::Pursuit::CHAIN_MAX;

		if (pursuitChain_ >= limit)
		{
			pursuitWindow_ = 0.0f;
			return;
		}
	}
	else
	{
		pursuitChain_ = 0;
	}

	pursuitWindow_ = cfg::Pursuit::WINDOW;
	pursuitAge_ = 0.0f;
}

bool Player::ConsumePursuit(void)
{
	const bool result = pursuitPending_;
	pursuitPending_ = false;
	return result;
}

float Player::GetPursuitRate(void) const
{
	return pursuitWindow_ > 0.0f ? pursuitWindow_ / cfg::Pursuit::WINDOW : 0.0f;
}

void Player::SetClash(bool active, float rate)
{
	clashActive_ = active;
	clashRate_ = rate;
	kamehame_.SetHold(active);
}

bool Player::ConsumeVanish(void)
{
	const bool result = vanishPending_;
	vanishPending_ = false;
	return result;
}

bool Player::ConsumeJustDodge(void)
{
	const bool result = justDodged_;
	justDodged_ = false;
	return result;
}

// ガード(L / パッドの L2)
//   ガード中にバースト(B)・バニッシュアタック(V)
void Player::UpdateGuard(void)
{
	InputManager& ins = InputManager::GetInstance();

	// ガードの状態は PlayerGuard に任せ、
	//   返ってきた出来事に合わせてアニメを再生する
	PlayerGuard::Context ctx{};
	ctx.deltaTime = SceneManager::GetInstance().GetDeltaTime();
	ctx.canStart =
		!attack_.IsAttack() &&
		!attack_.IsChasing() &&
		!kamehame_.IsActive() &&
		!isKiBlast_ &&
		!boostChase_.IsActive() &&
		!isDodge_ &&
		!formChange_.IsActive();
	ctx.hold = ins.IsNew(KEY_INPUT_L) || PadInput::IsGuard();
	ctx.burstTrg = ins.IsTrgDown(KEY_INPUT_B) || PadInput::IsBurstTrg();
	ctx.vanishTrg = ins.IsTrgDown(KEY_INPUT_V) || PadInput::IsVanishTrg();

	if (guardHitTimer_ > 0.0f)
	{
		guardHitTimer_ -= ctx.deltaTime;
	}

	const PlayerGuard::Result result = guard_.Update(ctx);

	// ガードブレイク・バースト中は動けない
	if (guard_.IsBreak() || guard_.IsBurst())
	{
		movePow_ = AsoUtility::VECTOR_ZERO;
	}

	if (result.breakEnded || result.burstEnded)
	{
		animationController_->Play((int)ANIM_TYPE::IDLE, true, 0.0f, -1.0f, false, true);
		return;
	}

	if (result.started)
	{
		// 気溜め中なら止める
		StopCharge();

		movePow_ = AsoUtility::VECTOR_ZERO;

		animationController_->Play((int)ANIM_TYPE::GUARD, false,
			0.0f, cfg::Guard::ANIM_HOLD_STEP, false);
	}
	else if (result.released)
	{
		animationController_->Play((int)ANIM_TYPE::IDLE);
	}

	// バニッシュアタック(ガードで攻撃を受けた直後だけ出せる)
	if (result.vanishRequested && hasAttackTarget_ && guardHitTimer_ > 0.0f && UseKi(cfg::Guard::VANISH_KI_COST))
	{
		guard_.StartVanish();
		vanishPending_ = true;

		movePow_ = AsoUtility::VECTOR_ZERO;

		animationController_->ClearEndLoop();
		animationController_->Play((int)ANIM_TYPE::ATTACK08, false, 0.0f, -1.0f, false, true);
		return;
	}

	// ガードバースト
	if (result.burstRequested && UseKi(cfg::Guard::BURST_KI_COST))
	{
		guard_.StartBurst();

		movePow_ = AsoUtility::VECTOR_ZERO;

		animationController_->ClearEndLoop();
		animationController_->Play((int)ANIM_TYPE::GUARD_BURST, false,
			0.0f, cfg::Guard::BURST_ANIM_END_STEP, false, true);
	}
}

void Player::GuardDamage(float damage)
{
	mainCamera.StartShake(cfg::HitShake::GUARD_TIME, cfg::HitShake::GUARD_POWER);

	AddTension(cfg::Tension::GAIN_GUARD);

	// ガードで受けた直後は、バニッシュアタックを出せる
	guardHitTimer_ = cfg::Guard::VANISH_WINDOW;

	// 耐久値が無くなったらガードブレイク
	if (guard_.Damage(damage))
	{
		movePow_ = AsoUtility::VECTOR_ZERO;

		animationController_->Play((int)ANIM_TYPE::GUARD_BREAK, true, 0.0f, -1.0f, false, true);
	}
}

bool Player::IsGuard(void) const { return guard_.IsGuard(); }

bool Player::IsGuardBreak(void) const { return guard_.IsBreak(); }

bool Player::IsGuardBurst(void) const { return guard_.IsBurst(); }

bool Player::IsGuardBurstTrigger(void) const { return guard_.IsBurstTrigger(); }

// ダメージ
void Player::Damage(int damage)
{
	// 死亡・変身中・ヒートアップ発動の演出中は受けない
	if (isDead_ || formChange_.IsActive() || heatActivate_ > 0.0f)
	{
		return;
	}

	hp_ -= damage * DamageCalc::TAKEN_SCALE;

	// 受けたダメージの大きさで、揺れの強さと長さを変える
	{
		float r = static_cast<float>(damage) * DamageCalc::TAKEN_SCALE /
			(static_cast<float>(MAX_HP) * cfg::HitShake::TAKEN_FULL_RATE);
		if (r > 1.0f) { r = 1.0f; }
		if (r < 0.0f) { r = 0.0f; }

		float power = cfg::HitShake::TAKEN_POWER_MIN +
			(cfg::HitShake::TAKEN_POWER_MAX - cfg::HitShake::TAKEN_POWER_MIN) * r;
		const float time = cfg::HitShake::TAKEN_TIME_MIN +
			(cfg::HitShake::TAKEN_TIME_MAX - cfg::HitShake::TAKEN_TIME_MIN) * r;

		if (barrage_.IsActive())
		{
			power *= cfg::HitShake::BARRAGE_TAKEN_RATE;
		}

		mainCamera.StartShake(time, power);
	}

	// 受けたダメージでテンションが増える
	{
		float gain = damage * cfg::Tension::GAIN_TAKEN_RATE;
		if (gain > cfg::Tension::GAIN_TAKEN_MAX) { gain = cfg::Tension::GAIN_TAKEN_MAX; }
		AddTension(gain);
	}

	if (hp_ <= 0)
	{
		hp_ = 0;
		isDead_ = true;
	}

	// 連続エネルギー弾の最中は、ダメージは受けるが怯まず撃ち続ける
	if (barrage_.IsActive() && !isDead_)
	{
		return;
	}

	// 攻撃をやめる
	attack_.Reset();

	// 追撃もやめる
	pursuitWindow_ = 0.0f;
	pursuitPending_ = false;

	// 気溜めをやめる
	if (isCharging_ || isChargeEnding_)
	{
		StopCharge();
	}

	// かめはめ波をやめる
	if (kamehame_.IsActive())
	{
		kamehame_.Cancel();
	}

	// 高速接近・追撃・回避・気弾をやめる
	boostChase_.Cancel();
	attack_.EndChase();
	isDodge_ = false;
	dodgeTimer_ = 0.0f;
	isKiBlast_ = false;
	isKiBlastShot_ = false;
	kiBlastTimer_ = 0.0f;
	barrage_.Cancel();

	movePow_ = AsoUtility::VECTOR_ZERO;

	// 少しの間動けない
	isDamage_ = true;
	damageTimer_ = Constants::DamageStateTime;

	animationController_->Play((int)ANIM_TYPE::DAMAGE, false);
}

void Player::ExtendDamageStun(float time)
{
	if (isDamage_ && damageTimer_ < time)
	{
		damageTimer_ = time;
	}
}
