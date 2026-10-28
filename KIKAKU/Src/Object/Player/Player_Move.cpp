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
#include "../../Manager/Camera.h"
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

// Y成分を捨てる
VECTOR Player::ToHorizontal(VECTOR v)
{
	v.y = 0.0f;
	return v;
}

// 長さがあるときだけ正規化する
VECTOR Player::NormalizeSafe(VECTOR v)
{
	return (VSize(v) > Constants::Epsilon) ? VNorm(v) : v;
}

// 正規化済みの方向を向く
void Player::FaceDirection(VECTOR dir)
{
	playerRotY_ = Quaternion::LookRotation(dir);
}

// 水平成分だけで向く(向けなければ false)
bool Player::FaceHorizontal(VECTOR dir)
{
	dir = ToHorizontal(dir);

	if (VSize(dir) <= Constants::Epsilon)
	{
		return false;
	}

	FaceDirection(VNorm(dir));
	return true;
}

// ロックオン時の上下の傾き
void Player::SetLockOnPitch(VECTOR dir)
{
	float horizontalDistance = sqrtf(dir.x * dir.x + dir.z * dir.z);
	lockOnPitch_ = -atan2f(dir.y, horizontalDistance);
}

// 移動の基準(前・右)を求める
//   ロックオン中は敵の方向、それ以外はカメラの方向
bool Player::GetMoveBasis(VECTOR& forward, VECTOR& right)
{
	if (isLockOn_ && hasAttackTarget_)
	{
		const VECTOR toTarget = ToHorizontal(VSub(attackTargetPos_, transform_.pos));

		// 敵が近すぎると向きが定まらないので、
		//   そのときはカメラ基準にする
		if (VSize(toTarget) > cfg::Move::LOCK_BASIS_MIN_DISTANCE)
		{
			forward = VNorm(toTarget);
			right = VGet(forward.z, 0.0f, -forward.x);
			return true;
		}
	}

	Quaternion cameraRot = mainCamera.GetQuaRotOutX();

	forward = NormalizeSafe(ToHorizontal(cameraRot.GetForward()));
	right = NormalizeSafe(ToHorizontal(cameraRot.GetRight()));
	return true;
}

void Player::ProcessMove(void)
{
	auto& ins = InputManager::GetInstance();

	if (kamehame_.IsActive())
	{
		movePow_ = AsoUtility::VECTOR_ZERO;
		return;
	}

	if (attack_.IsAttack() || attack_.IsChasing())
	{
		return;
	}

	movePow_ = AsoUtility::VECTOR_ZERO;

	// 移動の基準
	VECTOR forward;
	VECTOR right;
	GetMoveBasis(forward, right);

	// 移動の向き
	VECTOR dir = AsoUtility::VECTOR_ZERO;

	if (ins.IsNew(KEY_INPUT_W)) { dir = VAdd(dir, forward); }
	if (ins.IsNew(KEY_INPUT_S)) { dir = VSub(dir, forward); }
	if (ins.IsNew(KEY_INPUT_D)) { dir = VAdd(dir, right); }
	if (ins.IsNew(KEY_INPUT_A)) { dir = VSub(dir, right); }

	// 上下の移動(E: 上 / Q: 下)
	float verticalMove = 0.0f;
	if (ins.IsNew(KEY_INPUT_E)) { verticalMove = Constants::VerticalMoveSpeed; }
	if (ins.IsNew(KEY_INPUT_Q)) { verticalMove = -Constants::VerticalMoveSpeed; }

	// パッド: 上下移動モード中は、スティックの縦で上下に動く
	if (PadInput::IsVerticalMode())
	{
		verticalMove = PadInput::StickY() * Constants::VerticalMoveSpeed;
		dir = VAdd(dir, VScale(right, PadInput::StickX()));
	}
	else
	{
		dir = VAdd(dir, VScale(forward, PadInput::StickY()));
		dir = VAdd(dir, VScale(right, PadInput::StickX()));
	}

	// 斜めでも速くならないようにする
	dir = NormalizeSafe(dir);

	// ×を押し続けるとダッシュ
	if (PadInput::IsNew(PadInput::BTN::CROSS))
	{
		dashHold_ += scnMng_.GetDeltaTime();
	}
	else
	{
		dashHold_ = 0.0f;
	}

	const bool isDash = dashHold_ >= cfg::Move::DASH_HOLD_TIME && !isDodge_;

	if (!AsoUtility::EqualsVZero(dir) &&
		(isJump_ || IsEndLanding()))
	{
		const bool isRun = ins.IsNew(KEY_INPUT_RSHIFT) || isDash;

		// 速さ(ダッシュ / 走り / 歩き)に、形態とヒートアップの倍率をかける
		const float speed = (isDash ? cfg::Move::DASH_SPEED : (isRun ? SPEED_RUN : SPEED_MOVE))
			* kFormData[(int)form_].speedRate
			* (heatUp_ ? cfg::HeatUp::SPEED_RATE : 1.0f);
		movePow_ = VScale(dir, speed);

		// ロックオン中は敵の方を向いたまま動く
		if (!isLockOn_)
		{
			FaceDirection(dir);
		}

		if (!isJump_ && IsEndLanding() && !isKiBlast_)
		{
			PlayMoveAnimation(isRun);
		}
	}
	else if (!isJump_ &&
		IsEndLanding() &&
		attackEndTimer_ <= 0.0f &&
		!isKiBlast_)
	{
		animationController_->Play((int)ANIM_TYPE::IDLE);
	}

	// 上下の移動
	movePow_.y = verticalMove;

	// 高さの範囲に収める(movePow_ は Collision で FrameScale 倍されるので、その分も見込む)
	const float fs = FrameScale();

	if (fs > Constants::Epsilon)
	{
		const float nextY = transform_.pos.y + movePow_.y * fs;

		if (nextY < Constants::MinHeight)
		{
			movePow_.y = (Constants::MinHeight - transform_.pos.y) / fs;
		}

		if (nextY > Constants::MaxHeight)
		{
			movePow_.y = (Constants::MaxHeight - transform_.pos.y) / fs;
		}
	}
}

// 移動のアニメーション(ロックオン中は向きに合わせて横歩き・後ろ歩き)
void Player::PlayMoveAnimation(bool isRun)
{
	auto& ins = InputManager::GetInstance();

	ANIM_TYPE type = isRun ? ANIM_TYPE::FAST_RUN : ANIM_TYPE::RUN;

	// ロックオン中は、入力の向きでアニメを変える
	if (isLockOn_)
	{
		const bool w = ins.IsNew(KEY_INPUT_W) || PadInput::StickY() > 0.3f;
		const bool a = ins.IsNew(KEY_INPUT_A) || PadInput::StickX() < -0.3f;
		const bool s = ins.IsNew(KEY_INPUT_S) || PadInput::StickY() < -0.3f;
		const bool d = ins.IsNew(KEY_INPUT_D) || PadInput::StickX() > 0.3f;

		if (s && !w)
		{
			type = isRun ? ANIM_TYPE::LOCK_BACK_RUN : ANIM_TYPE::LOCK_BACK;
		}
		else if (a && !d)
		{
			type = isRun ? ANIM_TYPE::LOCK_LEFT_RUN : ANIM_TYPE::LOCK_LEFT;
		}
		else if (d && !a)
		{
			type = isRun ? ANIM_TYPE::LOCK_RIGHT_RUN : ANIM_TYPE::LOCK_RIGHT;
		}
	}

	animationController_->Play((int)type);
}

void Player::ProcessJump(void)
{
}

void Player::Collision(void)
{
	// 移動後の位置
	// (movePow_ は 144fps の1フレーム分の量。今のフレームの長さに合わせて掛ける)
	movedPos_ = VAdd(transform_.pos, VScale(movePow_, FrameScale()));

	CollisionCapsule();
	CollisionGravity();

	transform_.pos = movedPos_;
}

void Player::CollisionGravity(void)
{
	// 重力(ジャンプ)の分
	movedPos_ = VAdd(movedPos_, jumpPow_);

	const VECTOR dirGravity = AsoUtility::DIR_D;
	const VECTOR dirUpGravity = AsoUtility::DIR_U;
	const float gravityPow = Planet::DEFAULT_GRAVITY_POW;
	const float checkPow = 10.0f;

	gravHitPosUp_ = VAdd(movedPos_, VScale(dirUpGravity, gravityPow));
	gravHitPosUp_ = VAdd(gravHitPosUp_, VScale(dirUpGravity, checkPow * 2.0f));
	gravHitPosDown_ = VAdd(movedPos_, VScale(dirGravity, checkPow));

	for (const auto& c : colliders_)
	{
		auto collider = c.lock();
		if (!collider)
		{
			continue;
		}

		// 足元の地面との当たり判定
		auto hit = MV1CollCheck_Line(
			collider->modelId_, -1, gravHitPosUp_, gravHitPosDown_);

		if (hit.HitFlag > 0 && VDot(dirGravity, jumpPow_) > 0.9f)
		{
			// 地面の少し上に合わせる
			movedPos_ = VAdd(hit.HitPosition, VScale(dirUpGravity, 2.0f));

			// 着地
			jumpPow_ = AsoUtility::VECTOR_ZERO;
			stepJump_ = 0.0f;
			isJump_ = false;
		}
	}
}

void Player::CollisionCapsule(void)
{
	// 移動後の位置でカプセルを作る
	Transform trans = Transform(transform_);
	trans.pos = movedPos_;
	trans.Update();
	Capsule cap = Capsule(*capsule_, trans);

	for (const auto& c : colliders_)
	{
		auto collider = c.lock();
		if (!collider)
		{
			continue;
		}

		auto hits = MV1CollCheck_Capsule(
			collider->modelId_, -1,
			cap.GetPosTop(), cap.GetPosDown(), cap.GetRadius());

		for (int i = 0; i < hits.HitNum; i++)
		{
			const auto& hit = hits.Dim[i];

			// 当たらなくなるまで、面の向きに少しずつ押し出す
			for (int tryCnt = 0; tryCnt < 10; tryCnt++)
			{
				int pHit = HitCheck_Capsule_Triangle(
					cap.GetPosTop(), cap.GetPosDown(), cap.GetRadius(),
					hit.Position[0], hit.Position[1], hit.Position[2]);

				if (!pHit)
				{
					break;
				}

				movedPos_ = VAdd(movedPos_, VScale(hit.Normal, 1.0f));
				trans.pos = movedPos_;
				trans.Update();
			}
		}

		// 当たり判定の結果を片付ける
		MV1CollResultPolyDimTerminate(hits);
	}
}

void Player::CalcGravityPow(void)
{
	const VECTOR dirGravity = AsoUtility::DIR_D;
	const float gravityPow = Planet::DEFAULT_GRAVITY_POW;

	VECTOR gravity = VScale(dirGravity, gravityPow);
	jumpPow_ = VAdd(jumpPow_, gravity);

	// 上向きに進んでいなければ、重力の速さにそろえる
	float dot = VDot(dirGravity, jumpPow_);
	if (dot >= 0.0f)
	{
		jumpPow_ = gravity;
	}
}

bool Player::IsEndLanding(void)
{
	return true;
}

// 追撃(攻撃ボタンで敵へ飛びこむ)
//   届いたら敵の手前に位置を合わせて攻撃、届かなければ空振り
void Player::UpdateAttackChase(void)
{
	if (!attack_.IsChasing())
	{
		return;
	}

	movePow_ = AsoUtility::VECTOR_ZERO;

	const PlayerAttack::ChaseResult result = attack_.UpdateChase(MakeAttackContext());

	if (VSize(result.faceDir) > Constants::Epsilon)
	{
		FaceHorizontal(result.faceDir);
	}

	switch (result.end)
	{
	case PlayerAttack::ChaseEnd::NONE:
	{
		// 最高速で進んでいるときは、そのまま(Collision で fps に合わせて伸ばす)。
		// 敵の手前に着く最後の1歩は、残りの距離ぶんだけ進めたいので、伸ばした分を戻す
		const float fs = FrameScale();

		if (VSize(result.move) >= cfg::Chase::SPEED - 0.001f || fs <= 0.0001f)
		{
			movePow_ = result.move;
		}
		else
		{
			movePow_ = VScale(result.move, 1.0f / fs);
		}
		break;
	}

	case PlayerAttack::ChaseEnd::CANCEL:
		EndChase(false);
		break;

	case PlayerAttack::ChaseEnd::LUNGE:
		// 飛びこめる時間を使い切った: その場で攻撃(空振り)
		EndChase(false);
		StartAttack();
		break;

	case PlayerAttack::ChaseEnd::ATTACK:
		EndChase(true);
		break;
	}
}

// 追撃を終える(startAttack が true なら、敵の手前に合わせて攻撃を始める)
void Player::EndChase(bool startAttack)
{
	canChase_ = false;
	movePow_ = AsoUtility::VECTOR_ZERO;

	if (!startAttack)
	{
		animationController_->Play((int)ANIM_TYPE::IDLE);
		return;
	}

	SnapToTargetAndAttack();
}

// 敵の手前(水平)に位置を合わせて、1段目の攻撃を始める
void Player::SnapToTargetAndAttack(void)
{
	VECTOR snapPos;

	if (PlayerAttack::CalcSnapPos(MakeAttackContext(), snapPos))
	{
		transform_.pos.x = snapPos.x;
		transform_.pos.z = snapPos.z;

		FaceHorizontal(VSub(attackTargetPos_, transform_.pos));
	}

	StartAttack();
}

// 高速接近(ロックオン中に SPACE / L3)
//   弧を描いて敵の懐へ近づき、着いたらそのまま攻撃する
void Player::UpdateBoostChase(void)
{
	auto& ins = InputManager::GetInstance();

	boostChase_.Tick(scnMng_.GetDeltaTime());

	PlayerBoostChase::Context ctx{};
	ctx.playerPos = transform_.pos;
	ctx.targetPos = attackTargetPos_;
	ctx.hasTarget = isLockOn_ && hasAttackTarget_;
	ctx.interrupted = attack_.IsAttack();
	ctx.deltaTime = scnMng_.GetDeltaTime();

	// 高速接近を始める
	if (boostChase_.CanStart() &&
		isLockOn_ &&
		hasAttackTarget_ &&
		!attack_.IsAttack() &&
		!kamehame_.IsActive() &&
		!attack_.IsChasing() &&
		!isKiBlast_ &&
		!isDodge_ &&
		(ins.IsTrgDown(KEY_INPUT_SPACE) || PadInput::IsHighBoostTrg()))
	{
		movePow_ = AsoUtility::VECTOR_ZERO;

		// 弧を膨らませる側(横の入力があればその側)
		int sideInput = 0;
		if (ins.IsNew(KEY_INPUT_D) || PadInput::StickX() > 0.3f)
		{
			sideInput = 1;
		}
		else if (ins.IsNew(KEY_INPUT_A) || PadInput::StickX() < -0.3f)
		{
			sideInput = -1;
		}

		boostChase_.Start(ctx, sideInput);

		animationController_->Play((int)ANIM_TYPE::BOOST_CHASE);
	}

	if (!boostChase_.IsActive())
	{
		return;
	}

	const PlayerBoostChase::Result result = boostChase_.Update(ctx);

	if (result.finished)
	{
		EndBoostChase();

		// 敵の懐に着いたら、そのまま1段目の攻撃へ
		if (result.arrived && cfg::Boost::AUTO_ATTACK && hasAttackTarget_)
		{
			SnapToTargetAndAttack();
		}

		return;
	}

	// 進む向きを向く
	if (VSize(result.faceDir) > Constants::Epsilon)
	{
		FaceHorizontal(result.faceDir);
		SetLockOnPitch(result.faceDir);
	}

	// 時間(dt)で決まった今のフレームの移動量。最高速に達しているときだけ、
	// fps に合わせて伸ばす(それ以外は Collision で伸びる分を戻す)
	{
		const float fs = FrameScale();

		if (VSize(result.move) >= cfg::Boost::MAX_SPEED - 0.001f || fs <= 0.0001f)
		{
			movePow_ = result.move;
		}
		else
		{
			movePow_ = VScale(result.move, 1.0f / fs);
		}
	}

	// 一定間隔で残像を残す
	if (result.leaveAfterImage)
	{
		AddBoostAfterImage();
	}
}

// 高速接近を終える
void Player::EndBoostChase(void)
{
	boostChase_.Cancel();
	movePow_ = AsoUtility::VECTOR_ZERO;

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

// 回避ステップ(LSHIFT / パッドの×)
void Player::UpdateDodge(void)
{
	auto& ins = InputManager::GetInstance();

	// ジャスト回避のタイマー
	{
		const float dt = scnMng_.GetDeltaTime();

		if (justCooldown_ > 0.0f) { justCooldown_ -= dt; }
		if (justBuffTimer_ > 0.0f) { justBuffTimer_ -= dt; }
	}

	const bool trg = ins.IsTrgDown(KEY_INPUT_J) || PadInput::IsStepTrg();

	// ステップを始められるか
	//   (ステップ中は、少したてば次のステップを重ねて出せる)
	bool canStart = false;

	if (!isDodge_)
	{
		canStart = !(attack_.IsAttack() || boostChase_.IsActive() || kamehame_.IsActive());
	}
	else
	{
		canStart = dodgeTimer_ >= cfg::Step::CHAIN_TIME;
	}

	if (trg && canStart)
	{
		StartStep();
	}

	if (!isDodge_)
	{
		return;
	}

	dodgeTimer_ += scnMng_.GetDeltaTime();

	// 出だしが速く、だんだん遅くなる
	float t = dodgeTimer_ / cfg::Step::DURATION;

	if (t > 1.0f) { t = 1.0f; }

	const float ease = 1.0f - (1.0f - t) * (1.0f - t);	// 減速の曲線
	const float speed = cfg::Step::SPEED_START + (cfg::Step::SPEED_END - cfg::Step::SPEED_START) * ease;

	movePow_ = VScale(dodgeDir_, speed);

	// ロックオン中は敵の方を向いたまま
	if (isLockOn_ && hasAttackTarget_)
	{
		VECTOR dir = VSub(attackTargetPos_, transform_.pos);

		if (VSize(dir) > Constants::Epsilon)
		{
			FaceDirection(VNorm(dir));
		}
	}

	// 一定間隔で残像を残す
	stepAfterImageTimer_ -= scnMng_.GetDeltaTime();

	if (stepAfterImageTimer_ <= 0.0f)
	{
		AddBoostAfterImage();

		stepAfterImageTimer_ = cfg::Step::AFTER_IMAGE_INTERVAL;
	}

	if (dodgeTimer_ >= cfg::Step::DURATION)
	{
		isDodge_ = false;
		dodgeTimer_ = 0.0f;

		movePow_ = AsoUtility::VECTOR_ZERO;
	}
}

// ステップを始める(向き・アニメ)
void Player::StartStep(void)
{
	auto& ins = InputManager::GetInstance();

	VECTOR forward;
	VECTOR right;

	if (!GetMoveBasis(forward, right))
	{
		return;
	}

	// 入力の向きへ(入力が無ければ後ろへ)
	if (PadInput::IsStickTilted())
	{
		dodgeDir_ = NormalizeSafe(VAdd(
			VScale(forward, PadInput::StickY()),
			VScale(right, PadInput::StickX())));
	}
	else if (ins.IsNew(KEY_INPUT_W)) { dodgeDir_ = forward; }
	else if (ins.IsNew(KEY_INPUT_S)) { dodgeDir_ = VScale(forward, -1.0f); }
	else if (ins.IsNew(KEY_INPUT_A)) { dodgeDir_ = VScale(right, -1.0f); }
	else if (ins.IsNew(KEY_INPUT_D)) { dodgeDir_ = right; }
	else { dodgeDir_ = VScale(forward, -1.0f); }

	isDodge_ = true;
	dodgeTimer_ = 0.0f;
	stepAfterImageTimer_ = 0.0f;

	// 進む向きに合わせたアニメを選ぶ
	{
		const VECTOR f = NormalizeSafe(ToHorizontal(transform_.quaRot.GetForward()));
		const VECTOR r = VGet(f.z, 0.0f, -f.x);
		const VECTOR d = ToHorizontal(dodgeDir_);

		const float dotF = VDot(d, f);
		const float dotR = VDot(d, r);

		ANIM_TYPE anim = ANIM_TYPE::FAST_RUN;

		if (fabsf(dotR) > fabsf(dotF))
		{
			anim = (dotR > 0.0f) ? ANIM_TYPE::LOCK_RIGHT_RUN : ANIM_TYPE::LOCK_LEFT_RUN;
		}
		else if (dotF < 0.0f)
		{
			anim = ANIM_TYPE::LOCK_BACK_RUN;
		}

		animationController_->Play((int)anim);
	}
}

bool Player::IsDodging(void) const
{
	return isDodge_;
}

// ジャスト回避(敵の攻撃が回避の出だしに当たったら成立)
bool Player::TryJustDodge(void)
{
	if (!isDodge_ || dodgeTimer_ > cfg::JustDodge::WINDOW || justCooldown_ > 0.0f)
	{
		return false;
	}

	justCooldown_ = cfg::JustDodge::COOLDOWN;
	justBuffTimer_ = cfg::JustDodge::BUFF_TIME;
	justDodged_ = true;

	AddTension(cfg::Tension::GAIN_JUST_DODGE);

	AddKi(cfg::JustDodge::KI_GAIN);

	return true;
}

//----------------------------------------------------------------------
// テンションゲージ / ヒートアップ
//----------------------------------------------------------------------

void Player::DashStepTo(const VECTOR& pos, const VECTOR& targetPos)
{
	// 3回に1回だけ残像を残す
	if ((dashStepCount_++ % 3) == 0)
	{
		AddBoostAfterImage();
	}

	MoveAndFace(pos, targetPos);
}

void Player::VanishTo(const VECTOR& pos, const VECTOR& targetPos)
{
	// 残像を残して瞬間移動
	AddBoostAfterImage();
	MoveAndFace(pos, targetPos);
}

// pos へ移動して targetPos を向く(追撃・バニッシュアタック用)
void Player::MoveAndFace(const VECTOR& pos, const VECTOR& targetPos)
{
	movePow_ = AsoUtility::VECTOR_ZERO;
	transform_.pos = pos;
	vanishCalmTimer_ = cfg::Vanish::CAMERA_CALM_TIME;

	FaceHorizontal(VSub(targetPos, pos));
	transform_.Update();
}

void Player::AddKnockBack(VECTOR dir, float power)
{
	if (VSize(dir) <= Constants::Epsilon)
	{
		return;
	}

	knockBackPow_ = VScale(VNorm(dir), power);
}

void Player::AddAttackMove(VECTOR dir, float power)
{
	if (VSize(dir) <= Constants::Epsilon)
	{
		return;
	}

	transform_.pos = VAdd(transform_.pos, VScale(VNorm(dir), power));
}

// 敵の方を向く(上下の傾きも)
void Player::LookAtTarget(VECTOR targetPos)
{
	VECTOR dir = VSub(targetPos, transform_.pos);

	if (VSize(dir) <= Constants::Epsilon)
	{
		return;
	}

	SetLockOnPitch(dir);
	FaceHorizontal(dir);
}

// 攻撃してきた敵の方を向く(ダメージ中の向き)
void Player::LookAtDamageEnemy(VECTOR enemyPos)
{
	if (!FaceHorizontal(VSub(enemyPos, transform_.pos)))
	{
		return;
	}

	damageRot_ = playerRotY_;
	transform_.quaRot = damageRot_;
}
