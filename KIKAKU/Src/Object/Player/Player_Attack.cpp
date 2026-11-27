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

	if (ins.IsTrgDown(KEY_INPUT_F) || PadInput::IsFightTrg())
	{
		attackBuffer_ = ATTACK_BUFFER_TIME;
		dbgPress_++;
	}
	else if (attackBuffer_ > 0.0f)
	{
		attackBuffer_ -= scnMng_.GetDeltaTime();
		if (attackBuffer_ <= 0.0f)
		{
			dbgExpired_++;
		}
	}

	const bool pushAttack = attackBuffer_ > 0.0f;

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
			attackBuffer_ = 0.0f;
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
		attackBuffer_ = 0.0f;
		attack_.StartChase();
		canChase_ = false;
		movePow_ = AsoUtility::VECTOR_ZERO;

		animationController_->Play((int)ANIM_TYPE::BOOST_CHASE, true);
		return;
	}

	// 攻撃を始める / 攻撃中なら次の段の先行入力
	if (!kamehame_.IsActive() && !attack_.IsChasing() && !boostChase_.IsActive() && pushAttack)
	{
		attackBuffer_ = 0.0f;

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

void Player::DrawInputDebug(void) const
{
	// false にすると、表示しない
	constexpr bool ENABLE = true;
	if (!ENABLE)
	{
		return;
	}

	const int x = 20;
	const int y = 60;
	const int barW = 200;
	const int barH = 14;

	// 予約の残り(満タンから減っていく)
	float rate = attackBuffer_ / ATTACK_BUFFER_TIME;
	if (rate < 0.0f) { rate = 0.0f; }
	if (rate > 1.0f) { rate = 1.0f; }

	DrawBox(x, y, x + barW, y + barH, 0x333333, true);
	DrawBox(x, y, x + (int)(barW * rate), y + barH, 0x00ff66, true);
	DrawBox(x, y, x + barW, y + barH, 0xffffff, false);

	DrawFormatString(x, y + barH + 4, 0xffffff,
		"先行入力  押した:%d  期限切れ:%d", dbgPress_, dbgExpired_);
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
			BeginChargeEnding();
			return;
		}
		else
		{
			return;
		}
	}

	if (isChargeEnding_)
	{
		movePow_ = AsoUtility::VECTOR_ZERO;

		chargeEndTimer_ += scnMng_.GetDeltaTime();

		if (animationController_->IsEnd() || chargeEndTimer_ >= cfg::Charge::END_MAX_TIME)
		{
			isChargeEnding_ = false;
			chargeEndTimer_ = 0.0f;
			animationController_->Play((int)ANIM_TYPE::IDLE);
		}
	}
}

void Player::StopCharge(void)
{
	isCharging_ = false;
	isChargeEnding_ = false;

	animationController_->ClearEndLoop();

	EffekseerEffect::GetInstance()->StopChargeEffect();
}

void Player::BeginChargeEnding(void)
{
	isCharging_ = false;
	isChargeEnding_ = true;
	chargeEndTimer_ = 0.0f;

	EffekseerEffect::GetInstance()->StopChargeEffect();

	animationController_->ClearEndLoop();

	animationController_->Play((int)ANIM_TYPE::CHARGE, false,
		cfg::Charge::ANIM_LOOP_END, cfg::Charge::ANIM_END_STEP, false, true);
}

void Player::PlayChargeLoop(void)
{
	animationController_->Play((int)ANIM_TYPE::CHARGE, true, 0.0f, cfg::Charge::ANIM_LOOP_END);
	animationController_->SetEndLoop(
		cfg::Charge::ANIM_LOOP_START, cfg::Charge::ANIM_LOOP_END, cfg::Charge::ANIM_LOOP_SPEED);
}

void Player::AddKi(float amount)
{
	ki_ += amount;

	if (ki_ > MAX_KI)
	{
		ki_ = MAX_KI;
	}
}

void Player::DrawKiHoldGauge(void) const
{
	if (!isKiHold_)
	{
		return;
	}

	if (KiHoldTime_ < cfg::KiShot::MIN_CHARGE_TIME)
	{
		return;
	}

	float rate = KiHoldTime_ / cfg::KiShot::CHARGE_TIME;
	if (rate > 1.0f) { rate = 1.0f; }

	const VECTOR sp = ConvWorldPosToScreenPos(VAdd(transform_.pos, VGet(0.0f, 250.0f, 0.0f)));

	// 画面の外(後ろ)なら出さない
	if (sp.z <= 0.0f || sp.z >= 1.0f)
	{
		return;
	}

	const int w = 120;
	const int h = 10;
	const int x = (int)sp.x - w / 2;
	const int y = (int)sp.y;

	const bool full = rate >= 1.0f;

	unsigned int color = GetColor(255, 210, 60);
	if (full)
	{
		color = ((GetNowCount() / 80) % 2 == 0) ? GetColor(255, 255, 200) : GetColor(255, 170, 30);
	}

	DrawBox(x - 2, y - 2, x + w + 2, y + h + 2, GetColor(0, 0, 0), TRUE);
	DrawBox(x, y, x + (int)(w * rate), y + h, color, TRUE);
	DrawBox(x, y, x + w, y + h, GetColor(255, 255, 255), FALSE);

	if (full)
	{
		DrawString(x + w / 2 - 12, y - 18, "MAX", GetColor(255, 240, 120));
	}
}

void Player::UpdateKiBlast(void)
{
	auto& ins = InputManager::GetInstance();

	const bool held =
		(ins.IsNew(KEY_INPUT_U) && !ins.IsNew(KEY_INPUT_TAB)) ||
		PadInput::IsKiBlastHold();

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
		kiAnimStarted_ = false;
		kiPendingRate_ = -1.0f;

		animationController_->Play((int)ANIM_TYPE::KI_BLAST, false);
	}

	// 気弾のモーション中
	if (isKiBlast_ && !barrage_.IsActive())
	{
		if (isKiHold_)
		{
			UpdateKiHold(held);
		}
		else
		{
			kiBlastTimer_ += scnMng_.GetDeltaTime();

			// 押した直後: 押し続けていれば溜め、離していれば投げるアニメを始める
			if (!kiAnimStarted_ && kiBlastTimer_ >= cfg::KiShot::HOLD_DECIDE_TIME)
			{
				if (held)
				{
					BeginKiHold();
				}
				else
				{
					kiAnimStarted_ = true;
					kiBlastTimer_ = 0.0f;
					animationController_->Play((int)ANIM_TYPE::KI_BLAST, false);
				}
			}

			// 撃つタイミング
			if (kiAnimStarted_ && !isKiBlastShot_ && kiBlastTimer_ >= Constants::KiBlastShotTime)
			{
				ShotKiBlast(kiPendingRate_);
				kiPendingRate_ = -1.0f;
			}

			// モーションの終わり
			if (kiAnimStarted_ &&
				!isKiHold_ &&
				kiBlastTimer_ >= Constants::KiBlastShotTime &&
				(animationController_->IsEnd() || kiBlastTimer_ >= cfg::KiShot::THROW_MAX_TIME))
			{
				isKiBlast_ = false;
				isKiBlastShot_ = false;
				kiAnimStarted_ = false;
				kiBlastTimer_ = 0.0f;

				if (!IsMoveKeyDown(ins))
				{
					animationController_->Play((int)ANIM_TYPE::IDLE);
				}
			}
		}
	}

	// 撃った気弾を動かす
	for (auto& blast : kiBlasts_)
	{
		blast->SetTarget(
			VAdd(attackTargetPos_, VGet(0.0f, cfg::KiShot::AIM_HEIGHT, 0.0f)),
			isLockOn_ && hasAttackTarget_);
		blast->Update();
	}

	// 消えた気弾を片付ける
	kiBlasts_.erase(
		std::remove_if(kiBlasts_.begin(), kiBlasts_.end(),
			[](const std::unique_ptr<KiBlast>& blast) { return blast->IsDead(); }),
		kiBlasts_.end());
}

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

	float x = PadInput::RStickX();
	float y = PadInput::RStickY();	

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
void Player::ShotKiBlast(float chargeRate)
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
		shotDir = NormalizeSafe(
			VSub(VAdd(attackTargetPos_, VGet(0.0f, cfg::KiShot::AIM_HEIGHT, 0.0f)), shotPos));
	}

	isKiBlastShot_ = true;

	// 溜めて離したときは、フィニッシュ気弾
	if (chargeRate >= 0.0f)
	{
		auto finish = std::make_unique<PlayerFinishBlast>();
		finish->Init(shotPos, shotDir, chargeRate);
		finishBlasts_.push_back(std::move(finish));
		return;
	}

	kiBlasts_.push_back(std::make_unique<KiBlast>(
		shotPos, shotDir, isLockOn_ && hasAttackTarget_));
}

void Player::BeginKiHold(void)
{
	isKiHold_ = true;
	KiHoldTime_ = 0.0f;
	movePow_ = AsoUtility::VECTOR_ZERO;

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

void Player::UpdateKiHold(bool held)
{
	movePow_ = AsoUtility::VECTOR_ZERO;

	// 被弾したら、溜めをやめる
	if (isDamage_)
	{
		EndKiHold();
		isKiBlast_ = false;
		isKiBlastShot_ = false;
		kiBlastTimer_ = 0.0f;
		kiAnimStarted_ = false;
		return;
	}

	const float prev = KiHoldTime_;
	KiHoldTime_ += scnMng_.GetDeltaTime();

	// 本当に溜め始めたところで、エフェクトを出す
	if (prev < cfg::KiShot::MIN_CHARGE_TIME && KiHoldTime_ >= cfg::KiShot::MIN_CHARGE_TIME)
	{
		EffekseerEffect::GetInstance()->PlayChargeEffect(transform_.pos, form_ == FORM::SUPER);
	}

	if (KiHoldTime_ >= cfg::KiShot::MIN_CHARGE_TIME)
	{
		EffekseerEffect::GetInstance()->UpdateChargeEffect(transform_.pos);
	}

	if (isLockOn_ && hasAttackTarget_)
	{
		FaceHorizontal(VSub(attackTargetPos_, transform_.pos));
	}

	if (held)
	{
		return;
	}

	// 離した
	const float holdTime = KiHoldTime_;
	EndKiHold();

	// 溜めていれば、溜めた分のフィニッシュ気弾。短ければ小弾
	if (holdTime < cfg::KiShot::MIN_CHARGE_TIME)
	{
		kiPendingRate_ = -1.0f;
	}
	else
	{
		float rate = holdTime / cfg::KiShot::CHARGE_TIME;
		if (rate > 1.0f) { rate = 1.0f; }

		kiPendingRate_ = rate;
	}

	// 投げるアニメを始めて、撃つタイミングで撃つ
	kiAnimStarted_ = true;
	isKiBlastShot_ = false;
	kiBlastTimer_ = 0.0f;
	animationController_->Play((int)ANIM_TYPE::KI_BLAST, false);
}

void Player::EndKiHold(void)
{
	isKiHold_ = false;
	KiHoldTime_ = 0.0f;

	EffekseerEffect::GetInstance()->StopChargeEffect();
}

const std::vector<std::unique_ptr<KiBlast>>& Player::GetKiBlasts(void) const
{
	return kiBlasts_;
}
