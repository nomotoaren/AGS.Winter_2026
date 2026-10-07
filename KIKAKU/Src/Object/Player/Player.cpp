#include <string>
#include <cmath>
#include <cfloat>
#include <algorithm>
#include <filesystem>
#include "../../Application.h"
#include "../../Utility/AsoUtility.h"
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

// 定数・ヘルパー(このcppの中だけで使う)
// 調整用の数字は PlayerConfig.h にまとめてある
namespace
{
	// 形態ごとのデータ(model, animDir, scale, speedRate, attackRate)
	// ※ Player::FORM / Player::FormData は public にしておくこと
	const Player::FormData kFormData[(int)Player::FORM::MAX] =
	{
		{ ResourceManager::SRC::PLAYER,   "Player/Animation/", 1.5f, 1.0f, 1.0f },
		{ ResourceManager::SRC::EVPLAY_,  "Player/AnimationEV/", 0.8f, 0.8f, 2.0f },
	};

	// WASDのどれかが押されているか
	bool IsMoveKeyDown(InputManager& ins)
	{
		return ins.IsNew(KEY_INPUT_W) ||
			ins.IsNew(KEY_INPUT_A) ||
			ins.IsNew(KEY_INPUT_S) ||
			ins.IsNew(KEY_INPUT_D) ||
			PadInput::IsStickTilted();
	}

	// 2つの姿勢行列の間を t(0~1) で補間する
	//   要素ごとに補間したあと、回転部分を直交化して形が崩れないようにする
	MATRIX BlendMatrix(const MATRIX& a, const MATRIX& b, float t)
	{
		MATRIX r;
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				r.m[i][j] = a.m[i][j] + (b.m[i][j] - a.m[i][j]) * t;
			}
		}

		auto row = [](const MATRIX& m, int i) { return VGet(m.m[i][0], m.m[i][1], m.m[i][2]); };

		// 各軸の長さ(スケール)も補間して保つ
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

	// Mixamo のボーンを名前で探す(接頭辞がモデルによって違うので順に試す)
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

	speed_(0.0f),
	moveDir_(AsoUtility::VECTOR_ZERO),
	movePow_(AsoUtility::VECTOR_ZERO),
	movedPos_(AsoUtility::VECTOR_ZERO),
	knockBackPow_(AsoUtility::VECTOR_ZERO),
	isDamage_(false),
	damageTimer_(0.0f),

	playerRotY_(Quaternion()),
	goalQuaRot_(Quaternion()),
	stepRotTime_(0.0f),

	jumpPow_(AsoUtility::VECTOR_ZERO),
	isJump_(false),
	stepJump_(0.0f),

	// 衝突チェック
	gravHitPosDown_(AsoUtility::VECTOR_ZERO),
	gravHitPosUp_(AsoUtility::VECTOR_ZERO),

	imgShadow_(-1),
	recordTime_(0.0f),
	isRecording_(false),

	// 攻撃関連
	hasAttackTarget_(false),
	attackTargetPos_(AsoUtility::VECTOR_ZERO),
	canChase_(false),
	afterImageModel_(-1),
	isAfterImage_(false),
	afterImageTimer_(0.0f),
	afterImagePos_(AsoUtility::VECTOR_ZERO),
	afterImageMatrix_({}),

	// 気を溜める
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

	// 飛行
	isFlying_(false),

	// 残像
	afterImageAttachNo_(-1),

	hp_(100),
	isDead_(false),

	attackEndTimer_(0.0f),
	rightHandFrame_(-1),

	capsule_(nullptr),


	// 変身
	form_(FORM::BASE),
	nextForm_(FORM::BASE)
{
	// 状態管理
	stateChanges_.emplace(STATE::NONE, std::bind(&Player::ChangeStateNone, this));
	stateChanges_.emplace(STATE::PLAY, std::bind(&Player::ChangeStatePlay, this));
}

Player::~Player(void)
{
}

void Player::Init(void)
{
	// モデルの基本設定
	transform_.SetModel(resMng_.LoadModelDuplicate(ResourceManager::SRC::PLAYER));

	afterImageModel_ = MV1DuplicateModel(transform_.modelId);

	transform_.scl = { 1.5f, 1.5f, 1.5f };
	transform_.pos = { 0.0f, 1000.0f, 0.0f };
	transform_.quaRot = Quaternion();
	transform_.quaRotLocal =
		Quaternion::Euler({ 0.0f, AsoUtility::Deg2RadF(180.0f), 0.0f });
	transform_.Update();

	// アニメーションの設定
	InitAnimation(kFormData[(int)FORM::BASE].animDir);

	// 手のフレーム(かめはめ波・気弾の発射位置用)
	InitFrames();

	// かめはめ波(気弾・ビームのモデルとライト)
	kamehame_.Init(transform_.pos);

	// カプセルコライダ
	capsule_ = std::make_unique<Capsule>(transform_);
	capsule_->SetLocalPosTop({ 0.0f, 110.0f, 0.0f });
	capsule_->SetLocalPosDown({ 0.0f, 30.0f, 0.0f });
	capsule_->SetRadius(20.0f);

	// 丸影画像
	imgShadow_ = resMng_.Load(ResourceManager::SRC::PLAYER_SHADOW).handleId_;

	// 変身後のモデル・アニメを先に読み込んでおく(変身の瞬間にカクつくのを防ぐ)
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

	// 形態のフォルダにファイルがあればそれを使い、なければ通常形態のものを使う
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
	add(ANIM_TYPE::RUN, "Walk.mv1", 40.0f);
	add(ANIM_TYPE::FAST_RUN, "Running.mv1", 40.0f);
	add(ANIM_TYPE::LOCK_LEFT, "LSWalking.mv1", 60.0f);
	add(ANIM_TYPE::LOCK_RIGHT, "RSWalking.mv1", 40.0f);
	add(ANIM_TYPE::LOCK_LEFT_RUN, "Left Strafe.mv1", 40.0f);
	add(ANIM_TYPE::LOCK_RIGHT_RUN, "Right Strafe.mv1", 40.0f);
	add(ANIM_TYPE::LOCK_BACK, "Walking Back.mv1", 40.0f);
	add(ANIM_TYPE::LOCK_BACK_RUN, "Running Back.mv1", 40.0f);
	add(ANIM_TYPE::BOOST_CHASE, "Flying.mv1", 40.0f);

	// 攻撃
	add(ANIM_TYPE::ATTACK01, "Attack02.mv1", 60.0f);
	add(ANIM_TYPE::ATTACK02, "Attack01.mv1", 60.0f);
	add(ANIM_TYPE::ATTACK03, "Attack03.mv1", 80.0f);
	add(ANIM_TYPE::ATTACK04, "Attack04.mv1", 80.0f);
	add(ANIM_TYPE::ATTACK05, "Attack05.mv1", 85.0f);
	add(ANIM_TYPE::ATTACK06, "Attack01.mv1", 80.0f);
	add(ANIM_TYPE::ATTACK07, "Attack07.mv1", 65.0f);
	add(ANIM_TYPE::ATTACK08, "Attack08.mv1", 65.0f);

	// 気功
	add(ANIM_TYPE::KI_BLAST, "KiBlast.mv1", 60.0f);
	add(ANIM_TYPE::KAMEHAME, "Special.mv1", 20.0f);
	add(ANIM_TYPE::CHARGE, "pawer.mv1", 40.0f);

	// 被弾・ガード
	add(ANIM_TYPE::DAMAGE, "Damege.mv1", 60.0f);
	add(ANIM_TYPE::GUARD, "Block.mv1", 60.0f);
	add(ANIM_TYPE::GUARD_BURST, "pawer.mv1", 100.0f);
	add(ANIM_TYPE::GUARD_BREAK, "GuardBreak.mv1", 20.0f);

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

// 手のフレームを探す(モデルを差し替えたら再取得が必要)
void Player::InitFrames(void)
{
	kamehame_.SetModel(transform_.modelId);
	rightHandFrame_ = MV1SearchFrame(transform_.modelId, "mixamorig:RightHand");

	InitLegFrames();
}

// 脚のボーンを探す(モデルを差し替えたら再取得が必要)
void Player::InitLegFrames(void)
{
	const int model = transform_.modelId;

	legFrames_[LEG_L_UP] = FindMixamoFrame(model, "LeftUpLeg");
	legFrames_[LEG_L_KNEE] = FindMixamoFrame(model, "LeftLeg");
	legFrames_[LEG_L_FOOT] = FindMixamoFrame(model, "LeftFoot");
	legFrames_[LEG_R_UP] = FindMixamoFrame(model, "RightUpLeg");
	legFrames_[LEG_R_KNEE] = FindMixamoFrame(model, "RightLeg");
	legFrames_[LEG_R_FOOT] = FindMixamoFrame(model, "RightFoot");

	// ボーンごとに「キャラの左右軸・前後軸」を、そのボーン自身の座標系に直しておく
	//   ボーンの向き(Mixamo の回転軸)はモデルによってバラバラなので、
	//   初期姿勢での親子の行列をたどって、モデル空間の軸をボーンの中の軸に変換する
	for (int i = 0; i < LEG_MAX; i++)
	{
		const int frame = legFrames_[i];
		legAxisSide_[i] = VGet(1.0f, 0.0f, 0.0f);
		legAxisFront_[i] = VGet(0.0f, 0.0f, 1.0f);

		if (frame < 0)
		{
			continue;
		}

		// このボーンの初期姿勢での「モデル空間での向き」
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

// 形態を適用する(モデル・アニメ・スケール・残像用モデルを作り直す)
void Player::ApplyForm(FORM form)
{
	const FormData& data = kFormData[(int)form];

	// 1. アニメ管理は古いモデルが生きているうちに破棄する
	animationController_.reset();

	const int oldModel = transform_.modelId;

	// 2. 新しいモデルに差し替え
	const int newModel = resMng_.LoadModelDuplicate(data.model);
	transform_.SetModel(newModel);
	transform_.scl = { data.scale, data.scale, data.scale };
	transform_.quaRotLocal =
		Quaternion::Euler({ 0.0f, AsoUtility::Deg2RadF(180.0f), 0.0f });
	transform_.Update();

	// 3. アニメーション・手のフレームを作り直す
	InitAnimation(data.animDir);
	InitFrames();

	// 4. 残像用モデルを作り直す
	if (afterImageModel_ != -1)
	{
		MV1DeleteModel(afterImageModel_);
	}
	afterImageModel_ = MV1DuplicateModel(newModel);

	// 古いモデルの行列で作った残像は使えないので消す
	boostAfterImages_.clear();
	isAfterImage_ = false;

	// 5. 古いモデルを削除(これを先にやると変身後のアニメが再生されなかったので最後)
	MV1DeleteModel(oldModel);

	form_ = form;
}

// 変身(Gキー)
//   流れは PlayerTransform に任せ、モデル差し替えとアニメ再生だけここで行う
void Player::UpdateTransform(void)
{
	auto& ins = InputManager::GetInstance();

	// 開始
	if (!formChange_.IsActive() &&
		!attack_.IsAttack() &&
		!kamehame_.IsActive() &&
		!isKiBlast_ &&
		!isCharging_ &&
		!isChargeEnding_ &&
		!boostChase_.IsActive() &&
		((ins.IsNew(KEY_INPUT_H) &&	// フォームチェンジパレット(H)を開いている間だけ変身できる
			ins.IsTrgDown(KEY_INPUT_G)) ||
			PadInput::IsTransformTrg()))	// パッド: L2+R2 を押しながら □
	{
		const FORM next = (form_ == FORM::BASE) ? FORM::SUPER : FORM::BASE;

		// 変身は ki を消費、元に戻るときは消費しない
		if (next == FORM::BASE || UseKi(cfg::Transform::KI_COST))
		{
			nextForm_ = next;

			movePow_ = AsoUtility::VECTOR_ZERO;

			// 溜めモーションを最初から最後まで1回だけ流す(ループしない)
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

		// 差し替え後は、同じ再生位置から続きを流す(最初からにしない)
		animationController_->ClearEndLoop();
		animationController_->Play((int)ANIM_TYPE::CHARGE, false,
			result.step, cfg::Transform::ANIM_END_STEP, false, true);

		// 変身するときだけ、画面揺れを出す(元に戻るときは演出なし)
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
	return kFormData[(int)form_].attackRate;
}

int Player::GetAttackDamage(int baseDamage) const
{
	const int damage = static_cast<int>(std::lround(baseDamage * GetAttackRate()));
	return damage < 1 ? 1 : damage;
}

void Player::Update(void)
{
	// パッドの入力を更新(1フレームに1回)
	PadInput::Update();

	// HUDはダメージ中も更新する
	hud_.Update(*this);

	UpdateKnockBack();

	// ダメージ中は通常の更新をしない
	if (UpdateDamage())
	{
		return;
	}

	// 状態ごとの更新
	stateUpdate_();

	// 当たり判定用の向き(ロックオン中は上下の傾きも付ける)
	Quaternion logicRot = playerRotY_;
	if (isLockOn_ && hasAttackTarget_ && !attack_.IsAttack())
	{
		logicRot = playerRotY_.Mult(Quaternion::Euler({ lockOnPitch_, 0.0f, 0.0f }));
	}

	// 浮遊の傾き(movePow_ が決まったあとに計算する)
	UpdateFloatLean();
	ApplyVisualTransform(logicRot);

	animationController_->Update();

	// アニメを再生したあとで、脚だけ舞空術のポーズへ寄せる
	UpdateLegPose();

	UpdateAfterImages();
}

// 敵と重ならないように位置をずらす
void Player::PushOut(const VECTOR& offset)
{
	transform_.pos = VAdd(transform_.pos, offset);

	// transform_.Update() だと浮遊の見た目が一瞬消えるので、見た目込みで作り直す
	ApplyVisualTransform(transform_.quaRot);
}

void Player::UpdateKnockBack(void)
{
	if (VSize(knockBackPow_) > Constants::KnockBackThreshold)
	{
		transform_.pos = VAdd(transform_.pos, knockBackPow_);
		knockBackPow_ = VScale(knockBackPow_, Constants::KnockBackDamping);
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

	// 被弾中も上下ゆれは続ける(傾きは0へ戻っていく)
	UpdateFloatLean();
	ApplyVisualTransform(damageRot_);
	animationController_->Update();
	UpdateLegPose();					// 被弾中はポーズを解いていく
	return true;
}

void Player::UpdateAfterImages(void)
{
	// 攻撃時の残像
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

		if (it->timer <= 0.0f)
		{
			it = boostAfterImages_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

// 移動量から体の傾きを決める(見た目だけ)
//   前進で前傾、後退でのけぞり、横移動で横に傾く
void Player::UpdateFloatLean(void)
{
	const float dt = scnMng_.GetDeltaTime();
	floatBobTime_ += cfg::Float::BOB_SPEED * dt;

	float targetPitch = 0.0f;
	float targetRoll = 0.0f;

	// 自分で移動しているときだけ傾ける(攻撃・回避・高速接近などはモーションに任せる)
	if (IsFloatSelfMove())
	{
		const VECTOR forward = GetForward();
		const VECTOR right = VGet(forward.z, 0.0f, -forward.x);	// GetMoveBasis と同じ作り方

		const float f = VDot(movePow_, forward);	// 前がプラス、後ろがマイナス
		const float s = VDot(movePow_, right);		// 右がプラス

		targetPitch = f * cfg::Float::LEAN_PER_SPEED - movePow_.y * cfg::Float::VERTICAL_LEAN;
		targetRoll = -s * cfg::Float::LEAN_PER_SPEED * cfg::Float::ROLL_SIGN;

		targetPitch = std::clamp(targetPitch, -cfg::Float::MAX_PITCH, cfg::Float::MAX_PITCH);
		targetRoll = std::clamp(targetRoll, -cfg::Float::MAX_ROLL, cfg::Float::MAX_ROLL);
	}

	// (Windows.h の min マクロと衝突しないよう std::min は使わない)
	float k = cfg::Float::LEAN_SMOOTH * dt;
	if (k > 1.0f) { k = 1.0f; }

	floatPitch_ += (targetPitch - floatPitch_) * k;
	floatRoll_ += (targetRoll - floatRoll_) * k;
}

// 自分で移動している状態か(攻撃・回避・高速接近・気功などはそれぞれのモーションに任せる)
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

// 浮遊移動中の脚のポーズ
//   移動アニメ(Walk など)はそのまま再生し、その結果の上から脚のボーンだけを
//   「片膝を上げて、もう片方を後ろに曲げる」舞空術の姿勢へ寄せる
//   legPoseRate_ で少しずつ寄せるので、攻撃などに切り替わるときもカクッとしない
void Player::UpdateLegPose(void)
{
	namespace lp = cfg::LegPose;

	const float dt = scnMng_.GetDeltaTime();
	const int model = transform_.modelId;

	// 移動しているか(水平でも上下でも)
	const bool moving =
		VSize(ToHorizontal(movePow_)) > Constants::Epsilon ||
		fabsf(movePow_.y) > Constants::Epsilon;

	const float target = (IsFloatSelfMove() && (moving || lp::IN_IDLE)) ? 1.0f : 0.0f;

	float k = lp::BLEND_SPEED * dt;
	if (k > 1.0f) { k = 1.0f; }
	legPoseRate_ += (target - legPoseRate_) * k;

	// ほぼアニメのままなら、上書きを外して終わり
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

	// 横移動の量(-1~1)。右へ動くと脚は左へ流れる
	const VECTOR forward = GetForward();
	const VECTOR right = VGet(forward.z, 0.0f, -forward.x);
	float side = VDot(movePow_, right) / SPEED_RUN;
	side = std::clamp(side, -1.0f, 1.0f);

	const float sway = sinf(floatBobTime_ * 0.7f) * lp::SWAY;
	const float spread = -side * lp::SIDE_SPREAD * lp::SIDE_SIGN;

	// アニメの結果とポーズを rate で混ぜて、ボーンに設定する
	auto apply = [&](int frame, const MATRIX& rot)
		{
			if (frame < 0)
			{
				return;
			}

			// 一度上書きを外して、このフレームのアニメの姿勢を取り出す
			MV1ResetFrameUserLocalMatrix(model, frame);
			const MATRIX anim = MV1GetFrameLocalMatrix(model, frame);

			// ポーズは初期姿勢(脚がまっすぐ下)を基準に回して作る
			const MATRIX base = MV1GetFrameBaseLocalMatrix(model, frame);
			const MATRIX pose = MMult(rot, base);

			MV1SetFrameUserLocalMatrix(model, frame, BlendMatrix(anim, pose, legPoseRate_));
		};

	auto rad = [](float deg) { return AsoUtility::Deg2RadF(deg); };

	// キャラの左右軸まわりに振る回転(前へ振るのがプラス)と、前後軸まわりに開く回転
	//   ※ このモデルは素の状態で -Z 向き(Init で 180度回している)なので、
	//     左右軸(+X)まわりのプラス回転で脚が前(-Z)へ出る
	const float fs = lp::FORWARD_SIGN;
	auto swing = [&](int leg, float deg)
		{
			return MGetRotAxis(legAxisSide_[leg], rad(deg) * fs);
		};
	auto spreadRot = [&](int leg, float deg)
		{
			return MGetRotAxis(legAxisFront_[leg], rad(deg));
		};

	// 太もも:前へ上げる / 膝:すねを後ろへ折る(太ももと逆向き) / 足首:つま先を下へ伸ばす(逆向き)
	// 左脚(膝を曲げてすねを後ろへ)
	// 外へ開く角度(左脚はキャラの左=モデルの+X側へ、右脚は逆へ)
	const float openL = lp::LEG_OPEN * lp::OPEN_SIGN;
	const float openR = -lp::LEG_OPEN * lp::OPEN_SIGN;

	apply(legFrames_[LEG_L_UP], MMult(swing(LEG_L_UP, lp::L_THIGH + sway), spreadRot(LEG_L_UP, spread + openL)));
	apply(legFrames_[LEG_L_KNEE], swing(LEG_L_KNEE, -(lp::L_KNEE - sway)));
	apply(legFrames_[LEG_L_FOOT], swing(LEG_L_FOOT, -lp::FOOT));

	// 右脚(少し後ろで、ほぼ伸ばして垂らす)
	apply(legFrames_[LEG_R_UP], MMult(swing(LEG_R_UP, lp::R_THIGH - sway), spreadRot(LEG_R_UP, spread + openR)));
	apply(legFrames_[LEG_R_KNEE], swing(LEG_R_KNEE, -(lp::R_KNEE + sway)));
	apply(legFrames_[LEG_R_FOOT], swing(LEG_R_FOOT, -lp::FOOT));
}

// 見た目用の行列を作る
//   描画用には「傾き + 上下ゆれ + 腰中心の補正」を入れ、
//   作り終わったら pos / quaRot を当たり判定用の値に戻す(カプセルが傾かないように)
void Player::ApplyVisualTransform(const Quaternion& logicRot)
{
	const Quaternion visualRot =
		logicRot.Mult(Quaternion::Euler({ floatPitch_, 0.0f, floatRoll_ }));

	// 足元を中心に回すと腰が pivot → visualRot * pivot にずれるので、その差を戻す
	const VECTOR pivot = { 0.0f, cfg::Float::PIVOT_HEIGHT, 0.0f };
	VECTOR offset = VSub(pivot, visualRot.PosAxis(pivot));
	offset.y += sinf(floatBobTime_) * cfg::Float::BOB_AMPLITUDE;

	const VECTOR logicPos = transform_.pos;

	transform_.pos = VAdd(logicPos, offset);
	transform_.quaRot = visualRot;
	transform_.Update();		// ここでモデルの行列が見た目用になる

	transform_.pos = logicPos;
	transform_.quaRot = logicRot;
}

void Player::ChangeState(STATE state)
{
	state_ = state;

	// 各状態遷移の初期処理
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

	// ガード系の状態中は移動などを受け付けない
	if (guard_.IsBusy())
	{
		movePow_ = AsoUtility::VECTOR_ZERO;
		return;
	}

	// 変身
	UpdateTransform();

	// 変身中は他の処理をしない(重力・押し出しだけ処理する)
	if (formChange_.IsActive())
	{
		Collision();
		return;
	}

	// 気溜め
	UpdateChase();

	// 気溜め中は他の処理をしない
	if (isCharging_ || isChargeEnding_)
	{
		return;
	}

	UpdateKiBlast();
	UpdateAttack();
	UpdateKamehame();
	UpdateBoostChase();
	UpdateDodge();

	if (!boostChase_.IsActive() && !isDodge_)
	{
		ProcessMove();
	}

	// 追撃移動
	UpdateCharge();

	Rotate();

	jumpPow_ = AsoUtility::VECTOR_ZERO;

	Collision();

	transform_.quaRot = playerRotY_;
}

void Player::Draw(void)
{
	// 攻撃時の残像
	if (isAfterImage_)
	{
		DrawAfterImage(afterImageMatrix_, 0.35f, false);
	}

	// 高速移動の残像(時間とともに薄くする)
	for (auto& image : boostAfterImages_)
	{
		DrawAfterImage(image.matrix, image.timer / Constants::BoostAfterImageDuration * 0.5f, true);
	}

	// プレイヤー本体
	SetUseLighting(false);
	MV1DrawModel(transform_.modelId);
	SetUseLighting(true);

	DrawKiBlast();
	DrawKamehame();

	// HUD(2D)は3D描画のあとに描く
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
	for (auto& blast : kiBlasts_)
	{
		blast->Draw();
	}
}

void Player::DrawShadow(void)
{
	const float SHADOW_HEIGHT = Constants::PlayerShadowHeight;
	const float SHADOW_SIZE = Constants::PlayerShadowSize;
	const float SHADOW_MAX_ALPHA = 128.0f;

	// 高さに応じた影の濃さ(足元に近いほど濃い)
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

	// テクスチャの端からは端のドットが続くようにする
	SetTextureAddressMode(DX_TEXADDRESS_CLAMP);

	// 頂点データのうち、変化しない部分
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

		// プレイヤー直下の地面ポリゴンを取得
		MV1_COLL_RESULT_POLY_DIM hitDim = MV1CollCheck_Capsule(
			collider->modelId_, -1,
			transform_.pos,
			VAdd(transform_.pos, { 0.0f, -SHADOW_HEIGHT, 0.0f }),
			SHADOW_SIZE);

		for (int i = 0; i < hitDim.HitNum; i++)
		{
			const MV1_COLL_RESULT_POLY& poly = hitDim.Dim[i];

			// 少し持ち上げて地面と重ならないようにする
			VECTOR slide = VScale(poly.Normal, 0.5f);

			for (int v = 0; v < 3; v++)
			{
				vertex[v].pos = VAdd(poly.Position[v], slide);
				vertex[v].dif.a = calcAlpha(poly.Position[v].y);

				// UVは地面ポリゴンとプレイヤーの相対座標から求める
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

// 向きをそのまま設定(dirは正規化済みで長さがあること)
void Player::FaceDirection(VECTOR dir)
{
	playerRotY_ = Quaternion::LookRotation(dir);
	goalQuaRot_ = playerRotY_;
}

// 水平成分だけを使って向きを設定。向けたら true
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

// 上下の傾き(ロックオン用)を dir から求める
void Player::SetLockOnPitch(VECTOR dir)
{
	float horizontalDistance = sqrtf(dir.x * dir.x + dir.z * dir.z);
	lockOnPitch_ = -atan2f(dir.y, horizontalDistance);
}

// 移動・回避に使う前方向と右方向(水平)を求める。
// ロックオン中は敵が基準。敵との水平距離がほぼ0ならカメラ基準
bool Player::GetMoveBasis(VECTOR& forward, VECTOR& right)
{
	if (isLockOn_ && hasAttackTarget_)
	{
		const VECTOR toTarget = ToHorizontal(VSub(attackTargetPos_, transform_.pos));

		// 敵がほぼ真上・真下にいるときは、向きが定まらずおかしくなるので、
		// 距離の条件を満たすときだけ敵基準にする(満たさなければ下のカメラ基準)
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

	// 前方向・右方向
	VECTOR forward;
	VECTOR right;
	GetMoveBasis(forward, right);

	// 水平移動の入力
	VECTOR dir = AsoUtility::VECTOR_ZERO;

	if (ins.IsNew(KEY_INPUT_W)) { dir = VAdd(dir, forward); }
	if (ins.IsNew(KEY_INPUT_S)) { dir = VSub(dir, forward); }
	if (ins.IsNew(KEY_INPUT_D)) { dir = VAdd(dir, right); }
	if (ins.IsNew(KEY_INPUT_A)) { dir = VSub(dir, right); }

	// 上下移動(E:上昇 Q:下降 / パッド: R2を押しながら左スティックの上下)
	float verticalMove = 0.0f;
	if (ins.IsNew(KEY_INPUT_E)) { verticalMove = Constants::VerticalMoveSpeed; }
	if (ins.IsNew(KEY_INPUT_Q)) { verticalMove = -Constants::VerticalMoveSpeed; }

	// パッドの左スティック
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

	// 斜め移動の速度を揃える
	dir = NormalizeSafe(dir);

	if (!AsoUtility::EqualsVZero(dir) &&
		(isJump_ || IsEndLanding()))
	{
		const bool isRun = ins.IsNew(KEY_INPUT_RSHIFT);

		// 形態ごとの速度倍率をかける
		speed_ = (isRun ? SPEED_RUN : SPEED_MOVE) * kFormData[(int)form_].speedRate;
		moveDir_ = dir;
		movePow_ = VScale(dir, speed_);

		// 通常時は移動方向を向く
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

	// 水平移動とは別に上下移動を入れる
	movePow_.y = verticalMove;

	// 高さ制限
	float nextY = transform_.pos.y + movePow_.y;

	if (nextY < Constants::MinHeight)
	{
		movePow_.y = Constants::MinHeight - transform_.pos.y;
	}

	if (nextY > Constants::MaxHeight)
	{
		movePow_.y = Constants::MaxHeight - transform_.pos.y;
	}
}

// 移動中のアニメーションを選ぶ
void Player::PlayMoveAnimation(bool isRun)
{
	auto& ins = InputManager::GetInstance();

	ANIM_TYPE type = isRun ? ANIM_TYPE::FAST_RUN : ANIM_TYPE::RUN;

	// ロックオン中は入力方向に応じて後退・横移動
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

void Player::SetGoalRotate(double rotRad)
{
	VECTOR cameraRot = mainCamera.GetAngles();
	Quaternion axis = Quaternion::AngleAxis((double)cameraRot.y + rotRad, AsoUtility::AXIS_Y);

	// 現在設定されている回転との角度差を取る
	double angleDiff = Quaternion::Angle(axis, goalQuaRot_);

	// しきい値
	if (angleDiff > 0.1)
	{
		stepRotTime_ = TIME_ROT;
	}

	goalQuaRot_ = axis;
}

void Player::Rotate(void)
{
	// NOTE: 元のコードから、時間が二重に減算される挙動をそのまま残している
	stepRotTime_ -= scnMng_.GetDeltaTime();

	stepRotTime_ -= scnMng_.GetDeltaTime();
	if (stepRotTime_ < 0.0f) { stepRotTime_ = 0.0f; }
}

void Player::Collision(void)
{
	// 現在座標を起点に移動後座標を決める
	movedPos_ = VAdd(transform_.pos, movePow_);

	CollisionCapsule();
	CollisionGravity();

	transform_.pos = movedPos_;
}

void Player::CollisionGravity(void)
{
	// ジャンプ量を加算
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

		// 地面との衝突
		auto hit = MV1CollCheck_Line(
			collider->modelId_, -1, gravHitPosUp_, gravHitPosDown_);

		if (hit.HitFlag > 0 && VDot(dirGravity, jumpPow_) > 0.9f)
		{
			// 衝突地点から、少し上に移動
			movedPos_ = VAdd(hit.HitPosition, VScale(dirUpGravity, 2.0f));

			// ジャンプリセット
			jumpPow_ = AsoUtility::VECTOR_ZERO;
			stepJump_ = 0.0f;
			isJump_ = false;
		}
	}
}

void Player::CollisionCapsule(void)
{
	// カプセルを移動させる
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

			// 当たらなくなるまで、ポリゴンの法線方向へ押し出す
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

		// 検出した地面ポリゴン情報の後始末
		MV1CollResultPolyDimTerminate(hits);
	}
}

void Player::CalcGravityPow(void)
{
	const VECTOR dirGravity = AsoUtility::DIR_D;
	const float gravityPow = Planet::DEFAULT_GRAVITY_POW;

	VECTOR gravity = VScale(dirGravity, gravityPow);
	jumpPow_ = VAdd(jumpPow_, gravity);

	// 重力方向と反対方向(マイナス)でなければ、ジャンプ力を無くす
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

	// 敵がいれば追撃(接近)から始める
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

	// 攻撃開始 / 次のコンボの先行入力
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

		// 敵の方を向き、通常コンボ中は一定距離を保つ
		if (VSize(result.faceDir) > Constants::Epsilon)
		{
			FaceHorizontal(result.faceDir);
		}

		if (result.hasFollow)
		{
			movePow_ = result.follow;
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

// 1段目から攻撃を始める
void Player::StartAttack(void)
{
	attack_.StartAttack();

	animationController_->Play((int)ANIM_TYPE::ATTACK01, false);
}

// コンボ終了
void Player::FinishAttack(void)
{
	attack_.Reset();

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

// 次のコンボへ進む(2~8段目)。位置の計算は PlayerAttack、移動とアニメはここ
void Player::AdvanceCombo(void)
{
	const PlayerAttack::ComboStep step = attack_.AdvanceCombo(MakeAttackContext());

	// 残像を残して移動し、敵の方を向く
	if (step.warp)
	{
		if (step.setAfterImagePos)
		{
			afterImagePos_ = transform_.pos;
		}

		LeaveAfterImage();
		transform_.pos = step.warpPos;

		const VECTOR lookDir = VSub(attackTargetPos_, transform_.pos);

		if (step.lookPitch)
		{
			SetLockOnPitch(lookDir);	// 上下の傾きも付ける(8段目)
		}

		FaceHorizontal(lookDir);
	}

	// 瞬間移動する段(4,6,7,8)は、移動量を消す
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

	// 開始
	if (!attack_.IsAttack() &&
		!kamehame_.IsActive() &&
		!isJump_ &&
		!attack_.IsChasing() &&
		!boostChase_.IsActive() &&
		!isDodge_ &&
		((ins.IsNew(KEY_INPUT_TAB) &&	// 必殺技ページ(TAB)を開いている間だけ撃てる
			ins.IsTrgDown(KEY_INPUT_R)) ||
			PadInput::IsKamehameTrg()))	// パッド: L1 を押しながら □
	{
		if (UseKi(KAMEHAME_KI_COST))
		{
			movePow_ = AsoUtility::VECTOR_ZERO;

			animationController_->Play((int)ANIM_TYPE::KAMEHAME, false);

			kamehame_.Start(MakeKamehameContext());
		}
	}

	if (!kamehame_.IsActive())
	{
		return;
	}

	// 更新(狙う方向が返ってきたら、体をそちらへ向ける)
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

// 気溜め
void Player::UpdateChase(void)
{
	auto& ins = InputManager::GetInstance();

	// 気溜め開始
	if (!isCharging_ &&
		!isChargeEnding_ &&
		!attack_.IsAttack() &&
		!kamehame_.IsActive() &&
		!boostChase_.IsActive() &&
		(ins.IsTrgDown(KEY_INPUT_T) || PadInput::IsChargeTrg()))
	{
		isCharging_ = true;
		movePow_ = AsoUtility::VECTOR_ZERO;

		// 0~45フレームを再生し、40~45をループさせる
		animationController_->Play((int)ANIM_TYPE::CHARGE, true, 0.0f, 45.0f);
		animationController_->SetEndLoop(40.0f, 45.0f, 5.0f);

		// 変身後の姿のときは、変身用の気溜めエフェクトを出す
		EffekseerEffect::GetInstance()->PlayChargeEffect(
			transform_.pos, form_ == FORM::SUPER);
	}

	// 気溜め中
	if (isCharging_)
	{
		movePow_ = AsoUtility::VECTOR_ZERO;

		EffekseerEffect::GetInstance()->UpdateChargeEffect(transform_.pos);

		ki_ += KI_CHARGE_SPEED * scnMng_.GetDeltaTime();

		// 満タンで終了モーションへ
		if (ki_ >= MAX_KI)
		{
			ki_ = MAX_KI;
			BeginChargeEnding();
			return;
		}

		// 移動・攻撃などでキャンセル
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
			// Tキーを離したので終了モーションへ
			BeginChargeEnding();
			return;
		}
		else
		{
			return;
		}
	}

	// 気溜め終了モーション
	if (isChargeEnding_)
	{
		movePow_ = AsoUtility::VECTOR_ZERO;

		chargeEndTimer_ += scnMng_.GetDeltaTime();

		// 他のモーション(ガードなど)に切り替わるとIsEndにならないので、時間でも終わらせる
		if (animationController_->IsEnd() || chargeEndTimer_ >= cfg::Charge::END_MAX_TIME)
		{
			isChargeEnding_ = false;
			chargeEndTimer_ = 0.0f;
			animationController_->Play((int)ANIM_TYPE::IDLE);
		}
	}
}

// 気溜めを中断する(フラグ・ループ・エフェクトをすべて解除)
void Player::StopCharge(void)
{
	isCharging_ = false;
	isChargeEnding_ = false;

	animationController_->ClearEndLoop();

	EffekseerEffect::GetInstance()->StopChargeEffect();
}

// 気溜めを終えて、終了モーションに入る
void Player::BeginChargeEnding(void)
{
	isCharging_ = false;
	isChargeEnding_ = true;
	chargeEndTimer_ = 0.0f;

	EffekseerEffect::GetInstance()->StopChargeEffect();

	animationController_->ClearEndLoop();

	// 45~70フレームの終了モーション
	animationController_->Play((int)ANIM_TYPE::CHARGE, false, 45.0f, 70.0f, false, true);
}

// 追撃(Fキーで敵に接近)
//   接近の判断は PlayerAttack、移動・向き・アニメはここ
void Player::UpdateCharge(void)
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
		movePow_ = result.move;
		break;

	case PlayerAttack::ChaseEnd::CANCEL:
		EndChase(false);
		break;

	case PlayerAttack::ChaseEnd::LUNGE:
		// 飛び込む時間を使い切った: 届いていなくてもその場で攻撃する(空振りになる)
		EndChase(false);
		StartAttack();
		break;

	case PlayerAttack::ChaseEnd::ATTACK:
		EndChase(true);
		break;
	}
}

// 追撃を終える。startAttack が true なら、敵の前に位置を合わせてそのまま攻撃に入る
void Player::EndChase(bool startAttack)
{
	canChase_ = false;
	movePow_ = AsoUtility::VECTOR_ZERO;

	if (!startAttack)
	{
		animationController_->Play((int)ANIM_TYPE::IDLE);
		return;
	}

	// 敵との相対位置を攻撃距離にスナップして向きを合わせる
	const PlayerAttack::Context ctx = MakeAttackContext();
	VECTOR snapPos;

	if (PlayerAttack::CalcSnapPos(ctx, snapPos))
	{
		transform_.pos.x = snapPos.x;
		transform_.pos.z = snapPos.z;

		FaceHorizontal(VSub(attackTargetPos_, transform_.pos));
	}

	StartAttack();
}

// 気弾
void Player::UpdateKiBlast(void)
{
	auto& ins = InputManager::GetInstance();

	// 開始
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

	// モーション中
	if (isKiBlast_)
	{
		kiBlastTimer_ += scnMng_.GetDeltaTime();

		// 手が前に出たあたりで発射
		if (!isKiBlastShot_ && kiBlastTimer_ >= Constants::KiBlastShotTime)
		{
			ShotKiBlast();
		}

		// 終了
		if (kiBlastTimer_ >= Constants::KiBlastEndTime)
		{
			isKiBlast_ = false;
			isKiBlastShot_ = false;
			kiBlastTimer_ = 0.0f;

			// 移動していなかったらIDLE
			if (!IsMoveKeyDown(ins))
			{
				animationController_->Play((int)ANIM_TYPE::IDLE);
			}
		}
	}

	// 飛んでいる気弾を更新
	for (auto& blast : kiBlasts_)
	{
		blast->Update();
	}

	// 消えた気弾を削除
	kiBlasts_.erase(
		std::remove_if(kiBlasts_.begin(), kiBlasts_.end(),
			[](const std::unique_ptr<KiBlast>& blast) { return blast->IsDead(); }),
		kiBlasts_.end());
}

// 気弾を1発撃つ
void Player::ShotKiBlast(void)
{
	VECTOR shotPos = transform_.pos;

	if (rightHandFrame_ != -1)
	{
		shotPos = MV1GetFramePosition(transform_.modelId, rightHandFrame_);
	}

	VECTOR shotDir = GetForward();

	// ロックオンしている敵がいる場合は敵に向けて撃つ
	if (isLockOn_ && hasAttackTarget_)
	{
		shotDir = NormalizeSafe(VSub(attackTargetPos_, shotPos));
	}

	kiBlasts_.push_back(std::make_unique<KiBlast>(shotPos, shotDir));

	isKiBlastShot_ = true;
}

const std::vector<std::unique_ptr<KiBlast>>& Player::GetKiBlasts(void) const
{
	return kiBlasts_;
}

// 高速接近(ロックオン中にSPACE)
//   軌道の計算は PlayerBoostChase に任せ、結果(移動・向き・残像)を反映する
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

	// 開始
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

		// 弧の膨らむ側: 左右キー(スティック)を入れていればその側
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
		return;
	}

	// 向き(上下の傾きは lockOnPitch_ として Update で反映される)
	if (VSize(result.faceDir) > Constants::Epsilon)
	{
		FaceHorizontal(result.faceDir);
		SetLockOnPitch(result.faceDir);
	}

	movePow_ = result.move;

	// 残像
	if (result.leaveAfterImage)
	{
		BoostAfterImage image{};
		image.matrix = MV1GetMatrix(transform_.modelId);
		image.timer = Constants::BoostAfterImageDuration;
		boostAfterImages_.push_back(image);
	}
}

// 高速接近を終える(待ち時間は PlayerBoostChase が付ける)
void Player::EndBoostChase(void)
{
	boostChase_.Cancel();
	movePow_ = AsoUtility::VECTOR_ZERO;

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

// 回避(LSHIFT)
void Player::UpdateDodge(void)
{
	auto& ins = InputManager::GetInstance();

	if (!isDodge_)
	{
		if (attack_.IsAttack() || boostChase_.IsActive() || kamehame_.IsActive())
		{
			return;
		}

		if (ins.IsTrgDown(KEY_INPUT_LSHIFT) || PadInput::IsStepTrg())
		{
			VECTOR forward;
			VECTOR right;

			if (!GetMoveBasis(forward, right))
			{
				return;
			}

			// 入力方向へ回避。入力なしなら後ろ
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
		}
	}

	if (!isDodge_)
	{
		return;
	}

	dodgeTimer_ += scnMng_.GetDeltaTime();

	movePow_ = VScale(dodgeDir_, Constants::DodgeSpeed);

	// ロックオン中は敵の方を向いたまま
	if (isLockOn_ && hasAttackTarget_)
	{
		VECTOR dir = VSub(attackTargetPos_, transform_.pos);

		if (VSize(dir) > Constants::Epsilon)
		{
			FaceDirection(VNorm(dir));
		}
	}

	if (dodgeTimer_ >= Constants::DodgeDuration)
	{
		isDodge_ = false;
		dodgeTimer_ = 0.0f;

		movePow_ = AsoUtility::VECTOR_ZERO;
	}
}

bool Player::IsDodging(void) const
{
	return isDodge_;
}

// ガード
//   状態と耐久値は PlayerGuard に任せ、アニメの再生と気の消費だけここで行う
void Player::UpdateGuard(void)
{
	InputManager& ins = InputManager::GetInstance();

	// 攻撃・かめはめ波・気弾・回避・高速接近・変身の最中はガードを始められない
	// (途中でガードすると、モーションや状態が中途半端に残って止まってしまうため)
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

	const PlayerGuard::Result result = guard_.Update(ctx);

	// ガード系の状態中は動けない
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
		// 気溜めの途中でガードしたときは、気溜めをやめる(状態が残って動けなくなるのを防ぐ)
		StopCharge();

		movePow_ = AsoUtility::VECTOR_ZERO;

		animationController_->Play((int)ANIM_TYPE::GUARD, false, 0.0f, -1.0f, true);
	}
	else if (result.released)
	{
		animationController_->Play((int)ANIM_TYPE::IDLE);
	}

	// ガード中にバースト(気が足りない場合は発動しない)
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
	// ガードブレイクしたらモーションを変える
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
	// 死亡中・変身中は無敵
	if (isDead_ || formChange_.IsActive())
	{
		return;
	}

	hp_ -= damage;

	if (hp_ <= 0)
	{
		hp_ = 0;
		isDead_ = true;
	}

	// 攻撃をなくす
	attack_.Reset();

	// 気溜めを解除
	if (isCharging_ || isChargeEnding_)
	{
		StopCharge();
	}

	// かめはめ波を解除
	if (kamehame_.IsActive())
	{
		kamehame_.Cancel();
	}

	// 移動系の状態を解除する(残すと、被弾後に動けなくなったり状態が混ざる)
	boostChase_.Cancel();
	attack_.CancelChase();
	isDodge_ = false;
	dodgeTimer_ = 0.0f;
	isKiBlast_ = false;
	isKiBlastShot_ = false;
	kiBlastTimer_ = 0.0f;

	movePow_ = AsoUtility::VECTOR_ZERO;

	// ダメージ状態
	isDamage_ = true;
	damageTimer_ = Constants::DamageStateTime;

	animationController_->Play((int)ANIM_TYPE::DAMAGE, false);
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

// 攻撃判定・ロックオン・状態の取得/設定
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
	if (ki_ < amount)
	{
		return false;
	}

	ki_ -= amount;
	return true;
}

// ロックオン対象の方を向く(上下の傾きも設定)
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

// ダメージを与えた敵の方を向く
void Player::LookAtDamageEnemy(VECTOR enemyPos)
{
	if (!FaceHorizontal(VSub(enemyPos, transform_.pos)))
	{
		return;
	}

	damageRot_ = playerRotY_;
	transform_.quaRot = damageRot_;
}

// コライダ
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