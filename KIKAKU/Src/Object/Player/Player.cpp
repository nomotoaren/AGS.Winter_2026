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

Player::Player(void)
	:
	animationController_(nullptr),
	state_(STATE::NONE),

	movePow_(AsoUtility::VECTOR_ZERO),
	movedPos_(AsoUtility::VECTOR_ZERO),
	knockBackPow_(AsoUtility::VECTOR_ZERO),
	isDamage_(false),
	damageTimer_(0.0f),

	playerRotY_(Quaternion()),

	jumpPow_(AsoUtility::VECTOR_ZERO),
	isJump_(false),
	stepJump_(0.0f),

	// 衝突チェック
	gravHitPosDown_(AsoUtility::VECTOR_ZERO),
	gravHitPosUp_(AsoUtility::VECTOR_ZERO),

	imgShadow_(-1),
	isRecording_(false),

	// 攻撃
	hasAttackTarget_(false),
	attackTargetPos_(AsoUtility::VECTOR_ZERO),
	canChase_(false),
	isAfterImage_(false),
	afterImageTimer_(0.0f),
	afterImageMatrix_({}),

	// 気溜め
	isCharging_(false),
	isChargeEnding_(false),
	ki_(0.0f),

	// 気弾
	isKiBlast_(false),
	isKiBlastShot_(false),
	kiBlastTimer_(0.0f),

	// ロックオン
	isLockOn_(false),
	lockOnPitch_(0.0f),

	// 回避
	isDodge_(false),
	dodgeTimer_(0.0f),
	dodgeDir_(AsoUtility::VECTOR_ZERO),

	hp_(MAX_HP),
	isDead_(false),

	attackEndTimer_(0.0f),
	rightHandFrame_(-1),

	capsule_(nullptr),

	// 変身
	form_(FORM::BASE),
	nextForm_(FORM::BASE)
{
	// 状態ごとの初期処理を登録
	stateChanges_.emplace(STATE::NONE, std::bind(&Player::ChangeStateNone, this));
	stateChanges_.emplace(STATE::PLAY, std::bind(&Player::ChangeStatePlay, this));
}

Player::~Player(void)
{
}

void Player::Init(void)
{
	// モデル
	transform_.SetModel(resMng_.LoadModelDuplicate(ResourceManager::SRC::PLAYER));

	transform_.scl = { 1.5f, 1.5f, 1.5f };
	transform_.pos = { 0.0f, 1000.0f, 0.0f };
	transform_.quaRot = Quaternion();
	transform_.quaRotLocal =
		Quaternion::Euler({ 0.0f, AsoUtility::Deg2RadF(180.0f), 0.0f });
	transform_.Update();

	// アニメーション
	InitAnimation(kFormData[(int)FORM::BASE].animDir);

	// 手・腰・脚のフレーム
	InitFrames();

	// かめはめ波
	kamehame_.Init(transform_.pos);

	// カプセルコライダ
	capsule_ = std::make_unique<Capsule>(transform_);
	capsule_->SetLocalPosTop({ 0.0f, 110.0f, 0.0f });
	capsule_->SetLocalPosDown({ 0.0f, 30.0f, 0.0f });
	capsule_->SetRadius(20.0f);

	// 丸影
	imgShadow_ = resMng_.Load(ResourceManager::SRC::PLAYER_SHADOW).handleId_;

	// 変身後のモデルも一度読んでおき、通常の形態に戻す
	ApplyForm(FORM::SUPER);
	ApplyForm(FORM::BASE);

	// HUD
	hud_.Init();
	hud_.SetName("PLAYER");
	hud_.SetIconText("P");

	// 初期状態
	ChangeState(STATE::PLAY);
}

void Player::InitAnimation(const std::string& animDir)
{
	const std::string basePath =
		Application::PATH_MODEL + kFormData[(int)FORM::BASE].animDir;
	const std::string path = Application::PATH_MODEL + animDir;

	animationController_ = std::make_unique<AnimationController>(transform_.modelId);

	// 形態のフォルダに無いアニメは、通常形態のものを使う
	auto add = [&](ANIM_TYPE type, const char* file, float speed)
		{
			std::string full = path + file;

			if (!std::filesystem::exists(full))
			{
				full = basePath + file;
			}

			animationController_->Add((int)type, full, speed);
		};

	// 待機モーション
	add(ANIM_TYPE::IDLE, "Idle.mv1", 20.0f);

	// 移動
	add(ANIM_TYPE::RUN, "Walk.mv1", 20.0f);
	add(ANIM_TYPE::FAST_RUN, "Running.mv1", 20.0f);
	add(ANIM_TYPE::LOCK_LEFT, "LSWalking.mv1", 20.0f);
	add(ANIM_TYPE::LOCK_RIGHT, "RSWalking.mv1", 20.0f);
	add(ANIM_TYPE::LOCK_LEFT_RUN, "Left Strafe.mv1", 20.0f);
	add(ANIM_TYPE::LOCK_RIGHT_RUN, "Right Strafe.mv1", 20.0f);
	add(ANIM_TYPE::LOCK_BACK, "Walking Back.mv1", 20.0f);
	add(ANIM_TYPE::LOCK_BACK_RUN, "Running Back.mv1", 20.0f);
	add(ANIM_TYPE::BOOST_CHASE, "Flying.mv1", 40.0f);

	// 攻撃
	add(ANIM_TYPE::ATTACK01, "Attack01.mv1", 60.0f);
	add(ANIM_TYPE::ATTACK02, "Attack02.mv1", 60.0f);
	add(ANIM_TYPE::ATTACK03, "Attack03.mv1", 80.0f);
	add(ANIM_TYPE::ATTACK04, "Attack04.mv1", 80.0f);
	add(ANIM_TYPE::ATTACK05, "Attack05.mv1", 85.0f);
	add(ANIM_TYPE::ATTACK06, "Attack01.mv1", 80.0f);
	add(ANIM_TYPE::ATTACK07, "Attack07.mv1", 65.0f);
	add(ANIM_TYPE::ATTACK08, "Attack08.mv1", 65.0f);

	// 気弾・かめはめ波・気溜め
	add(ANIM_TYPE::KI_BLAST, "KiBlast.mv1", 60.0f);
	add(ANIM_TYPE::KAMEHAME, "Special.mv1", 40.0f);
	add(ANIM_TYPE::BARRAGE, "Special2.mv1", 40.0f);
	add(ANIM_TYPE::CHARGE, "pawer.mv1", 40.0f);

	// ダメージ・ガード
	add(ANIM_TYPE::DAMAGE, "Damege.mv1", 60.0f);
	add(ANIM_TYPE::GUARD, "Block.mv1", 80.0f);
	add(ANIM_TYPE::GUARD_BURST, "pawer.mv1", 100.0f);
	add(ANIM_TYPE::GUARD_BREAK, "GuardBreak.mv1", 20.0f);

	// 初期アニメーション
	animationController_->Play((int)ANIM_TYPE::IDLE);
}

// モデルのフレームを探し直す(モデルが変わったら呼ぶ)
void Player::InitFrames(void)
{
	kamehame_.SetModel(transform_.modelId);
	rightHandFrame_ = MV1SearchFrame(transform_.modelId, "mixamorig:RightHand");
	barrage_.SetModel(transform_.modelId);

	hipsFrame_ = MV1SearchFrame(transform_.modelId, "mixamorig:Hips");

	InitLegFrames();
}

// 形態を切り替える(モデル、アニメ、フレームを作り直す)
void Player::ApplyForm(FORM form)
{
	const FormData& data = kFormData[(int)form];

	// 古いモデルを参照しているアニメを先に消す
	animationController_.reset();

	const int oldModel = transform_.modelId;

	// 新しい形態のモデル
	const int newModel = resMng_.LoadModelDuplicate(data.model);
	transform_.SetModel(newModel);
	transform_.scl = { data.scale, data.scale, data.scale };
	transform_.quaRotLocal =
		Quaternion::Euler({ 0.0f, AsoUtility::Deg2RadF(180.0f), 0.0f });
	transform_.Update();

	// アニメーションとフレーム
	InitAnimation(data.animDir);
	InitFrames();

	// 古いモデルの残像は消す
	boostAfterImages_.clear();
	isAfterImage_ = false;

	// 古いモデルを消す
	MV1DeleteModel(oldModel);

	form_ = form;
}

// 変身(H+G / L2+R2+□)
//   気が足りれば変身、変身中なら元に戻る
void Player::UpdateTransform(void)
{
	auto& ins = InputManager::GetInstance();

	// 変身を始める
	if (!formChange_.IsActive() &&
		!attack_.IsAttack() &&
		!kamehame_.IsActive() &&
		!isKiBlast_ &&
		!isCharging_ &&
		!isChargeEnding_ &&
		!boostChase_.IsActive() &&
		((ins.IsNew(KEY_INPUT_H) &&	// Hを押しながらG
			ins.IsTrgDown(KEY_INPUT_G)) ||
			PadInput::IsTransformTrg()))	// パッド
	{
		const FORM next = (form_ == FORM::BASE) ? FORM::SUPER : FORM::BASE;

		// 元に戻るときは気を使わない
		if (next == FORM::BASE || UseKi(cfg::Transform::KI_COST))
		{
			nextForm_ = next;

			movePow_ = AsoUtility::VECTOR_ZERO;

			// 溜めモーションを1回だけ流す
			animationController_->ClearEndLoop();
			animationController_->Play((int)ANIM_TYPE::CHARGE, false,
				0.0f, cfg::Transform::ANIM_END_STEP, false, true);

			formChange_.Start(next == FORM::SUPER, transform_.pos);
		}
	}

	if (!formChange_.IsActive())
	{
		return;
	}

	movePow_ = AsoUtility::VECTOR_ZERO;

	const PlayerTransform::Result result =
		formChange_.Update(scnMng_.GetDeltaTime(), transform_.pos, animationController_->IsEnd());

	// モーションの途中でモデルを差し替える
	if (result.swap)
	{
		ApplyForm(nextForm_);

		// 差し替えたモデルで、溜めモーションの続きを流す
		animationController_->ClearEndLoop();
		animationController_->Play((int)ANIM_TYPE::CHARGE, false,
			result.step, cfg::Transform::ANIM_END_STEP, false, true);

		// 変身するときだけ画面を揺らす
		if (formChange_.IsToSuper())
		{
			mainCamera.StartShake(0.3f, 6.0f);
		}
	}

	if (result.finished)
	{
		animationController_->Play((int)ANIM_TYPE::IDLE);
	}
}

bool Player::IsTransforming(void) const
{
	return formChange_.IsActive();
}

float Player::GetAttackRate(void) const
{
	return kFormData[(int)form_].attackRate * (heatUp_ ? cfg::HeatUp::ATTACK_RATE : 1.0f);
}

int Player::GetAttackDamage(int baseDamage) const
{
	// ジャスト回避のあとは、ダメージが上がる
	const float justRate = (justBuffTimer_ > 0.0f) ? cfg::JustDodge::DAMAGE_RATE : 1.0f;

	// ダメージの計算(クリティカルもここで決まる)
	const DamageCalc::Result result =
		DamageCalc::Roll(baseDamage * GetAttackRate() * justRate);

	lastCritical_ = result.critical;
	return result.damage;
}

void Player::Update(void)
{
	// パッドの入力
	PadInput::Update();

	// HUD
	hud_.Update(*this);

	// テンション・ヒートアップ・追撃などのタイマー
	UpdateTensionTimers(scnMng_.GetDeltaTime());

	UpdateKnockBack();

	// ダメージ中は動けない
	if (UpdateDamage())
	{
		return;
	}

	// 状態ごとの更新
	stateUpdate_();

	// ロックオン中は、敵の方へ上下にも傾ける
	Quaternion logicRot = playerRotY_;
	if (isLockOn_ && hasAttackTarget_ && !attack_.IsAttack())
	{
		logicRot = playerRotY_.Mult(Quaternion::Euler({ lockOnPitch_, 0.0f, 0.0f }));
	}

	// 浮遊の見た目(傾き、上下ゆれ)
	UpdateFloatLean();
	ApplyVisualTransform(logicRot);

	animationController_->Update();

	CancelRootMotion();

	// 浮遊中の脚のポーズ
	UpdateLegPose();

	UpdateAfterImages();
}

// 敵と重ならないように位置をずらす
void Player::PushOut(const VECTOR& offset)
{
	transform_.pos = VAdd(transform_.pos, offset);

	// 見た目の位置も合わせる
	ApplyVisualTransform(transform_.quaRot);
}

void Player::UpdateKnockBack(void)
{
	if (VSize(knockBackPow_) > Constants::KnockBackThreshold)
	{
		const float fs = FrameScale();

		transform_.pos = VAdd(transform_.pos, VScale(knockBackPow_, fs));

		// 地面より下には行かない
		if (transform_.pos.y < Constants::MinHeight)
		{
			transform_.pos.y = Constants::MinHeight;
			knockBackPow_.y = 0.0f;
		}

		knockBackPow_ = VScale(knockBackPow_, powf(Constants::KnockBackDamping, fs));
	}
	else
	{
		knockBackPow_ = AsoUtility::VECTOR_ZERO;
	}
}

bool Player::UpdateDamage(void)
{
	if (!isDamage_)
	{
		return false;
	}

	damageTimer_ -= SceneManager::GetInstance().GetDeltaTime();

	if (damageTimer_ <= 0.0f)
	{
		damageTimer_ = 0.0f;
		isDamage_ = false;

		animationController_->Play((int)ANIM_TYPE::IDLE);
		return false;
	}

	// ダメージ中も浮遊の見た目は続ける
	UpdateFloatLean();
	ApplyVisualTransform(damageRot_);
	animationController_->Update();
	UpdateLegPose();					// ダメージ中はポーズを外していく
	return true;
}

void Player::ChangeState(STATE state)
{
	state_ = state;

	// 状態ごとの初期処理
	stateChanges_[state_]();
}

void Player::ChangeStateNone(void)
{
	stateUpdate_ = std::bind(&Player::UpdateNone, this);
}

void Player::ChangeStatePlay(void)
{
	stateUpdate_ = std::bind(&Player::UpdatePlay, this);
}

void Player::UpdateNone(void)
{
}

void Player::UpdatePlay(void)
{
	attack_.BeginFrame();

	UpdateGuard();

	// ガード、ガードブレイク・バースト中は動けない
	if (guard_.IsBusy())
	{
		movePow_ = AsoUtility::VECTOR_ZERO;
		return;
	}

	// 変身
	UpdateTransform();

	// 変身中は他のことをしない
	if (formChange_.IsActive())
	{
		Collision();
		return;
	}

	// ヒートアップ
	UpdateHeatUp();

	// ヒートアップ発動の演出中は動けない
	if (heatActivate_ > 0.0f)
	{
		movePow_ = AsoUtility::VECTOR_ZERO;
		Collision();
		return;
	}

	// 気溜め
	UpdateKiCharge();

	// 気溜め中は他のことをしない
	if (isCharging_ || isChargeEnding_)
	{
		return;
	}

	UpdateBarrage();
	UpdateKiBlast();
	UpdateAttack();
	UpdateKamehame();
	UpdateBoostChase();
	UpdateDodge();

	if (!boostChase_.IsActive() && !isDodge_)
	{
		ProcessMove();
	}

	// 追撃
	UpdateAttackChase();

	jumpPow_ = AsoUtility::VECTOR_ZERO;

	Collision();

	transform_.quaRot = playerRotY_;
}

// 攻撃判定
bool Player::IsAttack(void) const
{
	return attack_.IsAttack();
}

bool Player::IsAttackHitTiming(void) const { return attack_.IsHitTiming(); }

bool Player::HasAttackHit(void) const { return attack_.HasHit(); }

void Player::SetAttackHit(void) { attack_.SetHit(); }

VECTOR Player::GetForward(void) const
{
	return playerRotY_.GetForward();
}

bool Player::IsDead(void) const { return isDead_; }

int Player::GetHp(void) const { return hp_; }

int Player::GetCombo(void) const { return attack_.GetCombo(); }

bool Player::IsRecording(void) const { return isRecording_; }

void Player::StopRecord(void) { isRecording_ = false; }

void Player::SetLockOn(bool lockOn) { isLockOn_ = lockOn; }

bool Player::IsLockOn(void) const { return isLockOn_; }

void Player::SetAttackTarget(VECTOR pos)
{
	hasAttackTarget_ = true;
	attackTargetPos_ = pos;
}

void Player::ClearAttackTarget(void)
{
	hasAttackTarget_ = false;
}

void Player::SetCanChase(bool canChase) { canChase_ = canChase; }

bool Player::CanChase(void) const { return canChase_; }

bool Player::UseKi(float amount)
{
	// ヒートアップ中は気を使わない
	if (heatUp_)
	{
		return true;
	}

	if (ki_ < amount)
	{
		return false;
	}

	ki_ -= amount;
	return true;
}

// 衝突判定に使うコライダ
void Player::AddCollider(std::weak_ptr<Collider> collider)
{
	colliders_.push_back(collider);
}

void Player::ClearCollider(void)
{
	colliders_.clear();
}

const Capsule& Player::GetCapsule(void) const
{
	return *capsule_;
}
