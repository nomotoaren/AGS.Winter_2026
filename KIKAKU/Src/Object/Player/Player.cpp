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
#include "PlayerConfig.h"
#include "../../Manager/EffekseerEffect.h"

namespace cfg = PlayerConfig;

// 形態ごとのデータ
namespace
{
	// BASE: 通常 / SUPER: 変身後
	// モデル・アニメーションのフォルダ・スケール・移動速度の倍率・攻撃力の倍率
	const Player::FormData kFormData[(int)Player::FORM::MAX] =
	{
		{ ResourceManager::SRC::PLAYER,   "Player/Animation/", 1.5f, 1.0f, 1.0f },
		{ ResourceManager::SRC::EVPLAY_,  "Player/AnimationEV/", 0.8f, 0.8f, 2.0f },
	};

	// 1フレームあたりの移動量に掛ける倍率
	//   (移動の速さは 144fps の1フレーム分で決めてあるので、60fps なら約2.4倍にして同じ速さにする)
	float FrameScale(void)
	{
		float s = SceneManager::GetInstance().GetFrameScale();

		// 処理落ちした1フレームで、壁をすり抜けるほど飛ばないようにする
		if (s > 4.0f)
		{
			s = 4.0f;
		}

		return s;
	}

	// 移動キー(またはスティック)が押されているか
	bool IsMoveKeyDown(InputManager& ins)
	{
		return ins.IsNew(KEY_INPUT_W) ||
			ins.IsNew(KEY_INPUT_A) ||
			ins.IsNew(KEY_INPUT_S) ||
			ins.IsNew(KEY_INPUT_D) ||
			PadInput::IsStickTilted();
	}

	// 2つの行列を t(0~1) で混ぜる
	//   単純に混ぜると縮んだりゆがんだりするので、向きを直交させ直し、大きさは別に混ぜる
	MATRIX BlendMatrix(const MATRIX& a, const MATRIX& b, float t)
	{
		MATRIX r{};
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				r.m[i][j] = a.m[i][j] + (b.m[i][j] - a.m[i][j]) * t;
			}
		}

		auto row = [](const MATRIX& m, int i) { return VGet(m.m[i][0], m.m[i][1], m.m[i][2]); };

		// 各軸の大きさ(スケール)は別に混ぜる
		const float sx = VSize(row(a, 0)) + (VSize(row(b, 0)) - VSize(row(a, 0))) * t;
		const float sy = VSize(row(a, 1)) + (VSize(row(b, 1)) - VSize(row(a, 1))) * t;
		const float sz = VSize(row(a, 2)) + (VSize(row(b, 2)) - VSize(row(a, 2))) * t;

		VECTOR x = row(r, 0);
		VECTOR y = row(r, 1);

		if (VSize(x) < 0.0001f || VSize(y) < 0.0001f)
		{
			return (t < 0.5f) ? a : b;
		}

		x = VNorm(x);
		VECTOR z = VCross(x, y);
		if (VSize(z) < 0.0001f)
		{
			return (t < 0.5f) ? a : b;
		}
		z = VNorm(z);
		y = VCross(z, x);

		r.m[0][0] = x.x * sx; r.m[0][1] = x.y * sx; r.m[0][2] = x.z * sx; r.m[0][3] = 0.0f;
		r.m[1][0] = y.x * sy; r.m[1][1] = y.y * sy; r.m[1][2] = y.z * sy; r.m[1][3] = 0.0f;
		r.m[2][0] = z.x * sz; r.m[2][1] = z.y * sz; r.m[2][2] = z.z * sz; r.m[2][3] = 0.0f;
		r.m[3][3] = 1.0f;

		return r;
	}

	// mixamo のボーンを探す(名前の前に付く文字がモデルによって違うので、順に試す)
	int FindMixamoFrame(int model, const char* bone)
	{
		const char* prefixes[] = { "mixamorig:", "mixamorig1:", "mixamorig2:", "" };

		for (const char* p : prefixes)
		{
			const std::string name = std::string(p) + bone;
			const int frame = MV1SearchFrame(model, name.c_str());

			if (frame >= 0)
			{
				return frame;
			}
		}

		return -1;
	}
}

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

	// 脚のポーズの初期値
	namespace lp = cfg::LegPose;
	legTune_[TUNE_L_THIGH] = lp::L_THIGH;
	legTune_[TUNE_L_KNEE] = lp::L_KNEE;
	legTune_[TUNE_L_OPEN] = lp::L_OPEN;
	legTune_[TUNE_R_THIGH] = lp::R_THIGH;
	legTune_[TUNE_R_KNEE] = lp::R_KNEE;
	legTune_[TUNE_R_OPEN] = lp::R_OPEN;
	legTune_[TUNE_FOOT] = lp::FOOT;
	legTune_[TUNE_LEAN] = lp::LEAN_RATE;
}

// 脚のボーンを探し、回転軸を求める
void Player::InitLegFrames(void)
{
	const int model = transform_.modelId;

	legFrames_[LEG_L_UP] = FindMixamoFrame(model, "LeftUpLeg");
	legFrames_[LEG_L_KNEE] = FindMixamoFrame(model, "LeftLeg");
	legFrames_[LEG_L_FOOT] = FindMixamoFrame(model, "LeftFoot");
	legFrames_[LEG_R_UP] = FindMixamoFrame(model, "RightUpLeg");
	legFrames_[LEG_R_KNEE] = FindMixamoFrame(model, "RightLeg");
	legFrames_[LEG_R_FOOT] = FindMixamoFrame(model, "RightFoot");

	// 各ボーンから見た「キャラの左右の軸」と「キャラの前後の軸」を求めておく
	//   (ボーンごとに向きが違うので、モデル空間の軸をボーンの空間へ変換する)
	for (int i = 0; i < LEG_MAX; i++)
	{
		const int frame = legFrames_[i];
		legAxisSide_[i] = VGet(1.0f, 0.0f, 0.0f);
		legAxisFront_[i] = VGet(0.0f, 0.0f, 1.0f);

		if (frame < 0)
		{
			continue;
		}

		// ボーンからモデルの空間への行列(親をたどって掛ける)
		MATRIX toModel = MV1GetFrameBaseLocalMatrix(model, frame);
		for (int p = MV1GetFrameParent(model, frame); p >= 0; p = MV1GetFrameParent(model, p))
		{
			toModel = MMult(toModel, MV1GetFrameBaseLocalMatrix(model, p));
		}

		const MATRIX toLocal = MInverse(toModel);
		legAxisSide_[i] = NormalizeSafe(VTransformSR(VGet(1.0f, 0.0f, 0.0f), toLocal));
		legAxisFront_[i] = NormalizeSafe(VTransformSR(VGet(0.0f, 0.0f, 1.0f), toLocal));
	}
}

// 形態を切り替える(モデル・アニメ・フレームを作り直す)
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
		((ins.IsNew(KEY_INPUT_H) &&	// H を押しながら G
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

	// 脚のポーズの調整(Debug ビルドのみ)
	UpdateLegTune();

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

	// 浮遊の見た目(傾き・上下ゆれ)
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

		// 減衰も、fps が変わっても同じ減り方になるようにする
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

void Player::UpdateAfterImages(void)
{
	// 攻撃の残像
	if (isAfterImage_)
	{
		afterImageTimer_ -= scnMng_.GetDeltaTime();

		if (afterImageTimer_ <= 0.0f)
		{
			afterImageTimer_ = 0.0f;
			isAfterImage_ = false;
		}
	}

	// 高速移動の残像
	for (auto it = boostAfterImages_.begin(); it != boostAfterImages_.end();)
	{
		it->timer -= scnMng_.GetDeltaTime();

		// 多すぎるときは古いものから消す
		const bool tooMany = (boostAfterImages_.size() > 6);

		if (it->timer <= 0.0f || tooMany)
		{
			it = boostAfterImages_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

// 浮遊の見た目: 移動の向きに体を傾ける
//   当たり判定は動かさず、描画の行列だけ傾ける
void Player::UpdateFloatLean(void)
{
	const float dt = scnMng_.GetDeltaTime();
	floatBobTime_ += cfg::Float::BOB_SPEED * dt;

	float targetPitch = 0.0f;
	float targetRoll = 0.0f;

	// 自分で動いているときだけ傾ける
	if (IsFloatSelfMove())
	{
		const VECTOR forward = GetForward();
		const VECTOR right = VGet(forward.z, 0.0f, -forward.x);	// 右

		const float f = VDot(movePow_, forward);	// 前がプラス、後ろがマイナス
		const float s = VDot(movePow_, right);		// 右がプラス

		targetPitch = f * cfg::Float::LEAN_PER_SPEED - movePow_.y * cfg::Float::VERTICAL_LEAN;
		targetRoll = -s * cfg::Float::LEAN_PER_SPEED * cfg::Float::ROLL_SIGN;

		targetPitch = std::clamp(targetPitch, -cfg::Float::MAX_PITCH, cfg::Float::MAX_PITCH);
		targetRoll = std::clamp(targetRoll, -cfg::Float::MAX_ROLL, cfg::Float::MAX_ROLL);

		// 脚のポーズの倍率で、傾きも調整する
		targetPitch *= legTune_[TUNE_LEAN];
		targetRoll *= legTune_[TUNE_LEAN];
	}

	// 回避ステップ中は、進む向きへ大きく倒す
	float smooth = cfg::Float::LEAN_SMOOTH;

	if (isDodge_)
	{
		const VECTOR forward = GetForward();
		const VECTOR right = VGet(forward.z, 0.0f, -forward.x);

		const float f = VDot(dodgeDir_, forward);
		const float s = VDot(dodgeDir_, right);

		const float lean = cfg::Step::LEAN;

		targetPitch = f * lean;
		targetRoll = -s * lean * cfg::Float::ROLL_SIGN;

		smooth = cfg::Step::LEAN_SMOOTH;
	}

	// 目標の傾きへ少しずつ近づける
	float k = smooth * dt;
	if (k > 1.0f) { k = 1.0f; }

	floatPitch_ += (targetPitch - floatPitch_) * k;
	floatRoll_ += (targetRoll - floatRoll_) * k;
}

// 自分で移動している状態か(攻撃・回避などではない)
bool Player::IsFloatSelfMove(void) const
{
	return
		!isDamage_ &&
		!attack_.IsAttack() && !attack_.IsChasing() &&
		!boostChase_.IsActive() && !isDodge_ &&
		!kamehame_.IsActive() && !guard_.IsBusy() &&
		!formChange_.IsActive() &&
		!isCharging_ && !isChargeEnding_ &&
		!isKiBlast_;
}

// 脚のポーズの調整モード(Debug ビルドのみ)
//   F1: 開く/閉じる  F2/F3: 項目  F5/F6: 値  LSHIFT: 細かく
//   決まった値は画面に出るので、PlayerConfig.h の LegPose に書き写す
namespace
{
	const char* kLegTuneName[] =
	{
		"L_THIGH   左太もも(+前 -後)",
		"L_KNEE    左ひざ(曲げ)",
		"L_OPEN    左脚の開き",
		"R_THIGH   右太もも(+前 -後)",
		"R_KNEE    右ひざ(曲げ)",
		"R_OPEN    右脚の開き",
		"FOOT      足首",
		"LEAN_RATE 体の傾きの倍率",
	};
}

void Player::UpdateLegTune(void)
{
#ifdef _DEBUG
	auto& ins = InputManager::GetInstance();

	if (ins.IsTrgDown(KEY_INPUT_F1))
	{
		isLegTune_ = !isLegTune_;
	}

	if (!isLegTune_)
	{
		return;
	}

	if (ins.IsTrgDown(KEY_INPUT_F2)) { legTuneSel_ = (legTuneSel_ + TUNE_MAX - 1) % TUNE_MAX; }
	if (ins.IsTrgDown(KEY_INPUT_F3)) { legTuneSel_ = (legTuneSel_ + 1) % TUNE_MAX; }

	const bool fine = ins.IsNew(KEY_INPUT_LSHIFT);
	float step = fine ? 1.0f : 5.0f;
	if (legTuneSel_ == TUNE_LEAN) { step = fine ? 0.05f : 0.25f; }

	if (ins.IsTrgDown(KEY_INPUT_F5)) { legTune_[legTuneSel_] -= step; }
	if (ins.IsTrgDown(KEY_INPUT_F6)) { legTune_[legTuneSel_] += step; }

	if (legTune_[TUNE_LEAN] < 0.0f) { legTune_[TUNE_LEAN] = 0.0f; }
#endif
}

void Player::DrawLegTune(void) const
{
#ifdef _DEBUG
	if (!isLegTune_)
	{
		return;
	}

	const int x = 20;
	int y = 20;

	DrawString(x, y, "[脚の調整] F2/F3:項目  F5/F6:値(-/+)  LSHIFT:細かく  F1:閉じる", GetColor(255, 255, 0));
	y += 22;

	for (int i = 0; i < TUNE_MAX; i++)
	{
		const unsigned int color = (i == legTuneSel_) ? GetColor(255, 120, 60) : GetColor(255, 255, 255);
		DrawFormatString(x, y, color, "%s %-26s : %.2f", (i == legTuneSel_) ? ">" : " ", kLegTuneName[i], legTune_[i]);
		y += 20;
	}
#endif
}

void Player::CancelRootMotion(void)
{
	if (hipsFrame_ < 0) { return; }

	// 移動のアニメだけ、腰の水平移動を打ち消す(それ以外は元に戻す)
	switch ((ANIM_TYPE)animationController_->GetPlayType())
	{
	case ANIM_TYPE::RUN:
	case ANIM_TYPE::FAST_RUN:
	case ANIM_TYPE::LOCK_LEFT:
	case ANIM_TYPE::LOCK_RIGHT:
	case ANIM_TYPE::LOCK_LEFT_RUN:
	case ANIM_TYPE::LOCK_RIGHT_RUN:
	case ANIM_TYPE::LOCK_BACK:
	case ANIM_TYPE::LOCK_BACK_RUN:
		break;
	default:
		MV1ResetFrameUserLocalMatrix(transform_.modelId, hipsFrame_);
		return;
	}

	const int model = transform_.modelId;

	MV1ResetFrameUserLocalMatrix(model, hipsFrame_);
	MATRIX m = MV1GetFrameLocalMatrix(model, hipsFrame_);
	const MATRIX base = MV1GetFrameBaseLocalMatrix(model, hipsFrame_);

	// 水平(X・Z)だけ、元の位置に戻す
	m.m[3][0] = base.m[3][0];
	m.m[3][2] = base.m[3][2];

	MV1SetFrameUserLocalMatrix(model, hipsFrame_, m);
}

// 浮遊移動中の脚のポーズ
//   移動アニメの上から、脚のボーンだけポーズへ寄せる
//   左脚: 太ももを前へ上げて膝を曲げる / 右脚: 少し開いて軽く曲げて垂らす
void Player::UpdateLegPose(void)
{
	namespace lp = cfg::LegPose;

	const float dt = scnMng_.GetDeltaTime();
	const int model = transform_.modelId;

	// 動いているか
	const bool moving =
		VSize(ToHorizontal(movePow_)) > Constants::Epsilon ||
		fabsf(movePow_.y) > Constants::Epsilon;

	const float target = (IsFloatSelfMove() && (moving || lp::IN_IDLE)) ? 1.0f : 0.0f;

	float k = lp::BLEND_SPEED * dt;
	if (k > 1.0f) { k = 1.0f; }
	legPoseRate_ += (target - legPoseRate_) * k;

	// ほぼポーズなしなら、アニメのままにする
	if (legPoseRate_ < 0.01f)
	{
		legPoseRate_ = 0.0f;

		for (int frame : legFrames_)
		{
			if (frame >= 0)
			{
				MV1ResetFrameUserLocalMatrix(model, frame);
			}
		}
		return;
	}

	// 横移動の量(脚を進行方向と逆へ流す)
	const VECTOR forward = GetForward();
	const VECTOR right = VGet(forward.z, 0.0f, -forward.x);
	float side = VDot(movePow_, right) / SPEED_RUN;
	side = std::clamp(side, -1.0f, 1.0f);

	const float sway = sinf(floatBobTime_ * 0.7f) * lp::SWAY;
	const float spread = -side * lp::SIDE_SPREAD * lp::SIDE_SIGN;

	// アニメの姿勢とポーズを、legPoseRate_ で混ぜる
	auto apply = [&](int frame, const MATRIX& rot)
		{
			if (frame < 0)
			{
				return;
			}

			// アニメの姿勢
			MV1ResetFrameUserLocalMatrix(model, frame);
			const MATRIX anim = MV1GetFrameLocalMatrix(model, frame);

			// ポーズ: 元の姿勢から回したもの
			const MATRIX base = MV1GetFrameBaseLocalMatrix(model, frame);
			const MATRIX pose = MMult(rot, base);

			MV1SetFrameUserLocalMatrix(model, frame, BlendMatrix(anim, pose, legPoseRate_));
		};

	auto rad = [](float deg) { return AsoUtility::Deg2RadF(deg); };

	// swing: 脚を前後に振る(キャラの左右の軸で回す)
	// spreadRot: 脚を横に開く(キャラの前後の軸で回す)
	const float fs = lp::FORWARD_SIGN;
	auto swing = [&](int leg, float deg)
		{
			return MGetRotAxis(legAxisSide_[leg], rad(deg) * fs);
		};
	auto spreadRot = [&](int leg, float deg)
		{
			return MGetRotAxis(legAxisFront_[leg], rad(deg));
		};

	// 左脚
	//   太ももを前へ上げ、膝を深く曲げる
	const float openL = legTune_[TUNE_L_OPEN] * lp::OPEN_SIGN;
	const float openR = -legTune_[TUNE_R_OPEN] * lp::OPEN_SIGN;

	apply(legFrames_[LEG_L_UP], MMult(swing(LEG_L_UP, legTune_[TUNE_L_THIGH] + sway), spreadRot(LEG_L_UP, spread + openL)));
	apply(legFrames_[LEG_L_KNEE], swing(LEG_L_KNEE, -(legTune_[TUNE_L_KNEE] - sway)));
	apply(legFrames_[LEG_L_FOOT], swing(LEG_L_FOOT, -legTune_[TUNE_FOOT]));

	// 右脚
	apply(legFrames_[LEG_R_UP], MMult(swing(LEG_R_UP, legTune_[TUNE_R_THIGH] - sway), spreadRot(LEG_R_UP, spread + openR)));
	apply(legFrames_[LEG_R_KNEE], swing(LEG_R_KNEE, -(legTune_[TUNE_R_KNEE] + sway)));
	apply(legFrames_[LEG_R_FOOT], swing(LEG_R_FOOT, -legTune_[TUNE_FOOT]));
}

// 見た目の向き・位置を transform_ に反映する
//   当たり判定用の位置と向きは変えずに、描画の行列だけ傾けたり揺らしたりする
void Player::ApplyVisualTransform(const Quaternion& logicRot)
{
	// 気溜め・変身中は、見た目だけ体の向きを回す
	Quaternion baseRot = logicRot;
	if (isCharging_ || isChargeEnding_ || formChange_.IsActive())
	{
		baseRot = logicRot.Mult(Quaternion::Euler(
			{ 0.0f, AsoUtility::Deg2RadF(cfg::Charge::VISUAL_YAW_DEG), 0.0f }));
	}
	else if (heatActivate_ > 0.0f)
	{
		// ヒートアップ発動の演出中も、見た目だけ体の向きを回す
		baseRot = logicRot.Mult(Quaternion::Euler(
			{ 0.0f, AsoUtility::Deg2RadF(cfg::HeatUp::ACTIVATE_YAW_DEG), 0.0f }));
	}

	const Quaternion visualRot =
		baseRot.Mult(Quaternion::Euler({ floatPitch_, 0.0f, floatRoll_ }));

	// 傾ける中心(体の真ん中)がずれないように位置を補正し、上下にゆらす
	const VECTOR pivot = { 0.0f, cfg::Float::PIVOT_HEIGHT, 0.0f };
	VECTOR offset = VSub(pivot, visualRot.PosAxis(pivot));
	offset.y += sinf(floatBobTime_) * cfg::Float::BOB_AMPLITUDE;

	const VECTOR logicPos = transform_.pos;

	transform_.pos = VAdd(logicPos, offset);
	transform_.quaRot = visualRot;
	transform_.Update();		// 描画用の行列を作る

	transform_.pos = logicPos;
	transform_.quaRot = logicRot;
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

	// ガード・ガードブレイク・バースト中は動けない
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

void Player::Draw(void)
{
	// 攻撃の残像
	if (isAfterImage_)
	{
		DrawAfterImage(afterImageMatrix_, 0.35f, false);
	}

	// 高速移動の残像
	for (auto& image : boostAfterImages_)
	{
		DrawAfterImage(image.matrix, image.timer / Constants::BoostAfterImageDuration * 0.5f, true);
	}

	// ヒートアップ中は、体を金色っぽくする
	{
		const bool beforeGold = IsBeforeGold();

		float g = 1.0f;
		float b = 1.0f;

		if (heatUp_ && !beforeGold)
		{
			const float pulse = 0.5f + 0.5f * sinf((float)GetNowCount() * 0.001f * 8.0f);
			g = 0.88f + 0.06f * pulse;
			b = 0.55f + 0.15f * pulse;
		}

		MV1SetDifColorScale(transform_.modelId, GetColorF(1.0f, g, b, 1.0f));
	}

	// 本体
	SetUseLighting(false);
	MV1DrawModel(transform_.modelId);
	SetUseLighting(true);

	DrawHeatAura();
	DrawKiBlast();
	DrawKamehame();

	DrawLegTune();
}

float Player::GetHeatActivateElapsed(void) const
{
	if (heatActivate_ <= 0.0f)
	{
		return -1.0f;
	}

	return cfg::HeatUp::ACTIVATE_TIME - heatActivate_;
}

// ヒートアップ発動の演出中で、まだ金色になる前(光がはじけ終わる前)か
bool Player::IsBeforeGold(void) const
{
	return heatActivate_ > 0.0f &&
		GetHeatActivateElapsed() < cfg::HeatUp::ACTIVATE_BURST_END;
}

void Player::DrawHud(void)
{
	hud_.Draw(*this);
}

void Player::DrawAfterImage(const MATRIX& matrix, float opacity, bool disableLighting)
{
	MATRIX playerMatrix = MV1GetMatrix(transform_.modelId);

	MV1SetMatrix(transform_.modelId, matrix);
	MV1SetOpacityRate(transform_.modelId, opacity);

	if (disableLighting) { SetUseLighting(false); }
	MV1DrawModel(transform_.modelId);
	if (disableLighting) { SetUseLighting(true); }

	MV1SetMatrix(transform_.modelId, playerMatrix);
	MV1SetOpacityRate(transform_.modelId, 1.0f);
}

void Player::DrawKiBlast(void)
{
	barrage_.Draw();

	for (auto& blast : kiBlasts_)
	{
		blast->Draw();
	}

	for (auto& blast : finishBlasts_)
	{
		blast->Draw();
	}
}

void Player::DrawShadow(void)
{
	const float SHADOW_HEIGHT = Constants::PlayerShadowHeight;
	const float SHADOW_SIZE = Constants::PlayerShadowSize;
	const float SHADOW_MAX_ALPHA = 128.0f;

	// 高いところほど薄くする
	auto calcAlpha = [&](float y) -> int
		{
			if (y <= transform_.pos.y - SHADOW_HEIGHT)
			{
				return 0;
			}
			float rate = 1.0f - fabsf(y - transform_.pos.y) / SHADOW_HEIGHT;
			return static_cast<int>(roundf(SHADOW_MAX_ALPHA * rate));
		};

	SetUseLighting(FALSE);
	SetUseZBuffer3D(TRUE);

	// テクスチャの外は伸ばす
	SetTextureAddressMode(DX_TEXADDRESS_CLAMP);

	// 影のポリゴン
	VERTEX3D vertex[3] = { VERTEX3D(), VERTEX3D(), VERTEX3D() };
	vertex[0].dif = GetColorU8(255, 255, 255, 255);
	vertex[0].spc = GetColorU8(0, 0, 0, 0);
	vertex[0].su = 0.0f;
	vertex[0].sv = 0.0f;
	vertex[1] = vertex[0];
	vertex[2] = vertex[0];

	for (const auto& c : colliders_)
	{
		auto collider = c.lock();
		if (!collider)
		{
			continue;
		}

		// 足元の地面のポリゴンを探す
		MV1_COLL_RESULT_POLY_DIM hitDim = MV1CollCheck_Capsule(
			collider->modelId_, -1,
			transform_.pos,
			VAdd(transform_.pos, { 0.0f, -SHADOW_HEIGHT, 0.0f }),
			SHADOW_SIZE);

		for (int i = 0; i < hitDim.HitNum; i++)
		{
			const MV1_COLL_RESULT_POLY& poly = hitDim.Dim[i];

			// 地面に埋もれないように少し浮かせる
			VECTOR slide = VScale(poly.Normal, 0.5f);

			for (int v = 0; v < 3; v++)
			{
				vertex[v].pos = VAdd(poly.Position[v], slide);
				vertex[v].dif.a = calcAlpha(poly.Position[v].y);

				// プレイヤーの位置を中心にテクスチャを貼る
				vertex[v].u = (poly.Position[v].x - transform_.pos.x) / (SHADOW_SIZE * 2.0f) + 0.5f;
				vertex[v].v = (poly.Position[v].z - transform_.pos.z) / (SHADOW_SIZE * 2.0f) + 0.5f;
			}

			DrawPolygon3D(vertex, 1, imgShadow_, TRUE);
		}

		MV1CollResultPolyDimTerminate(hitDim);
	}

	SetUseLighting(TRUE);
	SetUseZBuffer3D(FALSE);
}

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

void Player::LeaveAfterImage(void)
{
	afterImageMatrix_ = MV1GetMatrix(transform_.modelId);
	isAfterImage_ = true;
	afterImageTimer_ = Constants::AfterImageDuration;
}

// 高速移動の残像を1枚残す
void Player::AddBoostAfterImage(void)
{
	BoostAfterImage image{};
	image.matrix = MV1GetMatrix(transform_.modelId);
	image.timer = Constants::BoostAfterImageDuration;
	boostAfterImages_.push_back(image);
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

void Player::DrawKamehame(void)
{
	kamehame_.Draw(MakeKamehameContext());
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

// ヒートアップのオーラ
void Player::DrawHeatAura(void) const
{
	if (!heatUp_ && heatActivate_ <= 0.0f)
	{
		return;
	}

	const float t = (float)GetNowCount() * 0.001f;
	const float pulse = 0.5f + 0.5f * sinf(t * 14.0f);
	const VECTOR base = transform_.pos;

	SetUseLighting(FALSE);

	// オーラで体が隠れないように、Zバッファに書かない
	SetWriteZBuffer3D(FALSE);

	// 発動の演出: 光の柱 → 光がはじける
	if (heatActivate_ > 0.0f)
	{
		const float t = cfg::HeatUp::ACTIVATE_TIME - heatActivate_;

		if (t < cfg::HeatUp::ACTIVATE_FLASH)
		{
			const float rate = t / cfg::HeatUp::ACTIVATE_FLASH;
			const VECTOR top = VAdd(base, VGet(0.0f, 120.0f + 380.0f * rate, 0.0f));

			SetDrawBlendMode(DX_BLENDMODE_ADD, 70 + (int)(60.0f * pulse));
			DrawCapsule3D(VAdd(base, VGet(0.0f, -20.0f, 0.0f)), top, 34.0f + 10.0f * rate, 12,
				GetColor(110, 170, 255), GetColor(110, 170, 255), TRUE);

			SetDrawBlendMode(DX_BLENDMODE_ADD, 120);
			DrawCapsule3D(VAdd(base, VGet(0.0f, -20.0f, 0.0f)), top, 14.0f, 8,
				GetColor(230, 245, 255), GetColor(230, 245, 255), TRUE);

			// 立ち昇る光の粒
			SetDrawBlendMode(DX_BLENDMODE_ADD, 200);

			for (int i = 0; i < 14; i++)
			{
				const float up = fmodf(t * 2.2f + i * 0.31f, 1.0f);
				const float ang = i * 0.9f + t * 3.0f;
				const VECTOR p = VAdd(base, VGet(cosf(ang) * 40.0f, up * 300.0f, sinf(ang) * 40.0f));

				DrawSphere3D(p, 1.5f + 4.0f * (1.0f - up), 6, GetColor(170, 215, 255), GetColor(170, 215, 255), TRUE);
			}
		}
		else if (t < cfg::HeatUp::ACTIVATE_BURST_END)
		{
			const float u = (t - cfg::HeatUp::ACTIVATE_FLASH) / (cfg::HeatUp::ACTIVATE_BURST_END - cfg::HeatUp::ACTIVATE_FLASH);
			const float fade = 1.0f - 0.85f * u;

			SetDrawBlendMode(DX_BLENDMODE_ADD, (int)(170.0f * fade));
			DrawSphere3D(VAdd(base, VGet(0.0f, 80.0f, 0.0f)), 50.0f + 300.0f * u, 24,
				GetColor(190, 225, 255), GetColor(190, 225, 255), TRUE);

			SetDrawBlendMode(DX_BLENDMODE_ADD, (int)(200.0f * fade));
			DrawSphere3D(VAdd(base, VGet(0.0f, 80.0f, 0.0f)), 30.0f + 120.0f * u, 20,
				GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);
		}
	}

	// 金色になる前は、ここまで
	const bool beforeGold = IsBeforeGold();

	if (!heatUp_ || beforeGold)
	{
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetWriteZBuffer3D(TRUE);
		SetUseLighting(TRUE);
		return;
	}

	// 金色のオーラ
	SetDrawBlendMode(DX_BLENDMODE_ADD, 22 + (int)(14.0f * pulse));
	DrawCapsule3D(VAdd(base, VGet(0.0f, 30.0f, 0.0f)), VAdd(base, VGet(0.0f, 130.0f, 0.0f)),
		46.0f + 6.0f * pulse, 12, GetColor(255, 200, 60), GetColor(255, 200, 60), TRUE);

	SetDrawBlendMode(DX_BLENDMODE_ADD, 10);
	DrawCapsule3D(VAdd(base, VGet(0.0f, 20.0f, 0.0f)), VAdd(base, VGet(0.0f, 150.0f, 0.0f)),
		70.0f + 10.0f * pulse, 12, GetColor(255, 235, 140), GetColor(255, 235, 140), TRUE);

	// 炎のようなゆらめき
	constexpr int   kFlameCount = 8;
	constexpr float kFlameRadius = 52.0f;	// 体からの距離
	constexpr float kFlameWidth = 14.0f;	// 炎の太さ
	constexpr int   kFlameAlpha = 70;

	for (int i = 0; i < kFlameCount; i++)
	{
		const float ang = (float)i / (float)kFlameCount * DX_TWO_PI_F + t * 1.5f;
		const float flicker = 0.5f + 0.5f * sinf(t * 11.0f + (float)i * 1.7f);
		const float height = 110.0f + 90.0f * flicker;

		const VECTOR root = VAdd(base, VGet(cosf(ang) * kFlameRadius, 10.0f, sinf(ang) * kFlameRadius));
		const VECTOR tip = VAdd(root, VGet(-cosf(ang) * 14.0f, height, -sinf(ang) * 14.0f));

		const unsigned int color = (i % 2 == 0) ? GetColor(255, 200, 60) : GetColor(255, 235, 140);

		SetDrawBlendMode(DX_BLENDMODE_ADD, kFlameAlpha);
		DrawCone3D(tip, root, kFlameWidth, 8, color, color, TRUE);
	}

	// 立ち昇る光の粒
	SetDrawBlendMode(DX_BLENDMODE_ADD, 200);

	for (int i = 0; i < 16; i++)
	{
		const float up = fmodf(t * 1.8f + i * 0.37f, 1.0f);
		const float ang = i * 0.8f + t * 2.0f;
		const VECTOR p = VAdd(base, VGet(cosf(ang) * 55.0f, 20.0f + up * 170.0f, sinf(ang) * 55.0f));

		DrawSphere3D(p, 1.0f + 5.0f * (1.0f - up), 6, GetColor(255, 230, 120), GetColor(255, 230, 120), TRUE);
	}

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	SetWriteZBuffer3D(TRUE);
	SetUseLighting(TRUE);
}

bool Player::ConsumeVanish(void)
{
	const bool result = vanishPending_;
	vanishPending_ = false;
	return result;
}

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

	if (vanishCalmTimer_ > 0.0f)
	{
		vanishCalmTimer_ -= ctx.deltaTime;
	}

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