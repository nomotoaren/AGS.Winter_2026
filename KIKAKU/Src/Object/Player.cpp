#include <string>
#include <cmath>
#include <algorithm>
#include "../Application.h"
#include "../Utility/AsoUtility.h"
#include "../Manager/InputManager.h"
#include "../Manager/SceneManager.h"
#include "../Manager/ResourceManager.h"
#include "../Manager/Camera.h"
#include "Common/AnimationController.h"
#include "Common/Capsule.h"
#include "Common/Collider.h"
#include "Planet.h"
#include "Player.h"
#include "../Manager/EffekseerEffect.h"

// 定数・ヘルパー(このcppの中だけで使う)
namespace
{
	constexpr int kMaxCombo = 8;

	// コンボごとの攻撃判定時間(index 0 = 1段目)
	struct HitWindow { float start; float end; };
	constexpr HitWindow kHitWindows[kMaxCombo] =
	{
		{ 0.20f, 0.30f },	// 1
		{ 0.30f, 0.40f },	// 2
		{ 0.25f, 0.35f },	// 3
		{ 0.30f, 0.40f },	// 4
		{ 0.35f, 0.45f },	// 5
		{ 0.30f, 0.40f },	// 6
		{ 0.40f, 0.50f },	// 7
		{ 0.50f, 0.60f },	// 8
	};

	// WASDのどれかが押されているか
	bool IsMoveKeyDown(InputManager& ins)
	{
		return ins.IsNew(KEY_INPUT_W) ||
			ins.IsNew(KEY_INPUT_A) ||
			ins.IsNew(KEY_INPUT_S) ||
			ins.IsNew(KEY_INPUT_D);
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
	isAttack_(false),
	combo_(0),
	nextAttack_(false),
	hasAttackTarget_(false),
	attackTargetPos_(AsoUtility::VECTOR_ZERO),
	canChase_(false),
	isChasing_(false),
	isAttack04Move_(false),
	attack04MoveTimer_(0.0f),
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
	isBoostChase_(false),
	boostChaseTimer_(0.0f),
	lockOnPitch_(0.0f),

	// 回避
	isDodge_(false),
	dodgeTimer_(0.0f),
	dodgeDir_(AsoUtility::VECTOR_ZERO),

	// ガード
	isGuard_(false),
	guardHp_(100.0f),
	isGuardBreak_(false),
	guardBreakTimer_(0.0f),
	guardRecoverTimer_(0.0f),
	isGuardBurst_(false),
	guardBurstTimer_(0.0f),
	guardBurstTrigger_(false),

	// 空中
	isFlying_(false),

	// 残像
	boostAfterImageTimer_(0.0f),
	afterImageAttachNo_(-1),
	attackTimer_(0.0f),
	hasAttackHit_(false),
	attackTrigger_(false),

	hp_(100),
	isDead_(false),

	attackEndTimer_(0.0f),
	isKamehame_(false),
	kamehameTimer_(0.0f),
	isKamehameBeam_(false),
	leftHandFrame_(-1),
	rightHandFrame_(-1),
	kamehameLightHandle_(-1),
	kamehameDir_(AsoUtility::VECTOR_ZERO),

	capsule_(nullptr),

	kamehameChargeModel_(-1),
	kamehameBeamModel_(-1)
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

	kamehameChargeModel_ =
		ResourceManager::GetInstance().LoadModelDuplicate(ResourceManager::SRC::CHARGE);

	kamehameBeamModel_ =
		ResourceManager::GetInstance().LoadModelDuplicate(ResourceManager::SRC::KAMEHAMEHA);

	transform_.scl = { 1.5f, 1.5f, 1.5f };
	transform_.pos = { 0.0f, 1000.0f, 0.0f };
	transform_.quaRot = Quaternion();
	transform_.quaRotLocal =
		Quaternion::Euler({ 0.0f, AsoUtility::Deg2RadF(180.0f), 0.0f });
	transform_.Update();

	// アニメーションの設定
	InitAnimation();

	// 手のフレーム(かめはめ波・気弾の発射位置用)
	leftHandFrame_ = MV1SearchFrame(transform_.modelId, "mixamorig:LeftHand");
	rightHandFrame_ = MV1SearchFrame(transform_.modelId, "mixamorig:RightHand");

	// かめはめ波用ライト
	kamehameLightHandle_ =
		CreatePointLightHandle(transform_.pos, 700.0f, 0.0f, 0.003f, 0.0f);
	SetLightEnableHandle(kamehameLightHandle_, false);

	// カプセルコライダ
	capsule_ = std::make_unique<Capsule>(transform_);
	capsule_->SetLocalPosTop({ 0.0f, 110.0f, 0.0f });
	capsule_->SetLocalPosDown({ 0.0f, 30.0f, 0.0f });
	capsule_->SetRadius(20.0f);

	// 丸影画像
	imgShadow_ = resMng_.Load(ResourceManager::SRC::PLAYER_SHADOW).handleId_;

	// 初期状態
	ChangeState(STATE::PLAY);
}

void Player::InitAnimation(void)
{
	std::string path = Application::PATH_MODEL + "Player/Animation/";
	std::string EVpath = Application::PATH_MODEL + "Player/AnimationEV";
	animationController_ = std::make_unique<AnimationController>(transform_.modelId);

	animationController_->Add((int)ANIM_TYPE::IDLE, path + "Idle.mv1", 20.0f);

	// 移動
	animationController_->Add((int)ANIM_TYPE::RUN, path + "Walk.mv1", 40.0f);
	animationController_->Add((int)ANIM_TYPE::FAST_RUN, path + "Running.mv1", 40.0f);
	animationController_->Add((int)ANIM_TYPE::LOCK_LEFT, path + "LSWalking.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::LOCK_RIGHT, path + "RSWalking.mv1", 40.0f);
	animationController_->Add((int)ANIM_TYPE::LOCK_LEFT_RUN, path + "Left Strafe.mv1", 40.0f);
	animationController_->Add((int)ANIM_TYPE::LOCK_RIGHT_RUN, path + "Right Strafe.mv1", 40.0f);
	animationController_->Add((int)ANIM_TYPE::LOCK_BACK, path + "Walking Back.mv1", 40.0f);
	animationController_->Add((int)ANIM_TYPE::LOCK_BACK_RUN, path + "Running Back.mv1", 40.0f);
	animationController_->Add((int)ANIM_TYPE::BOOST_CHASE, path + "Flying.mv1", 40.0f);

	// 攻撃
	animationController_->Add((int)ANIM_TYPE::ATTACK01, path + "Attack02.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK02, path + "Attack01.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK03, path + "Attack03.mv1", 80.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK04, path + "Attack04.mv1", 80.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK05, path + "Attack05.mv1", 85.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK06, path + "Attack01.mv1", 80.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK07, path + "Attack07.mv1", 65.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK08, path + "Attack08.mv1", 65.0f);

	// 気技
	animationController_->Add((int)ANIM_TYPE::KI_BLAST, path + "KiBlast.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::KAMEHAME, path + "Special.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::CHARGE, path + "pawer.mv1", 40.0f);

	// 被弾・ガード
	animationController_->Add((int)ANIM_TYPE::DAMAGE, path + "Damege.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::GUARD, path + "Block.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::GUARD_BURST, path + "pawer.mv1", 100.0f);
	animationController_->Add((int)ANIM_TYPE::GUARD_BREAK, path + "GuardBreak.mv1", 20.0f);

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

void Player::Update(void)
{
	UpdateKnockBack();

	// ダメージ中は通常の更新をしない
	if (UpdateDamage())
	{
		return;
	}

	// 状態ごとの更新
	stateUpdate_();

	// ロックオン中は上下の傾きも反映する
	if (isLockOn_ && hasAttackTarget_ && !isAttack_)
	{
		transform_.quaRot =
			playerRotY_.Mult(Quaternion::Euler({ lockOnPitch_, 0.0f, 0.0f }));
	}
	else
	{
		transform_.quaRot = playerRotY_;
	}

	transform_.Update();
	animationController_->Update();

	UpdateAfterImages();
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

	transform_.quaRot = damageRot_;
	transform_.Update();
	animationController_->Update();
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
	attackTrigger_ = false;

	UpdateGuard();
	UpdateGuardBurst();

	// ガード系の状態中は移動などを受け付けない
	if (isGuard_ || isGuardBreak_ || isGuardBurst_)
	{
		movePow_ = AsoUtility::VECTOR_ZERO;
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

	if (!isBoostChase_ && !isDodge_)
	{
		ProcessMove();
	}

	// 追撃移動
	UpdateCharge();

	// ProcessJump();

	Rotate();

	jumpPow_ = AsoUtility::VECTOR_ZERO;

	Collision();

	transform_.quaRot = playerRotY_;
}

void Player::Draw(void)
{
	const int white = GetColor(255, 255, 255);

	// デバッグ表示
	DrawFormatString(10, 100, white, "KI : %.1f / %.1f", ki_, MAX_KI);
	DrawFormatString(10, 120, white, "GUARD : %.1f / 100.0", guardHp_);
	DrawFormatString(10, 300, white, "Combo : %d  AttackTime : %.2f", combo_, attackTimer_);
	DrawFormatString(10, 320, white, "Attack:%d HitTiming:%d HasHit:%d Chase:%d",
		isAttack_, IsAttackHitTiming(), hasAttackHit_, isChasing_);

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

	// テクスチャの端より先は端のドットが続くようにする
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

// 長さが十分あるときだけ正規化する
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

// 水平方向だけを見て向きを設定。向けたら true
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
// ロックオン中は敵方向が基準。敵との水平距離がほぼ0なら false
bool Player::GetMoveBasis(VECTOR& forward, VECTOR& right)
{
	if (isLockOn_ && hasAttackTarget_)
	{
		forward = ToHorizontal(VSub(attackTargetPos_, transform_.pos));

		bool valid = VSize(forward) > Constants::Epsilon;
		forward = NormalizeSafe(forward);
		right = VGet(forward.z, 0.0f, -forward.x);
		return valid;
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

void Player::SetKamehameLight(bool enable)
{
	if (kamehameLightHandle_ != -1)
	{
		SetLightEnableHandle(kamehameLightHandle_, enable);
	}
}

void Player::ProcessMove(void)
{
	auto& ins = InputManager::GetInstance();

	if (isKamehame_)
	{
		movePow_ = AsoUtility::VECTOR_ZERO;
		return;
	}

	if (isAttack_ || isChasing_)
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

	// 上下移動(E:上昇 Q:下降)
	float verticalMove = 0.0f;
	if (ins.IsNew(KEY_INPUT_E)) { verticalMove = Constants::VerticalMoveSpeed; }
	if (ins.IsNew(KEY_INPUT_Q)) { verticalMove = -Constants::VerticalMoveSpeed; }

	// 斜め移動の速度を揃える
	dir = NormalizeSafe(dir);

	if (!AsoUtility::EqualsVZero(dir) &&
		(isJump_ || IsEndLanding()))
	{
		const bool isRun = ins.IsNew(KEY_INPUT_RSHIFT);

		speed_ = isRun ? SPEED_RUN : SPEED_MOVE;
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

	// 横移動とは別に上下移動を入れる
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
		const bool w = ins.IsNew(KEY_INPUT_W);
		const bool a = ins.IsNew(KEY_INPUT_A);
		const bool s = ins.IsNew(KEY_INPUT_S);
		const bool d = ins.IsNew(KEY_INPUT_D);

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
	// NOTE: 元コードから、時間が二重に減算される挙動をそのまま残している
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

void Player::UpdateAttack(void)
{
	auto& ins = InputManager::GetInstance();
	const bool pushAttack = ins.IsTrgDown(KEY_INPUT_F);

	// 敵がいれば追撃(接近)から始める
	if (hasAttackTarget_ &&
		!isAttack_ &&
		!isKamehame_ &&
		!isChasing_ &&
		pushAttack)
	{
		isChasing_ = true;
		canChase_ = false;
		movePow_ = AsoUtility::VECTOR_ZERO;

		animationController_->Play((int)ANIM_TYPE::BOOST_CHASE, true);
		return;
	}

	// 攻撃開始 / 次のコンボの先行入力
	if (!isKamehame_ && !isChasing_ && pushAttack)
	{
		if (!isAttack_)
		{
			StartAttack();
		}
		else
		{
			nextAttack_ = true;
		}
	}

	// 攻撃中
	if (isAttack_)
	{
		attackTimer_ += scnMng_.GetDeltaTime();
		movePow_ = AsoUtility::VECTOR_ZERO;

		FollowAttackTarget();

		const float attackRate = animationController_->GetPlayRate();
		const bool wantNext = nextAttack_ && combo_ < kMaxCombo;

		if (animationController_->IsEnd() ||
			(wantNext && attackRate >= Constants::NextAttackPlayRateThreshold))
		{
			if (wantNext)
			{
				AdvanceCombo();
			}
			else
			{
				FinishAttack();
			}
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
	isAttack_ = true;
	combo_ = 1;
	nextAttack_ = false;

	attackTimer_ = 0.0f;
	hasAttackHit_ = false;
	attackTrigger_ = true;

	animationController_->Play((int)ANIM_TYPE::ATTACK01, false);
}

// 攻撃状態をリセットする(コンボ・タイマー・ヒット情報)
void Player::ResetAttackState(void)
{
	isAttack_ = false;
	combo_ = 0;
	nextAttack_ = false;
	attackTimer_ = 0.0f;
	hasAttackHit_ = false;
}

// コンボ終了
void Player::FinishAttack(void)
{
	ResetAttackState();

	isAttack04Move_ = false;
	attack04MoveTimer_ = 0.0f;

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

// 攻撃中、敵の方を向き、通常コンボ中は一定距離を保つ
void Player::FollowAttackTarget(void)
{
	if (!hasAttackTarget_)
	{
		return;
	}

	VECTOR toTarget = VSub(attackTargetPos_, transform_.pos);

	if (VSize(toTarget) <= Constants::Epsilon)
	{
		return;
	}

	// 向きは横方向だけ
	FaceHorizontal(VNorm(toTarget));

	// 8段目は途中から追従しない
	bool keepAttackPosition = (combo_ >= 1 && combo_ <= 7);

	if (combo_ == kMaxCombo && attackTimer_ < Constants::Combo8FollowEnd)
	{
		keepAttackPosition = true;
	}

	if (!keepAttackPosition)
	{
		return;
	}

	VECTOR enemyDir = ToHorizontal(toTarget);

	if (VSize(enemyDir) <= Constants::Epsilon)
	{
		return;
	}

	enemyDir = VNorm(enemyDir);

	VECTOR targetPos =
		VSub(attackTargetPos_, VScale(enemyDir, Constants::AttackDistance));

	VECTOR follow = VSub(targetPos, transform_.pos);
	movePow_ = VScale(follow, Constants::AttackFollowRate);
}

// 次のコンボへ進む(2~8段目)
void Player::AdvanceCombo(void)
{
	combo_++;
	nextAttack_ = false;

	attackTimer_ = 0.0f;
	hasAttackHit_ = false;
	attackTrigger_ = true;

	// 残像を残して newPos へ移動し、敵の方を向く
	auto warp = [this](VECTOR newPos)
		{
			LeaveAfterImage();
			transform_.pos = newPos;
			FaceHorizontal(VSub(attackTargetPos_, transform_.pos));
		};

	switch (combo_)
	{
	case 2:
		animationController_->Play((int)ANIM_TYPE::ATTACK02, false, 0.0f, -1.0f, false, true);
		break;

	case 3:
		animationController_->Play((int)ANIM_TYPE::ATTACK03, false);
		break;

	case 4:	// 敵の横へ移動
	{
		if (hasAttackTarget_)
		{
			VECTOR toEnemy = ToHorizontal(VSub(attackTargetPos_, transform_.pos));

			if (VSize(toEnemy) > Constants::Epsilon)
			{
				VECTOR dir = VNorm(toEnemy);
				VECTOR sideDir = VGet(-dir.z, 0.0f, dir.x);

				afterImagePos_ = transform_.pos;
				warp(VAdd(attackTargetPos_, VScale(sideDir, Constants::WarpDistance)));
			}
		}

		movePow_ = AsoUtility::VECTOR_ZERO;
		animationController_->Play((int)ANIM_TYPE::ATTACK04, false);
		break;
	}

	case 5:
		animationController_->Play((int)ANIM_TYPE::ATTACK05, false);
		break;

	case 6:	// 敵の反対側へ移動
	{
		if (hasAttackTarget_)
		{
			VECTOR toEnemy = ToHorizontal(VSub(attackTargetPos_, transform_.pos));

			if (VSize(toEnemy) > Constants::Epsilon)
			{
				VECTOR dir = VNorm(toEnemy);
				VECTOR sideDir = VGet(dir.z, 0.0f, -dir.x);

				warp(VAdd(attackTargetPos_, VScale(sideDir, Constants::WarpDistance)));
			}
		}

		movePow_ = AsoUtility::VECTOR_ZERO;
		animationController_->Play((int)ANIM_TYPE::ATTACK06, false);
		break;
	}

	case 7:	// 敵の後ろへ移動
	{
		if (hasAttackTarget_)
		{
			VECTOR dir = VSub(attackTargetPos_, transform_.pos);

			if (VSize(dir) > Constants::Epsilon)
			{
				dir = VNorm(dir);
				warp(VAdd(attackTargetPos_, VScale(dir, Constants::WarpDistance)));
			}
		}

		movePow_ = AsoUtility::VECTOR_ZERO;
		animationController_->Play((int)ANIM_TYPE::ATTACK07, false);
		break;
	}

	case 8:	// 敵の斜め上へ移動
	{
		if (hasAttackTarget_)
		{
			VECTOR dir = ToHorizontal(VSub(attackTargetPos_, transform_.pos));

			if (VSize(dir) > Constants::Epsilon)
			{
				dir = VNorm(dir);

				LeaveAfterImage();

				transform_.pos = VAdd(attackTargetPos_, VScale(dir, -Constants::WarpBackDistance));
				transform_.pos.y += Constants::WarpHeight;

				// 敵を見る(上下の傾きも設定)
				VECTOR lookDir = VSub(attackTargetPos_, transform_.pos);
				SetLockOnPitch(lookDir);
				FaceHorizontal(lookDir);
			}
		}

		movePow_ = AsoUtility::VECTOR_ZERO;
		animationController_->Play((int)ANIM_TYPE::ATTACK08, false);
		break;
	}
	}
}

void Player::UpdateKamehame(void)
{
	auto& ins = InputManager::GetInstance();

	// 開始
	if (!isAttack_ &&
		!isKamehame_ &&
		!isJump_ &&
		!isChasing_ &&
		ins.IsTrgDown(KEY_INPUT_R))
	{
		if (UseKi(KAMEHAME_KI_COST))
		{
			isKamehame_ = true;
			isKamehameBeam_ = false;
			kamehameTimer_ = 0.0f;

			movePow_ = AsoUtility::VECTOR_ZERO;

			animationController_->Play((int)ANIM_TYPE::KAMEHAME, false);
		}
	}

	if (!isKamehame_)
	{
		return;
	}

	kamehameTimer_ += scnMng_.GetDeltaTime();

	// ビーム発射
	if (!isKamehameBeam_ && kamehameTimer_ >= KAMEHAME_SHOT_TIME)
	{
		if (isLockOn_ && hasAttackTarget_)
		{
			kamehameDir_ = ToHorizontal(VSub(attackTargetPos_, GetKamehameStartPos()));
			kamehameDir_ = NormalizeSafe(kamehameDir_);
		}
		else
		{
			kamehameDir_ = GetForward();
		}

		isKamehameBeam_ = true;
	}

	// 終了
	if (kamehameTimer_ >= KAMEHAME_END_TIME)
	{
		isKamehame_ = false;
		isKamehameBeam_ = false;
		kamehameTimer_ = 0.0f;

		animationController_->Play((int)ANIM_TYPE::IDLE);
	}
}

void Player::DrawKamehame(void)
{
	// 使っていなければライトを消して終了
	if (!isKamehame_)
	{
		SetKamehameLight(false);
		return;
	}

	// 手のボーンが見つかっていない
	if (leftHandFrame_ == -1 || rightHandFrame_ == -1)
	{
		return;
	}

	// 左右の手の真ん中
	const VECTOR chargePos = GetKamehameStartPos();

	UpdateKamehameLight(chargePos);

	// チャージ中
	if (!isKamehameBeam_)
	{
		float rate = kamehameTimer_ / KAMEHAME_SHOT_TIME;
		if (rate > 1.0f) { rate = 1.0f; }

		float scale = 0.08f + rate * 0.14f;

		// 発射直前は少し膨らませる
		if (rate > 0.85f)
		{
			float burstRate = (rate - 0.85f) / 0.15f;
			scale += burstRate * 0.06f;
		}

		DrawKamehameChargeModel(chargePos, scale, 3.0f);
		return;
	}

	// 発射中: 手元の気弾
	DrawKamehameChargeModel(chargePos, 0.18f, 5.0f);

	// 発射直後は手元の気弾が小さくなっていく
	float beamTime = kamehameTimer_ - KAMEHAME_SHOT_TIME;

	if (beamTime < KAMEHAME_TRANSITION_TIME)
	{
		float t = beamTime / KAMEHAME_TRANSITION_TIME;
		if (t < 0.0f) { t = 0.0f; }
		if (t > 1.0f) { t = 1.0f; }

		DrawKamehameChargeModel(chargePos, 0.1f * (1.0f - t), 5.0f);
	}

	// ビーム本体
	VECTOR forward = GetForward();

	MV1SetPosition(kamehameBeamModel_, VAdd(chargePos, VScale(forward, 20.0f)));

	float rotY = atan2f(forward.x, forward.z);
	float horizontal = sqrtf(forward.x * forward.x + forward.z * forward.z);
	float rotX = atan2f(forward.y, horizontal);

	MV1SetRotationXYZ(kamehameBeamModel_, { rotX, rotY, 0.0f });
	MV1SetScale(kamehameBeamModel_, { 0.5f, 0.5f, 1.0f });	// ビームサイズ
	MV1DrawModel(kamehameBeamModel_);
}

// 手元の気弾モデルを描く(rotSpeed: 回転の速さ)
void Player::DrawKamehameChargeModel(const VECTOR& pos, float scale, float rotSpeed)
{
	float rot = kamehameTimer_ * rotSpeed;

	MV1SetPosition(kamehameChargeModel_, pos);
	MV1SetScale(kamehameChargeModel_, { scale, scale, scale });
	MV1SetRotationXYZ(kamehameChargeModel_, { rot * 0.5f, rot, 0.0f });
	MV1DrawModel(kamehameChargeModel_);
}

// かめはめ波の発光ライト(+デバッグ表示)
void Player::UpdateKamehameLight(const VECTOR& chargePos)
{
	if (kamehameLightHandle_ == -1)
	{
		return;
	}

	SetLightEnableHandle(kamehameLightHandle_, true);

	// デバッグ表示
	DrawFormatString(10, 360, GetColor(255, 255, 0),
		"BeamDir X:%.2f Y:%.2f Z:%.2f",
		kamehameDir_.x, kamehameDir_.y, kamehameDir_.z);

	VECTOR enemyDir = NormalizeSafe(VSub(attackTargetPos_, chargePos));

	DrawFormatString(10, 380, GetColor(255, 255, 0),
		"EnemyDir X:%.2f Y:%.2f Z:%.2f",
		enemyDir.x, enemyDir.y, enemyDir.z);

	// 体の少し前方に置く
	VECTOR lightPos = transform_.pos;
	lightPos.y += 100.0f;
	lightPos = VAdd(lightPos, VScale(GetForward(), 120.0f));

	SetLightPositionHandle(kamehameLightHandle_, lightPos);
	SetLightDifColorHandle(kamehameLightHandle_, GetColorF(1.0f, 1.0f, 1.0f, 1.0f));
}

bool Player::IsKamehameBeam(void) const
{
	return isKamehameBeam_;
}

float Player::GetKamehameRadius(void) const
{
	return KAMEHAME_BEAM_RADIUS;
}

VECTOR Player::GetKamehameStartPos(void) const
{
	if (leftHandFrame_ == -1 || rightHandFrame_ == -1)
	{
		return transform_.pos;
	}

	VECTOR leftHandPos = MV1GetFramePosition(transform_.modelId, leftHandFrame_);
	VECTOR rightHandPos = MV1GetFramePosition(transform_.modelId, rightHandFrame_);

	return
	{
		(leftHandPos.x + rightHandPos.x) * 0.5f,
		(leftHandPos.y + rightHandPos.y) * 0.5f,
		(leftHandPos.z + rightHandPos.z) * 0.5f
	};
}

VECTOR Player::GetKamehameEndPos(void) const
{
	VECTOR dir = NormalizeSafe(GetForward());

	return VAdd(GetKamehameStartPos(), VScale(dir, KAMEHAME_BEAM_LENGTH));
}

bool Player::IsKamehame(void) const
{
	return isKamehame_;
}

// 気溜め
void Player::UpdateChase(void)
{
	auto& ins = InputManager::GetInstance();

	// 気溜め開始
	if (!isCharging_ &&
		!isChargeEnding_ &&
		!isAttack_ &&
		!isKamehame_ &&
		ins.IsTrgDown(KEY_INPUT_T))
	{
		isCharging_ = true;
		movePow_ = AsoUtility::VECTOR_ZERO;

		// 0~45フレームを再生し、40~45をループさせる
		animationController_->Play((int)ANIM_TYPE::CHARGE, true, 0.0f, 45.0f);
		animationController_->SetEndLoop(40.0f, 45.0f, 5.0f);

		EffekseerEffect::GetInstance()->PlayChargeEffect(transform_.pos);
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
			ins.IsNew(KEY_INPUT_BACKSLASH);

		if (cancelCharge)
		{
			StopCharge();
			animationController_->Play((int)ANIM_TYPE::IDLE);
		}
		else if (!ins.IsNew(KEY_INPUT_T))
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

		if (animationController_->IsEnd())
		{
			isChargeEnding_ = false;
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

	EffekseerEffect::GetInstance()->StopChargeEffect();

	animationController_->ClearEndLoop();

	// 45~70フレームの終了モーション
	animationController_->Play((int)ANIM_TYPE::CHARGE, false, 45.0f, 70.0f, false, true);
}

// 追撃(Fキーで敵に接近)
void Player::UpdateCharge(void)
{
	if (!isChasing_)
	{
		return;
	}

	movePow_ = AsoUtility::VECTOR_ZERO;

	if (!hasAttackTarget_)
	{
		isChasing_ = false;
		canChase_ = false;
		return;
	}

	VECTOR dir = VSub(attackTargetPos_, transform_.pos);
	float distance = VSize(dir);

	// 目標距離に到達したら攻撃開始
	if (distance <= Constants::ChaseStopDistance)
	{
		isChasing_ = false;
		canChase_ = false;

		// 敵との相対位置を攻撃距離にスナップして向きを合わせる
		VECTOR enemyDir = ToHorizontal(VSub(attackTargetPos_, transform_.pos));

		if (VSize(enemyDir) > Constants::Epsilon)
		{
			enemyDir = VNorm(enemyDir);

			transform_.pos =
				VSub(attackTargetPos_, VScale(enemyDir, Constants::AttackDistance));

			FaceHorizontal(VSub(attackTargetPos_, transform_.pos));
		}

		StartAttack();
		return;
	}

	dir = VNorm(dir);

	FaceHorizontal(dir);

	// 敵を通り抜けないようにする
	float moveDistance = distance - Constants::ChaseStopDistance;

	if (moveDistance > Constants::ChaseSpeed)
	{
		moveDistance = Constants::ChaseSpeed;
	}

	movePow_ = VScale(dir, moveDistance);
}

// 気弾
void Player::UpdateKiBlast(void)
{
	auto& ins = InputManager::GetInstance();

	// 開始
	if (!isKiBlast_ &&
		!isAttack_ &&
		!isKamehame_ &&
		!isCharging_ &&
		!isChargeEnding_ &&
		ins.IsTrgDown(KEY_INPUT_U))
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

		// 手を前に出したあたりで発射
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

			// 移動していない時だけIDLE
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
void Player::UpdateBoostChase(void)
{
	auto& ins = InputManager::GetInstance();

	// 開始
	if (!isBoostChase_ &&
		isLockOn_ &&
		hasAttackTarget_ &&
		!isAttack_ &&
		!isKamehame_ &&
		ins.IsTrgDown(KEY_INPUT_SPACE))
	{
		isBoostChase_ = true;

		boostChaseTimer_ = 0.0f;
		boostAfterImageTimer_ = 0.0f;

		movePow_ = AsoUtility::VECTOR_ZERO;

		animationController_->Play((int)ANIM_TYPE::BOOST_CHASE);
	}

	// 攻撃したら終了
	if (isBoostChase_ && isAttack_)
	{
		isBoostChase_ = false;
		boostChaseTimer_ = 0.0f;
		return;
	}

	if (!isBoostChase_)
	{
		return;
	}

	boostChaseTimer_ += scnMng_.GetDeltaTime();

	// 敵への方向
	VECTOR dir = VSub(attackTargetPos_, transform_.pos);
	float distance = VSize(dir);

	if (distance <= Constants::Epsilon)
	{
		isBoostChase_ = false;
		boostChaseTimer_ = 0.0f;
		return;
	}

	dir = VNorm(dir);

	// 敵の方を向く
	FaceDirection(dir);

	// 最初は一瞬その場で構える
	if (boostChaseTimer_ < Constants::BoostInitialDelay)
	{
		movePow_ = AsoUtility::VECTOR_ZERO;
		return;
	}

	// 敵の手前で停止
	if (distance <= Constants::BoostStopDistance)
	{
		isBoostChase_ = false;
		boostChaseTimer_ = 0.0f;

		movePow_ = AsoUtility::VECTOR_ZERO;

		animationController_->Play((int)ANIM_TYPE::IDLE);
		return;
	}

	// 加速(構えが終わってから少しずつ速くする)
	float boostSpeed = Constants::BoostMaxSpeed;

	if (boostChaseTimer_ < Constants::BoostAccelWindow)
	{
		float rate =
			(boostChaseTimer_ - Constants::BoostInitialDelay) /
			(Constants::BoostAccelWindow - Constants::BoostInitialDelay);

		boostSpeed = Constants::BoostMaxSpeed * rate;
	}

	// 敵を通り抜けないようにする
	float moveDistance = distance - Constants::BoostStopDistance;

	if (moveDistance > boostSpeed)
	{
		moveDistance = boostSpeed;
	}

	movePow_ = VScale(dir, moveDistance);

	// 一定間隔で残像を残す
	boostAfterImageTimer_ -= scnMng_.GetDeltaTime();

	if (boostAfterImageTimer_ <= 0.0f)
	{
		BoostAfterImage image;
		image.matrix = MV1GetMatrix(transform_.modelId);
		image.timer = Constants::BoostAfterImageDuration;

		boostAfterImages_.push_back(image);

		boostAfterImageTimer_ = Constants::BoostAfterImageInterval;
	}
}

// 回避(LSHIFT)
void Player::UpdateDodge(void)
{
	auto& ins = InputManager::GetInstance();

	if (!isDodge_)
	{
		if (isAttack_ || isBoostChase_ || isKamehame_)
		{
			return;
		}

		if (ins.IsTrgDown(KEY_INPUT_LSHIFT))
		{
			VECTOR forward;
			VECTOR right;

			if (!GetMoveBasis(forward, right))
			{
				return;
			}

			// 入力方向へ回避。入力なしなら後ろへ
			if (ins.IsNew(KEY_INPUT_W)) { dodgeDir_ = forward; }
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
void Player::UpdateGuard(void)
{
	InputManager& ins = InputManager::GetInstance();
	float deltaTime = SceneManager::GetInstance().GetDeltaTime();

	// ガードブレイク中
	if (isGuardBreak_)
	{
		guardBreakTimer_ -= deltaTime;

		movePow_ = AsoUtility::VECTOR_ZERO;

		if (guardBreakTimer_ <= 0.0f)
		{
			guardBreakTimer_ = 0.0f;
			isGuardBreak_ = false;

			guardHp_ = Constants::GuardMaxHp;

			animationController_->Play((int)ANIM_TYPE::IDLE, true, 0.0f, -1.0f, false, true);
		}

		return;
	}

	// ガード耐久値の回復(被弾から一定時間後に回復開始)
	if (guardHp_ < Constants::GuardMaxHp)
	{
		if (guardRecoverTimer_ > 0.0f)
		{
			guardRecoverTimer_ -= deltaTime;
		}
		else
		{
			guardHp_ += Constants::GuardRecoverSpeed * deltaTime;

			if (guardHp_ > Constants::GuardMaxHp)
			{
				guardHp_ = Constants::GuardMaxHp;
			}
		}
	}

	// ガード開始
	if (!isGuard_ && !isGuardBurst_ && ins.IsNew(KEY_INPUT_L))
	{
		isGuard_ = true;

		movePow_ = AsoUtility::VECTOR_ZERO;

		animationController_->Play((int)ANIM_TYPE::GUARD, false, 0.0f, -1.0f, true);
	}
	// ガード解除
	else if (isGuard_ && !ins.IsNew(KEY_INPUT_L))
	{
		isGuard_ = false;

		animationController_->Play((int)ANIM_TYPE::IDLE);
	}
}

void Player::UpdateGuardBurst(void)
{
	auto& ins = InputManager::GetInstance();
	float deltaTime = SceneManager::GetInstance().GetDeltaTime();

	guardBurstTrigger_ = false;

	// バースト中
	if (isGuardBurst_)
	{
		guardBurstTimer_ -= deltaTime;

		movePow_ = AsoUtility::VECTOR_ZERO;

		if (guardBurstTimer_ <= 0.0f)
		{
			guardBurstTimer_ = 0.0f;
			isGuardBurst_ = false;

			animationController_->Play((int)ANIM_TYPE::IDLE, true, 0.0f, -1.0f, false, true);
		}

		return;
	}

	// ガード中にBでバースト(気が足りない場合は発動しない)
	if (isGuard_ && ins.IsTrgDown(KEY_INPUT_B))
	{
		if (!UseKi(Constants::GuardBurstKiCost))
		{
			return;
		}

		isGuardBurst_ = true;
		guardBurstTrigger_ = true;
		guardBurstTimer_ = Constants::GuardBurstTime;

		isGuard_ = false;

		movePow_ = AsoUtility::VECTOR_ZERO;

		animationController_->ClearEndLoop();
		animationController_->Play((int)ANIM_TYPE::GUARD_BURST, false, 0.0f, 45.0f, false, true);
	}
}

void Player::GuardDamage(float damage)
{
	if (isGuardBreak_)
	{
		return;
	}

	// 回復開始までの時間をリセット
	guardRecoverTimer_ = Constants::GuardRecoverDelay;

	guardHp_ -= damage;

	if (guardHp_ <= 0.0f)
	{
		guardHp_ = 0.0f;

		isGuard_ = false;
		isGuardBreak_ = true;
		guardBreakTimer_ = Constants::GuardBreakTime;

		movePow_ = AsoUtility::VECTOR_ZERO;

		animationController_->Play((int)ANIM_TYPE::GUARD_BREAK, true, 0.0f, -1.0f, false, true);
	}
}

bool Player::IsGuard(void) const { return isGuard_; }
bool Player::IsGuardBreak(void) const { return isGuardBreak_; }
bool Player::IsGuardBurst(void) const { return isGuardBurst_; }
bool Player::IsGuardBurstTrigger(void) const { return guardBurstTrigger_; }

// ダメージ
void Player::Damage(int damage)
{
	if (isDead_)
	{
		return;
	}

	hp_ -= damage;

	if (hp_ <= 0)
	{
		hp_ = 0;
		isDead_ = true;
	}

	// 攻撃中なら解除
	ResetAttackState();

	// 気溜めを解除
	if (isCharging_ || isChargeEnding_)
	{
		StopCharge();
	}

	// かめはめ波を解除
	if (isKamehame_)
	{
		isKamehame_ = false;
		isKamehameBeam_ = false;
		kamehameTimer_ = 0.0f;

		SetKamehameLight(false);
	}

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
	return isAttack_;
}

bool Player::IsAttackHitTiming(void) const
{
	// 範囲外のコンボは既定値(0.2~0.3秒)
	HitWindow window = { 0.2f, 0.3f };

	if (combo_ >= 1 && combo_ <= kMaxCombo)
	{
		window = kHitWindows[combo_ - 1];
	}

	return isAttack_ &&
		attackTimer_ >= window.start &&
		attackTimer_ <= window.end;
}

bool Player::HasAttackHit(void) const { return hasAttackHit_; }
void Player::SetAttackHit(void) { hasAttackHit_ = true; }

VECTOR Player::GetForward(void) const
{
	return playerRotY_.GetForward();
}

bool Player::IsDead(void) const { return isDead_; }
int Player::GetHp(void) const { return hp_; }
int Player::GetCombo(void) const { return combo_; }

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
