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

PlayerAttack::Context Player::MakeAttackContext(void) const
{
	PlayerAttack::Context ctx{};
	ctx.playerPos = transform_.pos;
	ctx.targetPos = attackTargetPos_;
	ctx.hasTarget = hasAttackTarget_;
	ctx.deltaTime = scnMng_.GetDeltaTime();
	return ctx;
}

void Player::UpdateAttack(void)
{
	auto& ins = InputManager::GetInstance();
	const bool pushAttack = ins.IsTrgDown(KEY_INPUT_F) || PadInput::IsFightTrg();

	// 追撃できる間に攻撃ボタン(ヒートアップ中は自動)で追撃する
	if (pursuitWindow_ > 0.0f &&
		!pursuitPending_ &&
		!kamehame_.IsActive() &&
		!isCharging_ &&
		!isChargeEnding_ &&
		!formChange_.IsActive())
	{
		const bool autoChase = heatUp_ && pursuitAge_ >= cfg::HeatUp::PURSUIT_AUTO_DELAY;

		if (pushAttack || autoChase)
		{
			pursuitPending_ = true;
			pursuitWindow_ = 0.0f;
			pursuitRecover_ = cfg::Pursuit::HIT_DELAY + cfg::Pursuit::RECOVER_TIME;

			attack_.Reset();
			attack_.EndChase();
			boostChase_.Cancel();
			movePow_ = AsoUtility::VECTOR_ZERO;

			animationController_->Play((int)ANIM_TYPE::ATTACK08, false);
			return;
		}
	}

	// 敵がいれば、まず敵へ飛びこむ
	if (hasAttackTarget_ &&
		!attack_.IsAttack() &&
		!kamehame_.IsActive() &&
		!attack_.IsChasing() &&
		!boostChase_.IsActive() &&
		pushAttack)
	{
		attack_.StartChase();
		canChase_ = false;
		movePow_ = AsoUtility::VECTOR_ZERO;

		animationController_->Play((int)ANIM_TYPE::BOOST_CHASE, true);
		return;
	}

	// 攻撃を始める / 攻撃中なら次の段の先行入力
	if (!kamehame_.IsActive() && !attack_.IsChasing() && !boostChase_.IsActive() && pushAttack)
	{
		if (!attack_.IsAttack())
		{
			StartAttack();
		}
		else
		{
			attack_.RequestNext();
		}
	}

	// 攻撃中
	if (attack_.IsAttack())
	{
		movePow_ = AsoUtility::VECTOR_ZERO;

		const PlayerAttack::ComboResult result = attack_.UpdateCombo(
			MakeAttackContext(),
			animationController_->GetPlayRate(),
			animationController_->IsEnd());

		// 敵の方を向く
		if (VSize(result.faceDir) > Constants::Epsilon)
		{
			FaceHorizontal(result.faceDir);
		}

		if (result.hasFollow)
		{
			// 毎フレーム「残りの距離の rate だけ近づく」動き。
			// fps が変わっても追いつく速さが同じになるよう、近づく割合を直す
			const float fs = FrameScale();
			const float rate = cfg::Attack::FOLLOW_RATE;

			float k = 1.0f;

			if (rate > 0.0001f && fs > 0.0001f)
			{
				k = (1.0f - powf(1.0f - rate, fs)) / rate / fs;
			}

			movePow_ = VScale(result.follow, k);
		}

		if (result.advance)
		{
			AdvanceCombo();
		}
		else if (result.finish)
		{
			FinishAttack();
		}
	}

	if (attackEndTimer_ > 0.0f)
	{
		attackEndTimer_ -= scnMng_.GetDeltaTime();

		if (attackEndTimer_ < 0.0f)
		{
			attackEndTimer_ = 0.0f;
		}
	}
}

// 1段目の攻撃を始める
void Player::StartAttack(void)
{
	attack_.StartAttack();

	animationController_->Play((int)ANIM_TYPE::ATTACK01, false);
}

// 攻撃を終える
void Player::FinishAttack(void)
{
	attack_.Reset();

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

// 次の段の攻撃へ(瞬間移動とアニメ)
void Player::AdvanceCombo(void)
{
	const PlayerAttack::ComboStep step = attack_.AdvanceCombo(MakeAttackContext());

	// 瞬間移動する段
	if (step.warp)
	{
		LeaveAfterImage();
		transform_.pos = step.warpPos;

		const VECTOR lookDir = VSub(attackTargetPos_, transform_.pos);

		if (step.lookPitch)
		{
			SetLockOnPitch(lookDir);	// 8段目は上下にも傾ける
		}

		FaceHorizontal(lookDir);
	}

	// 瞬間移動する段は、そこで止まる
	if (step.combo >= 6 || step.combo == 4)
	{
		movePow_ = AsoUtility::VECTOR_ZERO;
	}

	switch (step.combo)
	{
	case 2:
		animationController_->Play((int)ANIM_TYPE::ATTACK02, false, 0.0f, -1.0f, false, true);
		break;

	case 3:
		animationController_->Play((int)ANIM_TYPE::ATTACK03, false);
		break;

	case 4:
		animationController_->Play((int)ANIM_TYPE::ATTACK04, false);
		break;

	case 5:
		animationController_->Play((int)ANIM_TYPE::ATTACK05, false);
		break;

	case 6:
		animationController_->Play((int)ANIM_TYPE::ATTACK06, false);
		break;

	case 7:
		animationController_->Play((int)ANIM_TYPE::ATTACK07, false);
		break;

	case 8:
		animationController_->Play((int)ANIM_TYPE::ATTACK08, false);
		break;

	default:
		break;
	}
}

PlayerKamehameha::Context Player::MakeKamehameContext(void) const
{
	PlayerKamehameha::Context ctx{};
	ctx.playerPos = transform_.pos;
	ctx.forward = GetForward();
	ctx.hasTarget = isLockOn_ && hasAttackTarget_;
	ctx.targetPos = attackTargetPos_;
	ctx.deltaTime = scnMng_.GetDeltaTime();
	return ctx;
}

void Player::UpdateKamehame(void)
{
	auto& ins = InputManager::GetInstance();

	// かめはめ波を撃ち始める
	if (!attack_.IsAttack() &&
		!kamehame_.IsActive() &&
		!isJump_ &&
		!attack_.IsChasing() &&
		!boostChase_.IsActive() &&
		!isDodge_ &&
		((ins.IsNew(KEY_INPUT_TAB) &&	// TAB を押しながら R
			ins.IsTrgDown(KEY_INPUT_R)) ||
			PadInput::IsKamehameTrg()))	// パッド
	{
		if (UseKi(KAMEHAME_KI_COST))
		{
			movePow_ = AsoUtility::VECTOR_ZERO;

			animationController_->Play((int)ANIM_TYPE::KAMEHAME, true,
				0.0f, cfg::Kamehame::ANIM_LOOP_END);
			animationController_->SetEndLoop(
				cfg::Kamehame::ANIM_LOOP_START,
				cfg::Kamehame::ANIM_LOOP_END,
				cfg::Kamehame::ANIM_LOOP_SPEED);

			kamehame_.Start(MakeKamehameContext());
		}
	}

	if (!kamehame_.IsActive())
	{
		return;
	}

	// 撃っている間は、狙う向きへ体を向ける
	VECTOR aim = AsoUtility::VECTOR_ZERO;
	const bool finished = kamehame_.Update(MakeKamehameContext(), aim);

	if (VSize(aim) > Constants::Epsilon)
	{
		FaceHorizontal(aim);
	}

	if (finished)
	{
		animationController_->Play((int)ANIM_TYPE::IDLE);
	}
}

bool Player::IsKamehameBeam(void) const
{
	return kamehame_.IsBeam();
}

float Player::GetKamehameRadius(void) const
{
	return kamehame_.GetRadius();
}

VECTOR Player::GetKamehameStartPos(void) const
{
	return kamehame_.GetStartPos();
}

VECTOR Player::GetKamehameEndPos(void) const
{
	return kamehame_.GetEndPos(GetForward());
}

bool Player::IsKamehame(void) const
{
	return kamehame_.IsActive();
}

// 気溜め(T 長押し / パッドの△長押し)
void Player::UpdateKiCharge(void)
{
	auto& ins = InputManager::GetInstance();

	// 気溜めを始める
	if (!isCharging_ &&
		!isChargeEnding_ &&
		!attack_.IsAttack() &&
		!kamehame_.IsActive() &&
		!boostChase_.IsActive() &&
		(ins.IsTrgDown(KEY_INPUT_T) || PadInput::IsChargeTrg()))
	{
		isCharging_ = true;
		movePow_ = AsoUtility::VECTOR_ZERO;

		// 溜めモーションをループさせる
		PlayChargeLoop();

		// 気溜めのエフェクト
		EffekseerEffect::GetInstance()->PlayChargeEffect(
			transform_.pos, form_ == FORM::SUPER);
	}

	// 気溜め中
	if (isCharging_)
	{
		movePow_ = AsoUtility::VECTOR_ZERO;

		EffekseerEffect::GetInstance()->UpdateChargeEffect(transform_.pos);

		AddKi(KI_CHARGE_SPEED * scnMng_.GetDeltaTime());

		// 満タンになったら、ヒートアップできないときは終わる
		if (ki_ >= MAX_KI)
		{
			if (!CanHeatUp())
			{
				BeginChargeEnding();
				return;
			}
		}

		// 移動や攻撃の入力があったら、すぐにやめる
		const bool cancelCharge =
			IsMoveKeyDown(ins) ||
			ins.IsNew(KEY_INPUT_RSHIFT) ||
			ins.IsTrgDown(KEY_INPUT_F) ||
			ins.IsTrgDown(KEY_INPUT_R) ||
			ins.IsNew(KEY_INPUT_BACKSLASH) ||
			PadInput::IsFightTrg() ||
			PadInput::IsKamehameTrg();

		if (cancelCharge)
		{
			StopCharge();
			animationController_->Play((int)ANIM_TYPE::IDLE);
		}
		else if (!ins.IsNew(KEY_INPUT_T) && !PadInput::IsChargeHold())
		{
			// ボタンを離したら、終了モーションへ
			BeginChargeEnding();
			return;
		}
		else
		{
			return;
		}
	}

	// 気溜めの終了モーション
	if (isChargeEnding_)
	{
		movePow_ = AsoUtility::VECTOR_ZERO;

		chargeEndTimer_ += scnMng_.GetDeltaTime();

		// モーションが終わったら(念のため時間でも)待機へ
		if (animationController_->IsEnd() || chargeEndTimer_ >= cfg::Charge::END_MAX_TIME)
		{
			isChargeEnding_ = false;
			chargeEndTimer_ = 0.0f;
			animationController_->Play((int)ANIM_TYPE::IDLE);
		}
	}
}

// 気溜めをすぐにやめる
void Player::StopCharge(void)
{
	isCharging_ = false;
	isChargeEnding_ = false;

	animationController_->ClearEndLoop();

	EffekseerEffect::GetInstance()->StopChargeEffect();
}

// 気溜めの終了モーションを始める
void Player::BeginChargeEnding(void)
{
	isCharging_ = false;
	isChargeEnding_ = true;
	chargeEndTimer_ = 0.0f;

	EffekseerEffect::GetInstance()->StopChargeEffect();

	animationController_->ClearEndLoop();

	// 溜めモーションの続き(ループの後ろ)を最後まで流す
	animationController_->Play((int)ANIM_TYPE::CHARGE, false,
		cfg::Charge::ANIM_LOOP_END, cfg::Charge::ANIM_END_STEP, false, true);
}

// 気溜めモーションを、ループ付きで最初から流す
void Player::PlayChargeLoop(void)
{
	animationController_->Play((int)ANIM_TYPE::CHARGE, true, 0.0f, cfg::Charge::ANIM_LOOP_END);
	animationController_->SetEndLoop(
		cfg::Charge::ANIM_LOOP_START, cfg::Charge::ANIM_LOOP_END, cfg::Charge::ANIM_LOOP_SPEED);
}

// 気を増やす(最大で止める)
void Player::AddKi(float amount)
{
	ki_ += amount;

	if (ki_ > MAX_KI)
	{
		ki_ = MAX_KI;
	}
}

// 気弾(U / パッドの○)
void Player::UpdateKiBlast(void)
{
	auto& ins = InputManager::GetInstance();

	// 気弾を撃ち始める
	if (!isKiBlast_ &&
		!attack_.IsAttack() &&
		!kamehame_.IsActive() &&
		!isCharging_ &&
		!isChargeEnding_ &&
		!boostChase_.IsActive() &&
		(ins.IsTrgDown(KEY_INPUT_U) || PadInput::IsKiBlastTrg()))
	{
		isKiBlast_ = true;
		isKiBlastShot_ = false;
		kiBlastTimer_ = 0.0f;

		animationController_->Play((int)ANIM_TYPE::KI_BLAST, false);
	}

	// 気弾のモーション中
	if (isKiBlast_ && !barrage_.IsActive())
	{
		kiBlastTimer_ += scnMng_.GetDeltaTime();

		// 撃つタイミング
		if (!isKiBlastShot_ && kiBlastTimer_ >= Constants::KiBlastShotTime)
		{
			ShotKiBlast();
		}

		// モーションの終わり
		if (kiBlastTimer_ >= Constants::KiBlastEndTime)
		{
			isKiBlast_ = false;
			isKiBlastShot_ = false;
			kiBlastTimer_ = 0.0f;

			// 移動していなければ待機のアニメへ
			if (!IsMoveKeyDown(ins))
			{
				animationController_->Play((int)ANIM_TYPE::IDLE);
			}
		}
	}

	// 撃った気弾を動かす
	for (auto& blast : kiBlasts_)
	{
		blast->Update();
	}

	// 消えた気弾を片付ける
	kiBlasts_.erase(
		std::remove_if(kiBlasts_.begin(), kiBlasts_.end(),
			[](const std::unique_ptr<KiBlast>& blast) { return blast->IsDead(); }),
		kiBlasts_.end());
}

// 連続エネルギー弾(構え・連射・弾の管理は PlayerBarrage が行う)
void Player::UpdateBarrage(void)
{
	auto& ins = InputManager::GetInstance();

	// 構えを始める
	if (!barrage_.IsActive() &&
		!isKiBlast_ &&
		!attack_.IsAttack() &&
		!kamehame_.IsActive() &&
		!isCharging_ &&
		!isChargeEnding_ &&
		!isJump_ &&
		!attack_.IsChasing() &&
		!boostChase_.IsActive() &&
		!isDodge_ &&
		((ins.IsNew(KEY_INPUT_TAB) && ins.IsTrgDown(KEY_INPUT_U)) ||
			PadInput::IsBarrageTrg()))
	{
		if (UseKi(cfg::Barrage::KI_COST))
		{
			barrage_.Start();
			isKiBlast_ = true;
			movePow_ = AsoUtility::VECTOR_ZERO;

			animationController_->Play((int)ANIM_TYPE::BARRAGE, false);
		}
	}

	// 撃っていなくても、飛んでいる弾は動かし続ける
	PlayerBarrage::Context ctx{};
	ctx.deltaTime = scnMng_.GetDeltaTime();
	ctx.playerPos = transform_.pos;
	ctx.forward = GetForward();
	ctx.hasTarget = isLockOn_ && hasAttackTarget_;
	ctx.targetPos = attackTargetPos_;

	// 枠を動かす: 右スティック、またはキーボードの矢印キー
	float x = PadInput::RStickX();
	float y = PadInput::RStickY();	// 上に倒して枠が下がるなら、ここを -PadInput::RStickY() にする

	if (ins.IsNew(KEY_INPUT_RIGHT)) { x += 1.0f; }
	if (ins.IsNew(KEY_INPUT_LEFT)) { x -= 1.0f; }
	if (ins.IsNew(KEY_INPUT_UP)) { y += 1.0f; }
	if (ins.IsNew(KEY_INPUT_DOWN)) { y -= 1.0f; }

	if (x > 1.0f) { x = 1.0f; }
	if (x < -1.0f) { x = -1.0f; }
	if (y > 1.0f) { y = 1.0f; }
	if (y < -1.0f) { y = -1.0f; }

	ctx.stickX = x;
	ctx.stickY = y;

	// 撃っている間は、狙う方向へ体を向ける
	if (barrage_.IsActive() && ctx.hasTarget)
	{
		FaceHorizontal(VSub(attackTargetPos_, transform_.pos));
	}

	const bool finished = barrage_.Update(ctx);

	// 1発撃つたびに、腕を突き出し直す
	barrage_.ConsumeFired();

	// 撃ち終わった
	if (finished)
	{
		isKiBlast_ = false;

		if (!IsMoveKeyDown(ins))
		{
			animationController_->Play((int)ANIM_TYPE::IDLE);
		}
	}
}

// 気弾を撃つ
void Player::ShotKiBlast(void)
{
	VECTOR shotPos = transform_.pos;

	if (rightHandFrame_ != -1)
	{
		shotPos = MV1GetFramePosition(transform_.modelId, rightHandFrame_);
	}

	VECTOR shotDir = GetForward();

	// ロックオン中は敵へ向けて撃つ
	if (isLockOn_ && hasAttackTarget_)
	{
		shotDir = NormalizeSafe(VSub(attackTargetPos_, shotPos));
	}

	isKiBlastShot_ = true;

	// 続けて撃った回数を数える(間が空くと戻る)
	kiBlastChainTimer_ = cfg::Finish::RESET_TIME;
	kiBlastCount_++;

	if (kiBlastCount_ >= (heatUp_ ? cfg::Finish::COUNT_HEAT : cfg::Finish::COUNT))
	{
		kiBlastCount_ = 0;

		auto finish = std::make_unique<PlayerFinishBlast>();
		finish->Init(shotPos, shotDir);
		finishBlasts_.push_back(std::move(finish));
		return;
	}

	kiBlasts_.push_back(std::make_unique<KiBlast>(shotPos, shotDir));
}

const std::vector<std::unique_ptr<KiBlast>>& Player::GetKiBlasts(void) const
{
	return kiBlasts_;
}
