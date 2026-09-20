#include <string>
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

Player::Player(void)
{

	animationController_ = nullptr;
	state_ = STATE::NONE;

	speed_ = 0.0f;
	moveDir_ = AsoUtility::VECTOR_ZERO;
	movePow_ = AsoUtility::VECTOR_ZERO;
	movedPos_ = AsoUtility::VECTOR_ZERO;
	knockBackPow_ = AsoUtility::VECTOR_ZERO;
	isDamage_ = false;
	damageTimer_ = 0.0f;

	playerRotY_ = Quaternion();
	goalQuaRot_ = Quaternion();
	stepRotTime_ = 0.0f;

	jumpPow_ = AsoUtility::VECTOR_ZERO;
	isJump_ = false;
	stepJump_ = 0.0f;

	// 衝突チェック
	gravHitPosDown_ = AsoUtility::VECTOR_ZERO;
	gravHitPosUp_ = AsoUtility::VECTOR_ZERO;

	imgShadow_ = -1;
	recordTime_ = 0.0f;
	isRecording_ = false;

	// 攻撃関連
	isAttack_ = false;
	combo_ = 0;
	nextAttack_ = false;
	hasAttackTarget_ = false;
	attackTargetPos_ = AsoUtility::VECTOR_ZERO;
	canChase_ = false;
	isChasing_ = false;
	isAttack04Move_ = false;
	attack04MoveTimer_ = 0.0f;
	afterImageModel_ = -1;
	isAfterImage_ = false;
	afterImageTimer_ = 0.0f;
	afterImagePos_ =
		AsoUtility::VECTOR_ZERO;
	// 気を溜める
	isCharging_ = false;
	isChargeEnding_ = false;
	ki_ = 0.0f;

	// 気弾
	isKiBlast_ = false;
	isKiBlastShot_ = false;
	kiBlastTimer_ = 0.0f;

	// ロックオン
	isLockOn_ = false;
	isBoostChase_ = false;
	boostChaseTimer_ = 0.0f;
	lockOnPitch_ = 0.0f;

	// 回避
	isDodge_ = false;
	dodgeTimer_ = 0.0f;
	dodgeDir_ = AsoUtility::VECTOR_ZERO;

	// ガード
	isGuard_ = false;
	guardHp_ = 100.0f;
	isGuardBreak_ = false;
	guardBreakTimer_ = 0.0f;
	guardRecoverTimer_ = 0.0f;
	isGuardBurst_ = false;
	guardBurstTimer_ = 0.0f;
	guardBurstTrigger_ = false;

	// 空中
	isFlying_ = false;

	// 残像
	boostAfterImageTimer_ = 0.0f;
	afterImageAttachNo_ = -1;
	attackTimer_ = 0.0f;
	hasAttackHit_ = false;
	attackTrigger_ = false;

	hp_ = 100;
	isDead_ = false;

	attackEndTimer_ = 0.0f;
	isKamehame_ = false;
	kamehameTimer_ = 0.0f;
	isKamehameBeam_ = false;
	leftHandFrame_ = -1;
	rightHandFrame_ = -1;
	kamehameLightHandle_ = -1;

	capsule_ = nullptr;

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
	transform_.SetModel(resMng_.LoadModelDuplicate(
		ResourceManager::SRC::PLAYER));

	afterImageModel_ =
		MV1DuplicateModel(
			transform_.modelId
		);

	kamehameChargeModel_ =
		ResourceManager::GetInstance().
		LoadModelDuplicate(
			ResourceManager::SRC::CHARGE
		);

	kamehameBeamModel_ =
		ResourceManager::GetInstance().
		LoadModelDuplicate(
			ResourceManager::SRC::KAMEHAMEHA
		);

	transform_.scl =
	{
		1.0f,
		1.0f,
		1.0f
	};

	transform_.pos = { 0.0f, 1000.0f, 0.0f };
	transform_.quaRot = Quaternion();
	transform_.quaRotLocal =
		Quaternion::Euler({ 0.0f, AsoUtility::Deg2RadF(180.0f), 0.0f });
	transform_.Update();

	// アニメーションの設定
	InitAnimation();

	leftHandFrame_ =
		MV1SearchFrame(
			transform_.modelId,
			"mixamorig:LeftHand"
		);

	rightHandFrame_ =
		MV1SearchFrame(
			transform_.modelId,
			"mixamorig:RightHand"
		);

	kamehameLightHandle_ =
		CreatePointLightHandle(
			transform_.pos,
			700.0f,
			0.0f,
			0.003f,
			0.0f
		);

	SetLightEnableHandle(
		kamehameLightHandle_,
		false
	);

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

void Player::Update(void)
{

	if (VSize(knockBackPow_) > 0.1f)
	{
		transform_.pos =
			VAdd(
				transform_.pos,
				knockBackPow_
			);

		knockBackPow_ =
			VScale(
				knockBackPow_,
				0.8f
			);
	}
	else
	{
		knockBackPow_ =
			AsoUtility::VECTOR_ZERO;
	}

	// ダメージ中
	if (isDamage_)
	{
		float damageDeltaTime =
			SceneManager::GetInstance().GetDeltaTime();

		damageTimer_ -= damageDeltaTime;

		if (damageTimer_ <= 0.0f)
		{
			damageTimer_ = 0.0f;
			isDamage_ = false;

			animationController_->Play(
				(int)ANIM_TYPE::IDLE
			);
		}
		else
		{
			transform_.Update();

			animationController_->Update();

			return;
		}
	}

	// 更新ステップ
	stateUpdate_();

	if (isLockOn_)
	{
		// 格闘中は上下に傾けない
		if (isAttack_)
		{
			transform_.quaRot =
				playerRotY_;
		}
		else
		{
			transform_.quaRot =
				playerRotY_.Mult(
					Quaternion::Euler(
						{
							lockOnPitch_,
							0.0f,
							0.0f
						}
					)
				);
		}
	}
	else
	{
		transform_.quaRot =
			playerRotY_;

		lockOnPitch_ = 0.0f;
	}

	// モデル制御更新
	transform_.Update();

	// アニメーション再生
	animationController_->Update();

	if (isAfterImage_)
	{
		afterImageTimer_ -=
			scnMng_.GetDeltaTime();

		if (afterImageTimer_ <= 0.0f)
		{
			afterImageTimer_ = 0.0f;
			isAfterImage_ = false;
		}
	}

	for (auto it = boostAfterImages_.begin();
		it != boostAfterImages_.end();)
	{
		it->timer -=
			scnMng_.GetDeltaTime();

		if (it->timer <= 0.0f)
		{
			it =
				boostAfterImages_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void Player::Draw(void)
{
	DrawFormatString(
		10,
		100,
		GetColor(255, 255, 255),
		"KI : %.1f / %.1f",
		ki_,
		MAX_KI
	);

	DrawFormatString(
		10,
		120,
		GetColor(255, 255, 255),
		"GUARD : %.1f / 100.0",
		guardHp_
	);

	DrawFormatString(
		10,
		300,
		GetColor(255, 255, 255),
		"Combo : %d  AttackTime : %.2f",
		combo_,
		attackTimer_
	);

	DrawFormatString(
		10,
		320,
		GetColor(255, 255, 255),
		"Attack:%d HitTiming:%d HasHit:%d Chase:%d",
		isAttack_,
		IsAttackHitTiming(),
		hasAttackHit_,
		isChasing_
	);

	if (isAfterImage_)
	{
		MATRIX playerMatrix =
			MV1GetMatrix(
				transform_.modelId
			);

		MV1SetMatrix(
			transform_.modelId,
			afterImageMatrix_
		);

		MV1SetOpacityRate(
			transform_.modelId,
			0.35f
		);

		MV1DrawModel(
			transform_.modelId
		);

		MV1SetMatrix(
			transform_.modelId,
			playerMatrix
		);

		MV1SetOpacityRate(
			transform_.modelId,
			1.0f
		);
	}

	// 高速移動の残像
	for (auto& image : boostAfterImages_)
	{
		MATRIX playerMatrix =
			MV1GetMatrix(
				transform_.modelId
			);

		MV1SetMatrix(
			transform_.modelId,
			image.matrix
		);

		float alpha =
			image.timer / 0.15f;

		MV1SetOpacityRate(
			transform_.modelId,
			alpha * 0.5f
		);

		SetUseLighting(FALSE);

		MV1DrawModel(
			transform_.modelId
		);

		SetUseLighting(TRUE);

		MV1SetMatrix(
			transform_.modelId,
			playerMatrix
		);

		MV1SetOpacityRate(
			transform_.modelId,
			1.0f
		);
	}

	// プレイヤー本体
	SetUseLighting(FALSE);

	MV1DrawModel(
		transform_.modelId
	);

	SetUseLighting(TRUE);

	DrawKiBlast();

	DrawKamehame();
}

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

void Player::InitAnimation(void)
{

	std::string path = Application::PATH_MODEL + "Player/";
	animationController_ = std::make_unique<AnimationController>(transform_.modelId);
	animationController_->Add((int)ANIM_TYPE::IDLE, path + "Idle.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::RUN, path + "Running.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::FAST_RUN, path + "Fast Run.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::LOCK_LEFT, path + "LSWalking.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::LOCK_RIGHT, path + "RSWalking.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::LOCK_LEFT_RUN, path + "Left Strafe.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::LOCK_RIGHT_RUN, path + "Right Strafe.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::LOCK_BACK, path + "Walking Back.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::LOCK_BACK_RUN, path + "Running Back.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::BOOST_CHASE, path + "Kousoku.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::JUMP, path + "Jumping.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::WARP_PAUSE, path + "WarpPose.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::FLY, path + "Flying.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::FALLING, path + "Falling.mv1", 80.0f);
	animationController_->Add((int)ANIM_TYPE::VICTORY, path + "Victory.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK01, path + "Attack01.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK02, path + "Attack02.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK03, path + "Attack03.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK04, path + "Attack04.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK05, path + "Attack05.mv1", 75.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK06, path + "Attack01.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK07, path + "Attack07.mv1", 55.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK08, path + "Attack09.mv1", 55.0f);
	animationController_->Add((int)ANIM_TYPE::KI_BLAST, path + "KiBlast.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::KAMEHAME, path + "かめはめ波.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::CHARGE, path + "pawer.mv1", 40.0f);
	animationController_->Add((int)ANIM_TYPE::DAMAGE, path + "Damege.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::GUARD, path + "Block.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::GUARD_BURST, path + "pawer.mv1", 100.0f);
	animationController_->Add((int)ANIM_TYPE::GUARD_BREAK, path + "GuardBreak.mv1", 20.0f);
	animationController_->Play((int)ANIM_TYPE::IDLE);
}

void Player::ChangeState(STATE state)
{

	// 状態変更
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
	InputManager& ins = InputManager::GetInstance();

	attackTrigger_ = false;
	
	UpdateGuard();

	UpdateGuardBurst();

	if (isGuard_ ||
		isGuardBreak_ ||
		isGuardBurst_)
	{
		movePow_ =
			AsoUtility::VECTOR_ZERO;

		return;
	}

	// 気溜め
	UpdateChase();

	// 気溜め中は他の処理をしない
	if (isCharging_ ||
		isChargeEnding_)
	{
		return;
	}

	// 気弾
	UpdateKiBlast();

	// 攻撃処理
	UpdateAttack();

	// かめはめ波
	UpdateKamehame();

	UpdateBoostChase();

	UpdateDodge();

	if (!isBoostChase_ &&
		!isDodge_)
	{
		// 移動処理
		ProcessMove();
	}

	// 高速追撃
	UpdateCharge();

	// ジャンプ処理
	//ProcessJump();

	// 移動方向に応じた回転
	Rotate();

	jumpPow_ =
		AsoUtility::VECTOR_ZERO;

	// 衝突判定
	Collision();

	// 回転させる
	transform_.quaRot = playerRotY_;
}

void Player::DrawShadow(void)
{

	float PLAYER_SHADOW_HEIGHT = 300.0f;
	float PLAYER_SHADOW_SIZE = 30.0f;

	int i;
	MV1_COLL_RESULT_POLY_DIM HitResDim;
	MV1_COLL_RESULT_POLY* HitRes;
	VERTEX3D Vertex[3] = { VERTEX3D(), VERTEX3D(), VERTEX3D() };
	VECTOR SlideVec;
	int ModelHandle;

	// ライティングを無効にする
	SetUseLighting(FALSE);

	// Ｚバッファを有効にする
	SetUseZBuffer3D(TRUE);

	// テクスチャアドレスモードを CLAMP にする( テクスチャの端より先は端のドットが延々続く )
	SetTextureAddressMode(DX_TEXADDRESS_CLAMP);

	// 影を落とすモデルの数だけ繰り返し
	for (const auto c : colliders_)
	{

		// チェックするモデルは、jが0の時はステージモデル、1以上の場合はコリジョンモデル
		ModelHandle = c.lock()->modelId_;

		// プレイヤーの直下に存在する地面のポリゴンを取得
		HitResDim = MV1CollCheck_Capsule(
			ModelHandle, -1,
			transform_.pos, VAdd(transform_.pos, { 0.0f, -PLAYER_SHADOW_HEIGHT, 0.0f }), PLAYER_SHADOW_SIZE);

		// 頂点データで変化が無い部分をセット
		Vertex[0].dif = GetColorU8(255, 255, 255, 255);
		Vertex[0].spc = GetColorU8(0, 0, 0, 0);
		Vertex[0].su = 0.0f;
		Vertex[0].sv = 0.0f;
		Vertex[1] = Vertex[0];
		Vertex[2] = Vertex[0];

		// 球の直下に存在するポリゴンの数だけ繰り返し
		HitRes = HitResDim.Dim;
		for (i = 0; i < HitResDim.HitNum; i++, HitRes++)
		{
			// ポリゴンの座標は地面ポリゴンの座標
			Vertex[0].pos = HitRes->Position[0];
			Vertex[1].pos = HitRes->Position[1];
			Vertex[2].pos = HitRes->Position[2];

			// ちょっと持ち上げて重ならないようにする
			SlideVec = VScale(HitRes->Normal, 0.5f);
			Vertex[0].pos = VAdd(Vertex[0].pos, SlideVec);
			Vertex[1].pos = VAdd(Vertex[1].pos, SlideVec);
			Vertex[2].pos = VAdd(Vertex[2].pos, SlideVec);

			// ポリゴンの不透明度を設定する
			Vertex[0].dif.a = 0;
			Vertex[1].dif.a = 0;
			Vertex[2].dif.a = 0;
			if (HitRes->Position[0].y > transform_.pos.y - PLAYER_SHADOW_HEIGHT)
				Vertex[0].dif.a = static_cast<int>(roundf(128.0f * (1.0f - fabs(HitRes->Position[0].y - transform_.pos.y) / PLAYER_SHADOW_HEIGHT)));

			if (HitRes->Position[1].y > transform_.pos.y - PLAYER_SHADOW_HEIGHT)
				Vertex[1].dif.a = static_cast<int>(roundf(128.0f * (1.0f - fabs(HitRes->Position[1].y - transform_.pos.y) / PLAYER_SHADOW_HEIGHT)));

			if (HitRes->Position[2].y > transform_.pos.y - PLAYER_SHADOW_HEIGHT)
				Vertex[2].dif.a = static_cast<int>(roundf(128.0f * (1.0f - fabs(HitRes->Position[2].y - transform_.pos.y) / PLAYER_SHADOW_HEIGHT)));

			// ＵＶ値は地面ポリゴンとプレイヤーの相対座標から割り出す
			Vertex[0].u = (HitRes->Position[0].x - transform_.pos.x) / (PLAYER_SHADOW_SIZE * 2.0f) + 0.5f;
			Vertex[0].v = (HitRes->Position[0].z - transform_.pos.z) / (PLAYER_SHADOW_SIZE * 2.0f) + 0.5f;
			Vertex[1].u = (HitRes->Position[1].x - transform_.pos.x) / (PLAYER_SHADOW_SIZE * 2.0f) + 0.5f;
			Vertex[1].v = (HitRes->Position[1].z - transform_.pos.z) / (PLAYER_SHADOW_SIZE * 2.0f) + 0.5f;
			Vertex[2].u = (HitRes->Position[2].x - transform_.pos.x) / (PLAYER_SHADOW_SIZE * 2.0f) + 0.5f;
			Vertex[2].v = (HitRes->Position[2].z - transform_.pos.z) / (PLAYER_SHADOW_SIZE * 2.0f) + 0.5f;

			// 影ポリゴンを描画
			DrawPolygon3D(Vertex, 1, imgShadow_, TRUE);
		}

		// 検出した地面ポリゴン情報の後始末
		MV1CollResultPolyDimTerminate(HitResDim);
	}

	// ライティングを有効にする
	SetUseLighting(TRUE);

	// Ｚバッファを無効にする
	SetUseZBuffer3D(FALSE);

}

void Player::DrawKiBlast(void)
{
	for (auto& blast : kiBlasts_)
	{
		blast->Draw();
	}
}

void Player::ProcessMove(void)
{
	auto& ins =
		InputManager::GetInstance();

	if (isKamehame_)
	{
		movePow_ =
			AsoUtility::VECTOR_ZERO;

		return;
	}

	if (isAttack_ ||
		isChasing_)
	{
		return;
	}

	movePow_ =
		AsoUtility::VECTOR_ZERO;

	Quaternion cameraRot =
		mainCamera.GetQuaRotOutX();

	VECTOR dir =
		AsoUtility::VECTOR_ZERO;

	VECTOR forward =
		AsoUtility::VECTOR_ZERO;

	VECTOR right =
		AsoUtility::VECTOR_ZERO;


	// ロックオン中
	if (isLockOn_ &&
		hasAttackTarget_)
	{
		// プレイヤー → 敵
		forward =
			VSub(
				attackTargetPos_,
				transform_.pos
			);

		forward.y = 0.0f;

		if (VSize(forward) > 0.001f)
		{
			forward =
				VNorm(forward);
		}

		// 右方向
		right =
			VGet(
				forward.z,
				0.0f,
				-forward.x
			);
	}
	else
	{
		// 通常時
		forward =
			cameraRot.GetForward();

		right =
			cameraRot.GetRight();

		forward.y = 0.0f;
		right.y = 0.0f;

		if (VSize(forward) > 0.001f)
		{
			forward =
				VNorm(forward);
		}

		if (VSize(right) > 0.001f)
		{
			right =
				VNorm(right);
		}
	}


	// 上下方向は移動に使わない
	forward.y = 0.0f;
	right.y = 0.0f;

	if (VSize(forward) > 0.001f)
	{
		forward =
			VNorm(forward);
	}

	if (VSize(right) > 0.001f)
	{
		right =
			VNorm(right);
	}


	// W：前
	if (ins.IsNew(KEY_INPUT_W))
	{
		dir =
			VAdd(
				dir,
				forward
			);
	}

	// S：後ろ
	if (ins.IsNew(KEY_INPUT_S))
	{
		dir =
			VSub(
				dir,
				forward
			);
	}

	// D：右
	if (ins.IsNew(KEY_INPUT_D))
	{
		dir =
			VAdd(
				dir,
				right
			);
	}

	// A：左
	if (ins.IsNew(KEY_INPUT_A))
	{
		dir =
			VSub(
				dir,
				right
			);
	}

	// 上昇
	if (ins.IsNew(KEY_INPUT_E))
	{
		movePow_.y += 8.0f;
	}

	// 下降
	if (ins.IsNew(KEY_INPUT_Q))
	{
		movePow_.y -= 8.0f;
	}

	// 高さ制限
	float nextY =
		transform_.pos.y + movePow_.y;

	const float MIN_HEIGHT = 100.0f;
	const float MAX_HEIGHT = 2000.0f;

	if (nextY < MIN_HEIGHT)
	{
		movePow_.y =
			MIN_HEIGHT - transform_.pos.y;
	}

	if (nextY > MAX_HEIGHT)
	{
		movePow_.y =
			MAX_HEIGHT - transform_.pos.y;
	}

	if (VSize(dir) > 0.001f)
	{
		dir =
			VNorm(dir);
	}

	// 斜め移動の速度を揃える
	if (VSize(dir) > 0.001f)
	{
		dir =
			VNorm(dir);
	}


	// 移動
	if (!AsoUtility::EqualsVZero(dir) &&
		(isJump_ || IsEndLanding()))
	{
		speed_ =
			SPEED_MOVE;

		// ダッシュ
		if (ins.IsNew(KEY_INPUT_RSHIFT))
		{
			speed_ =
				SPEED_RUN;
		}

		moveDir_ =
			dir;

		movePow_ =
			VScale(
				dir,
				speed_
			);


		// 通常時は移動方向を向く
		if (!isLockOn_)
		{
			playerRotY_ =
				Quaternion::LookRotation(dir);

			goalQuaRot_ =
				playerRotY_;
		}

		// 移動アニメーション
		if (!isJump_ &&
			IsEndLanding() &&
			!isKiBlast_)
		{
			// ロックオン中
			if (isLockOn_)
			{
				// 後退
				if (ins.IsNew(KEY_INPUT_S) &&
					!ins.IsNew(KEY_INPUT_W))
				{
					if (ins.IsNew(KEY_INPUT_RSHIFT))
					{
						animationController_->Play(
							(int)ANIM_TYPE::LOCK_BACK_RUN
						);
					}
					else
					{
						animationController_->Play(
							(int)ANIM_TYPE::LOCK_BACK
						);
					}
				}
				// 左移動
				else if (ins.IsNew(KEY_INPUT_A) &&
					!ins.IsNew(KEY_INPUT_D))
				{
					if (ins.IsNew(KEY_INPUT_RSHIFT))
					{
						animationController_->Play(
							(int)ANIM_TYPE::LOCK_LEFT_RUN
						);
					}
					else
					{
						animationController_->Play(
							(int)ANIM_TYPE::LOCK_LEFT
						);
					}
				}
				// 右移動
				else if (ins.IsNew(KEY_INPUT_D) &&
					!ins.IsNew(KEY_INPUT_A))
				{
					if (ins.IsNew(KEY_INPUT_RSHIFT))
					{
						animationController_->Play(
							(int)ANIM_TYPE::LOCK_RIGHT_RUN
						);
					}
					else
					{
						animationController_->Play(
							(int)ANIM_TYPE::LOCK_RIGHT
						);
					}
				}
				// Wは今まで通り
				else
				{
					if (ins.IsNew(KEY_INPUT_RSHIFT))
					{
						animationController_->Play(
							(int)ANIM_TYPE::FAST_RUN
						);
					}
					else
					{
						animationController_->Play(
							(int)ANIM_TYPE::RUN
						);
					}
				}
			}
			// ロックオンしていない時
			else
			{
				if (ins.IsNew(KEY_INPUT_RSHIFT))
				{
					animationController_->Play(
						(int)ANIM_TYPE::FAST_RUN
					);
				}
				else
				{
					animationController_->Play(
						(int)ANIM_TYPE::RUN
					);
				}
			}
		}
	}
	else
	{
		if (!isJump_ &&
			IsEndLanding() &&
			attackEndTimer_ <= 0.0f &&
			!isKiBlast_)
		{
			animationController_->Play(
				(int)ANIM_TYPE::IDLE
			);
		}
	}
}

void Player::ProcessJump(void)
{

	bool isHit = CheckHitKey(KEY_INPUT_BACKSLASH);

	if (isKamehame_)
	{
		return;
	}

	// ジャンプ
	if (isHit && (isJump_ || IsEndLanding()))
	{

		if (!isJump_)
		{
			// 制御無しジャンプ
			//mAnimationController->Play((int)ANIM_TYPE::JUMP);
			// ループしないジャンプ
			//mAnimationController->Play((int)ANIM_TYPE::JUMP, false);
			// 切り取りアニメーション
			//mAnimationController->Play((int)ANIM_TYPE::JUMP, false, 13.0f, 24.0f);
			// 無理やりアニメーション
			animationController_->Play((int)ANIM_TYPE::JUMP, true, 13.0f, 25.0f);
			animationController_->SetEndLoop(23.0f, 25.0f, 5.0f);
		}

		isJump_ = true;

		// ジャンプの入力受付時間をヘラス
		stepJump_ += scnMng_.GetDeltaTime();
		if (stepJump_ < TIME_JUMP_IN)
		{
			jumpPow_ = VScale(AsoUtility::DIR_U, POW_JUMP);
		}

	}

	// ボタンを離したらジャンプ力に加算しない
	if (!isHit)
	{
		stepJump_ = TIME_JUMP_IN;
	}

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

	stepRotTime_ -= scnMng_.GetDeltaTime();

	// 回転の球面補間
	playerRotY_ = Quaternion::Slerp(
		playerRotY_, goalQuaRot_, (TIME_ROT - stepRotTime_) / TIME_ROT);

}

void Player::Collision(void)
{

	// 現在座標を起点に移動後座標を決める
	movedPos_ = VAdd(transform_.pos, movePow_);

	// 衝突(カプセル)
	CollisionCapsule();

	// 衝突(重力)
	CollisionGravity();

	// 移動
	transform_.pos = movedPos_;

}

void Player::CollisionGravity(void)
{

	// ジャンプ量を加算
	movedPos_ = VAdd(movedPos_, jumpPow_);

	// 重力方向
	VECTOR dirGravity = AsoUtility::DIR_D;

	// 重力方向の反対
	VECTOR dirUpGravity = AsoUtility::DIR_U;

	// 重力の強さ
	float gravityPow = Planet::DEFAULT_GRAVITY_POW;

	float checkPow = 10.0f;
	gravHitPosUp_ = VAdd(movedPos_, VScale(dirUpGravity, gravityPow));
	gravHitPosUp_ = VAdd(gravHitPosUp_, VScale(dirUpGravity, checkPow * 2.0f));
	gravHitPosDown_ = VAdd(movedPos_, VScale(dirGravity, checkPow));
	for (const auto c : colliders_)
	{

		// 地面との衝突
		auto hit = MV1CollCheck_Line(
			c.lock()->modelId_, -1, gravHitPosUp_, gravHitPosDown_);

		// 最初は上の行のように実装して、木の上に登ってしまうことを確認する
		//if (hit.HitFlag > 0)
		if (hit.HitFlag > 0 && VDot(dirGravity, jumpPow_) > 0.9f)
		{

			// 衝突地点から、少し上に移動
			movedPos_ = VAdd(hit.HitPosition, VScale(dirUpGravity, 2.0f));

			// ジャンプリセット
			jumpPow_ = AsoUtility::VECTOR_ZERO;
			stepJump_ = 0.0f;

			if (isJump_)
			{
				// 着地モーション
				animationController_->Play(
					(int)ANIM_TYPE::JUMP, false, 29.0f, 45.0f, false, true);
			}

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

	// カプセルとの衝突判定
	for (const auto c : colliders_)
	{

		auto hits = MV1CollCheck_Capsule(
			c.lock()->modelId_, -1,
			cap.GetPosTop(), cap.GetPosDown(), cap.GetRadius());

		for (int i = 0; i < hits.HitNum; i++)
		{

			auto hit = hits.Dim[i];

			for (int tryCnt = 0; tryCnt < 10; tryCnt++)
			{

				int pHit = HitCheck_Capsule_Triangle(
					cap.GetPosTop(), cap.GetPosDown(), cap.GetRadius(),
					hit.Position[0], hit.Position[1], hit.Position[2]);

				if (pHit)
				{
					movedPos_ = VAdd(movedPos_, VScale(hit.Normal, 1.0f));
					// カプセルを移動させる
					trans.pos = movedPos_;
					trans.Update();
					continue;
				}

				break;

			}

		}

		// 検出した地面ポリゴン情報の後始末
		MV1CollResultPolyDimTerminate(hits);

	}

}

void Player::CalcGravityPow(void)
{
	// 重力方向
	VECTOR dirGravity = AsoUtility::DIR_D;

	// 重力の強さ
	float gravityPow = Planet::DEFAULT_GRAVITY_POW;

	// 重力
	VECTOR gravity = VScale(dirGravity, gravityPow);
	jumpPow_ = VAdd(jumpPow_, gravity);

	// 最初は実装しない。地面と突き抜けることを確認する。
	// 内積
	float dot = VDot(dirGravity, jumpPow_);
	if (dot >= 0.0f)
	{
		// 重力方向と反対方向(マイナス)でなければ、ジャンプ力を無くす
		jumpPow_ = gravity;
	}

}

bool Player::IsEndLanding(void)
{
	bool ret = true;

	// アニメーションがジャンプではない
	if (animationController_->GetPlayType() != (int)ANIM_TYPE::JUMP)
	{
		return ret;
	}

	// アニメーションが終了しているか
	if (animationController_->IsEnd())
	{
		return ret;
	}

	return false;

}
void Player::StopRecord(void)
{
	isRecording_ = false;
}

const std::vector<std::unique_ptr<KiBlast>>&
Player::GetKiBlasts(void) const
{
	return kiBlasts_;
}

bool Player::IsAttack(void) const
{
	return isAttack_;
}

bool Player::IsAttackHitTiming(void) const
{
	float hitStart = 0.2f;
	float hitEnd = 0.3f;

	switch (combo_)
	{
	case 1:
		hitStart = 0.20f;
		hitEnd = 0.30f;
		break;

	case 2:
		hitStart = 0.30f;
		hitEnd = 0.40f;
		break;

	case 3:
		hitStart = 0.25f;
		hitEnd = 0.35f;
		break;

	case 4:
		hitStart = 0.30f;
		hitEnd = 0.40f;
		break;

	case 5:
		hitStart = 0.35f;
		hitEnd = 0.45f;
		break;

	case 6:
		hitStart = 0.30f;
		hitEnd = 0.40f;
		break;

	case 7:
		hitStart = 0.40f;
		hitEnd = 0.50f;
		break;

	case 8:
		hitStart = 0.50f;
		hitEnd = 0.60f;
		break;
	}

	return isAttack_ &&
		attackTimer_ >= hitStart &&
		attackTimer_ <= hitEnd;
}

bool Player::HasAttackHit(void) const
{
	return hasAttackHit_;
}

void Player::SetAttackHit(void)
{
	hasAttackHit_ = true;
}

VECTOR Player::GetForward(void) const
{
	return playerRotY_.GetForward();
}

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
	isAttack_ = false;
	combo_ = 0;
	nextAttack_ = false;
	attackTimer_ = 0.0f;
	hasAttackHit_ = false;

	// 気溜めを解除
	if (isCharging_ || isChargeEnding_)
	{
		isCharging_ = false;
		isChargeEnding_ = false;

		animationController_->ClearEndLoop();

		EffekseerEffect::GetInstance()->
			StopChargeEffect();
	}

	movePow_ =
		AsoUtility::VECTOR_ZERO;

	// ダメージ状態
	isDamage_ = true;
	damageTimer_ = 0.35f;

	animationController_->Play(
		(int)ANIM_TYPE::DAMAGE,
		false
	);
}

void Player::AddKnockBack(
	VECTOR dir,
	float power)
{
	if (VSize(dir) <= 0.001f)
	{
		return;
	}

	dir = VNorm(dir);

	knockBackPow_ =
		VScale(
			dir,
			power
		);

	// 攻撃してきた方向を向く
	VECTOR lookDir =
		VScale(
			dir,
			-1.0f
		);

	lookDir.y = 0.0f;

	if (VSize(lookDir) > 0.001f)
	{
		lookDir = VNorm(lookDir);

		playerRotY_ =
			Quaternion::LookRotation(
				lookDir
			);

		goalQuaRot_ =
			playerRotY_;

		transform_.quaRot =
			playerRotY_;
	}
}

bool Player::IsDead(void) const
{
	return isDead_;
}

int Player::GetHp(void) const
{
	return hp_;
}

bool Player::IsRecording(void) const
{
	return isRecording_;
}

void Player::SetLockOn(bool lockOn)
{
	isLockOn_ = lockOn;
}

bool Player::IsGuardBurst(void) const
{
	return isGuardBurst_;
}

bool Player::IsLockOn(void) const
{
	return isLockOn_;
}

void Player::LookAtTarget(VECTOR targetPos)
{
	VECTOR dir =
		VSub(
			targetPos,
			transform_.pos
		);

	if (VSize(dir) <= 0.001f)
	{
		return;
	}

	float horizontalDistance =
		sqrtf(
			dir.x * dir.x +
			dir.z * dir.z
		);

	lockOnPitch_ =
		-atan2f(
			dir.y,
			horizontalDistance
		);

	dir.y = 0.0f;

	if (VSize(dir) <= 0.001f)
	{
		return;
	}

	dir =
		VNorm(dir);

	playerRotY_ =
		Quaternion::LookRotation(dir);

	goalQuaRot_ =
		playerRotY_;
}

void Player::DrawKamehame(void)
{
	// かめはめ波を使っていない
	if (!isKamehame_)
	{
		if (kamehameLightHandle_ != -1)
		{
			SetLightEnableHandle(
				kamehameLightHandle_,
				false
			);
		}

		return;
	}

	// 手のボーンが見つかっていない
	if (leftHandFrame_ == -1 ||
		rightHandFrame_ == -1)
	{
		return;
	}

	// 左右の手の位置を取得
	VECTOR leftHandPos =
		MV1GetFramePosition(
			transform_.modelId,
			leftHandFrame_
		);

	VECTOR rightHandPos =
		MV1GetFramePosition(
			transform_.modelId,
			rightHandFrame_
		);

	// 左右の手の真ん中
	VECTOR chargePos =
	{
		(leftHandPos.x + rightHandPos.x) * 0.5f,
		(leftHandPos.y + rightHandPos.y) * 0.5f,
		(leftHandPos.z + rightHandPos.z) * 0.5f
	};

	// ライト
	if (kamehameLightHandle_ != -1)
	{
		SetLightEnableHandle(
			kamehameLightHandle_,
			true
		);

		VECTOR lightPos =
			transform_.pos;

		lightPos.y += 100.0f;

		VECTOR forward =
			GetForward();

		lightPos =
			VAdd(
				lightPos,
				VScale(forward, 120.0f)
			);

		SetLightPositionHandle(
			kamehameLightHandle_,
			lightPos
		);

		SetLightDifColorHandle(
			kamehameLightHandle_,
			GetColorF(
				1.0f,
				1.0f,
				1.0f,
				1.0f
			)
		);
	}

	// チャージ中
	if (!isKamehameBeam_)
	{
		float rate =
			kamehameTimer_ /
			KAMEHAME_SHOT_TIME;

		if (rate > 1.0f)
		{
			rate = 1.0f;
		}

		float scale =
			0.08f +
			rate * 0.14f;

		if (rate > 0.85f)
		{
			float burstRate =
				(rate - 0.85f) /
				0.15f;

			scale +=
				burstRate *
				0.06f;
		}

		MV1SetPosition(
			kamehameChargeModel_,
			chargePos
		);

		MV1SetScale(
			kamehameChargeModel_,
			{
				scale,
				scale,
				scale
			}
		);

		float rot =
			kamehameTimer_ * 3.0f;

		MV1SetRotationXYZ(
			kamehameChargeModel_,
			{
				rot * 0.5f,
				rot,
				0.0f
			}
		);

		MV1DrawModel(
			kamehameChargeModel_
		);

		return;
	}

	float baseScale = 0.18f;

	MV1SetPosition(
		kamehameChargeModel_,
		chargePos
	);

	MV1SetScale(
		kamehameChargeModel_,
		{
			baseScale,
			baseScale,
			baseScale
		}
	);

	float baseRot =
		kamehameTimer_ * 5.0f;

	MV1SetRotationXYZ(
		kamehameChargeModel_,
		{
			baseRot * 0.5f,
			baseRot,
			0.0f
		}
	);

	MV1DrawModel(
		kamehameChargeModel_
	);

	float beamTime =
		kamehameTimer_ -
		KAMEHAME_SHOT_TIME;

	if (beamTime < KAMEHAME_TRANSITION_TIME)
	{
		float t =
			beamTime /
			KAMEHAME_TRANSITION_TIME;

		if (t < 0.0f)
		{
			t = 0.0f;
		}

		if (t > 1.0f)
		{
			t = 1.0f;
		}

		float chargeScale =
			0.1f *
			(1.0f - t);

		MV1SetPosition(
			kamehameChargeModel_,
			chargePos
		);

		MV1SetScale(
			kamehameChargeModel_,
			{
				chargeScale,
				chargeScale,
				chargeScale
			}
		);

		MV1DrawModel(
			kamehameChargeModel_
		);
	}

	VECTOR forward =
		GetForward();

	if (isLockOn_ &&
		hasAttackTarget_)
	{
		forward =
			VSub(
				attackTargetPos_,
				chargePos
			);

		if (VSize(forward) > 0.001f)
		{
			forward =
				VNorm(forward);
		}
	}


	VECTOR startPos =
		VAdd(
			chargePos,
			VScale(
				forward,
				20.0f
			)
		);

	// ビームモデルの位置
	MV1SetPosition(
		kamehameBeamModel_,
		startPos
	);

	float rotY =
		atan2f(
			forward.x,
			forward.z
		) + DX_PI_F;

	float horizontal =
		sqrtf(
			forward.x * forward.x +
			forward.z * forward.z
		);

	float rotX =
		atan2f(
			forward.y,
			horizontal
		);

	MV1SetRotationXYZ(
		kamehameBeamModel_,
		{
			rotX,
			rotY,
			0.0f
		}
	);

	// ビームサイズ
	MV1SetScale(
		kamehameBeamModel_,
		{
			0.5f,  // 太さ
			0.5f,  // 太さ
			1.0f    // 長さ
		}
	);

	MV1DrawModel(
		kamehameBeamModel_
	);
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
	if (leftHandFrame_ == -1 ||
		rightHandFrame_ == -1)
	{
		return transform_.pos;
	}

	VECTOR leftHandPos =
		MV1GetFramePosition(
			transform_.modelId,
			leftHandFrame_
		);

	VECTOR rightHandPos =
		MV1GetFramePosition(
			transform_.modelId,
			rightHandFrame_
		);

	VECTOR startPos =
	{
		(leftHandPos.x + rightHandPos.x) * 0.5f,
		(leftHandPos.y + rightHandPos.y) * 0.5f,
		(leftHandPos.z + rightHandPos.z) * 0.5f
	};

	return startPos;
}

VECTOR Player::GetKamehameEndPos(void) const
{
	VECTOR startPos =
		GetKamehameStartPos();

	VECTOR dir =
		GetForward();

	if (isLockOn_ &&
		hasAttackTarget_)
	{
		dir =
			VSub(
				attackTargetPos_,
				startPos
			);

		if (VSize(dir) > 0.001f)
		{
			dir =
				VNorm(dir);
		}
	}

	return VAdd(
		startPos,
		VScale(
			dir,
			KAMEHAME_BEAM_LENGTH
		)
	);
}

bool Player::IsKamehame(void) const
{
	return isKamehame_;
}

int Player::GetCombo(void) const
{
	return combo_;
}

void Player::SetAttackTarget(VECTOR pos)
{
	hasAttackTarget_ = true;
	attackTargetPos_ = pos;
}

void Player::ClearAttackTarget(void)
{
	hasAttackTarget_ = false;
}

void Player::SetCanChase(bool canChase)
{
	canChase_ = canChase;
}

bool Player::CanChase(void) const
{
	return canChase_;
}

bool Player::UseKi(float amount)
{
	if (ki_ < amount)
	{
		return false;
	}

	ki_ -= amount;

	return true;
}

void Player::UpdateAttack(void)
{
	auto& ins =
		InputManager::GetInstance();

	if (canChase_ &&
		!isAttack_ &&
		ins.IsTrgDown(KEY_INPUT_F))
	{
		isChasing_ = true;
		canChase_ = false;

		movePow_ =
			AsoUtility::VECTOR_ZERO;

		animationController_->Play(
			(int)ANIM_TYPE::BOOST_CHASE,
			true
		);

		return;
	}

	// 攻撃開始
	if (!isKamehame_ &&
		!isChasing_ &&
		ins.IsTrgDown(KEY_INPUT_F))
	{
		if (!isAttack_)
		{
			isAttack_ = true;
			combo_ = 1;
			nextAttack_ = false;

			attackTimer_ = 0.0f;
			hasAttackHit_ = false;
			attackTrigger_ = true;

			animationController_->Play(
				(int)ANIM_TYPE::ATTACK01,
				false
			);
		}
		else
		{
			nextAttack_ = true;
		}
	}

	// 攻撃中
	if (isAttack_)
	{
		attackTimer_ +=
			scnMng_.GetDeltaTime();

		movePow_ =
			AsoUtility::VECTOR_ZERO;

		if (hasAttackTarget_)
		{
			VECTOR dir =
				VSub(
					attackTargetPos_,
					transform_.pos
				);

			float distance =
				VSize(dir);

			if (distance > 0.001f)
			{
				dir =
					VNorm(dir);

				// 向きは横方向だけ
				VECTOR lookDir = dir;
				lookDir.y = 0.0f;

				if (VSize(lookDir) > 0.001f)
				{
					lookDir =
						VNorm(lookDir);

					playerRotY_ =
						Quaternion::LookRotation(
							lookDir
						);

					goalQuaRot_ =
						playerRotY_;
				}

				// 1～7段目は敵についていく
				if (combo_ >= 1 &&
					combo_ <= 7)
				{
					float stopDistance = 65.0f;
					float chaseSpeed = 12.0f;

					if (distance > stopDistance)
					{
						float moveDistance =
							distance - stopDistance;

						if (moveDistance > chaseSpeed)
						{
							moveDistance =
								chaseSpeed;
						}

						movePow_ =
							VScale(
								dir,
								moveDistance
							);
					}
				}
			}
		}

		float attackRate =
			animationController_->GetPlayRate();

		bool canNextAttack =
			attackRate >= 0.75f;

		if (animationController_->IsEnd() ||
			(nextAttack_ &&
				combo_ < 8 &&
				canNextAttack))
		{
			if (nextAttack_ &&
				combo_ < 8)
			{
				combo_++;
				nextAttack_ = false;

				attackTimer_ = 0.0f;
				hasAttackHit_ = false;
				attackTrigger_ = true;

				switch (combo_)
				{
				case 2:
					animationController_->Play(
						(int)ANIM_TYPE::ATTACK02,
						false,
						0.0f,
						-1.0f,
						false,
						true
					);

					break;
				case 3:
					animationController_->Play(
						(int)ANIM_TYPE::ATTACK03,
						false
					);
					break;

				case 4:
				{
					if (hasAttackTarget_)
					{
						VECTOR enemyPos =
							attackTargetPos_;

						VECTOR toEnemy =
							VSub(
								enemyPos,
								transform_.pos
							);

						toEnemy.y = 0.0f;

						if (VSize(toEnemy) > 0.001f)
						{
							VECTOR dir =
								VNorm(toEnemy);

							VECTOR sideDir =
								VGet(
									-dir.z,
									0.0f,
									dir.x
								);

							// 移動前の位置を残す
							afterImagePos_ =
								transform_.pos;

							afterImageMatrix_ =
								MV1GetMatrix(
									transform_.modelId
								);

							isAfterImage_ = true;
							afterImageTimer_ = 0.10f;

							// 4発目の攻撃時は敵の横に移動する
							transform_.pos =
								VAdd(
									enemyPos,
									VScale(
										sideDir,
										70.0f
									)
								);

							VECTOR lookDir =
								VSub(
									enemyPos,
									transform_.pos
								);

							lookDir.y = 0.0f;

							if (VSize(lookDir) > 0.001f)
							{
								lookDir =
									VNorm(lookDir);

								playerRotY_ =
									Quaternion::LookRotation(
										lookDir
									);

								goalQuaRot_ =
									playerRotY_;
							}
						}
					}

					movePow_ =
						AsoUtility::VECTOR_ZERO;

					animationController_->Play(
						(int)ANIM_TYPE::ATTACK04,
						false
					);

					break;
				}
				case 5:
					animationController_->Play(
						(int)ANIM_TYPE::ATTACK05,
						false
					);
					break;

				case 6:
				{
					if (hasAttackTarget_)
					{
						VECTOR enemyPos =
							attackTargetPos_;

						VECTOR toEnemy =
							VSub(
								enemyPos,
								transform_.pos
							);

						toEnemy.y = 0.0f;

						if (VSize(toEnemy) > 0.001f)
						{
							VECTOR dir =
								VNorm(toEnemy);

							VECTOR sideDir =
								VGet(
									dir.z,
									0.0f,
									-dir.x
								);

							// 移動前の姿を残す
							afterImageMatrix_ =
								MV1GetMatrix(
									transform_.modelId
								);

							isAfterImage_ = true;
							afterImageTimer_ = 0.10f;

							// 敵の反対側へ移動
							transform_.pos =
								VAdd(
									enemyPos,
									VScale(
										sideDir,
										70.0f
									)
								);

							// 敵の方を向く
							VECTOR lookDir =
								VSub(
									enemyPos,
									transform_.pos
								);

							lookDir.y = 0.0f;

							if (VSize(lookDir) > 0.001f)
							{
								lookDir =
									VNorm(lookDir);

								playerRotY_ =
									Quaternion::LookRotation(
										lookDir
									);

								goalQuaRot_ =
									playerRotY_;
							}
						}
					}

					movePow_ =
						AsoUtility::VECTOR_ZERO;

					animationController_->Play(
						(int)ANIM_TYPE::ATTACK06,
						false
					);

					break;
				}
				case 7:
				{
					if (hasAttackTarget_)
					{
						VECTOR enemyPos =
							attackTargetPos_;

						VECTOR dir =
							VSub(
								enemyPos,
								transform_.pos
							);

						if (VSize(dir) > 0.001f)
						{
							dir = VNorm(dir);

							// 移動前の姿を残す
							afterImageMatrix_ =
								MV1GetMatrix(
									transform_.modelId
								);

							isAfterImage_ = true;
							afterImageTimer_ = 0.10f;

							// 敵の後ろへ移動
							transform_.pos =
								VAdd(
									enemyPos,
									VScale(
										dir,
										70.0f
									)
								);

							VECTOR lookDir =
								VSub(
									enemyPos,
									transform_.pos
								);

							lookDir.y = 0.0f;

							if (VSize(lookDir) > 0.001f)
							{
								lookDir = VNorm(lookDir);

								playerRotY_ =
									Quaternion::LookRotation(
										lookDir
									);

								goalQuaRot_ =
									playerRotY_;
							}
						}
					}

					movePow_ =
						AsoUtility::VECTOR_ZERO;

					animationController_->Play(
						(int)ANIM_TYPE::ATTACK07,
						false
					);

					break;
				}
				case 8:
				{
					if (hasAttackTarget_)
					{
						VECTOR enemyPos =
							attackTargetPos_;

						// 敵までの横方向
						VECTOR dir =
							VSub(
								enemyPos,
								transform_.pos
							);

						dir.y = 0.0f;

						if (VSize(dir) > 0.001f)
						{
							dir =
								VNorm(dir);

							// 移動前の姿を残す
							afterImageMatrix_ =
								MV1GetMatrix(
									transform_.modelId
								);

							isAfterImage_ = true;
							afterImageTimer_ = 0.10f;

							// 敵の斜め上へ移動
							transform_.pos =
								VAdd(
									enemyPos,
									VScale(
										dir,
										-130.0f
									)
								);

							transform_.pos.y += 200.0f;

							// 敵を見る
							VECTOR lookDir =
								VSub(
									enemyPos,
									transform_.pos
								);

							float horizontalDistance =
								sqrtf(
									lookDir.x * lookDir.x +
									lookDir.z * lookDir.z
								);

							lockOnPitch_ =
								-atan2f(
									lookDir.y,
									horizontalDistance
								);

							lookDir.y = 0.0f;

							if (VSize(lookDir) > 0.001f)
							{
								lookDir =
									VNorm(lookDir);

								playerRotY_ =
									Quaternion::LookRotation(
										lookDir
									);

								goalQuaRot_ =
									playerRotY_;
							}
						}
					}

					movePow_ =
						AsoUtility::VECTOR_ZERO;

					animationController_->Play(
						(int)ANIM_TYPE::ATTACK08,
						false
					);

					break;
				}
				}

			}
			else
			{
				isAttack_ = false;
				nextAttack_ = false;
				combo_ = 0;

				attackTimer_ = 0.0f;
				hasAttackHit_ = false;

				isAttack04Move_ = false;
				attack04MoveTimer_ = 0.0f;

				animationController_->Play(
					(int)ANIM_TYPE::IDLE
				);
			}
		}
	}

	if (attackEndTimer_ > 0.0f)
	{
		attackEndTimer_ -=
			scnMng_.GetDeltaTime();

		if (attackEndTimer_ < 0.0f)
		{
			attackEndTimer_ = 0.0f;
		}
	}
}

void Player::UpdateKamehame(void)
{
	auto& ins =
		InputManager::GetInstance();

	// かめはめ波開始
	if (!isAttack_ &&
		!isKamehame_ &&
		!isJump_ &&
		ins.IsTrgDown(KEY_INPUT_R))
	{
		if (UseKi(KAMEHAME_KI_COST))
		{
			isKamehame_ = true;
			isKamehameBeam_ = false;

			kamehameTimer_ = 0.0f;

			movePow_ =
				AsoUtility::VECTOR_ZERO;

			animationController_->Play(
				(int)ANIM_TYPE::KAMEHAME,
				false
			);
		}
	}

	if (isKamehame_)
	{
		kamehameTimer_ +=
			scnMng_.GetDeltaTime();

		if (kamehameTimer_ >=
			KAMEHAME_SHOT_TIME)
		{
			isKamehameBeam_ = true;
		}

		if (kamehameTimer_ >=
			KAMEHAME_END_TIME)
		{
			isKamehame_ = false;
			isKamehameBeam_ = false;
			kamehameTimer_ = 0.0f;

			animationController_->Play(
				(int)ANIM_TYPE::IDLE
			);
		}
	}
}

void Player::UpdateChase(void)
{
	auto& ins =
		InputManager::GetInstance();

	// 気溜め開始
	if (!isCharging_ &&
		!isChargeEnding_ &&
		ins.IsTrgDown(KEY_INPUT_T))
	{
		isCharging_ = true;

		movePow_ =
			AsoUtility::VECTOR_ZERO;

		animationController_->Play(
			(int)ANIM_TYPE::CHARGE,
			true,
			0.0f,
			45.0f
		);

		animationController_->SetEndLoop(
			40.0f,
			45.0f,
			5.0f
		);

		EffekseerEffect::GetInstance()->
			PlayChargeEffect(
				transform_.pos
			);
	}

	// 気溜め中
	if (isCharging_)
	{
		movePow_ =
			AsoUtility::VECTOR_ZERO;

		EffekseerEffect::GetInstance()->
			UpdateChargeEffect(
				transform_.pos
			);

		ki_ +=
			KI_CHARGE_SPEED *
			scnMng_.GetDeltaTime();

		if (ki_ >= MAX_KI)
		{
			ki_ = MAX_KI;

			isCharging_ = false;
			isChargeEnding_ = true;

			EffekseerEffect::GetInstance()->
				StopChargeEffect();

			animationController_->
				ClearEndLoop();

			animationController_->Play(
				(int)ANIM_TYPE::CHARGE,
				false,
				45.0f,
				70.0f,
				false,
				true
			);

			return;
		}

		bool cancelCharge =
			ins.IsNew(KEY_INPUT_W) ||
			ins.IsNew(KEY_INPUT_A) ||
			ins.IsNew(KEY_INPUT_S) ||
			ins.IsNew(KEY_INPUT_D) ||
			ins.IsNew(KEY_INPUT_RSHIFT) ||
			ins.IsTrgDown(KEY_INPUT_F) ||
			ins.IsTrgDown(KEY_INPUT_R) ||
			ins.IsNew(KEY_INPUT_BACKSLASH);

		if (cancelCharge)
		{
			isCharging_ = false;
			isChargeEnding_ = false;

			animationController_->
				ClearEndLoop();

			EffekseerEffect::GetInstance()->
				StopChargeEffect();

			animationController_->Play(
				(int)ANIM_TYPE::IDLE
			);
		}
		else if (!ins.IsNew(KEY_INPUT_T))
		{
			isCharging_ = false;
			isChargeEnding_ = true;

			EffekseerEffect::GetInstance()->
				StopChargeEffect();

			animationController_->
				ClearEndLoop();

			animationController_->Play(
				(int)ANIM_TYPE::CHARGE,
				false,
				45.0f,
				70.0f,
				false,
				true
			);

			return;
		}
		else
		{
			return;
		}
	}

	// 気溜め終了
	if (isChargeEnding_)
	{
		movePow_ =
			AsoUtility::VECTOR_ZERO;

		if (animationController_->IsEnd())
		{
			isChargeEnding_ = false;

			animationController_->Play(
				(int)ANIM_TYPE::IDLE
			);
		}

		return;
	}
}

void Player::UpdateCharge(void)
{
	if (!isChasing_)
	{
		return;
	}

	movePow_ =
		AsoUtility::VECTOR_ZERO;

	if (!hasAttackTarget_)
	{
		isChasing_ = false;
		canChase_ = false;
		return;
	}

	VECTOR dir =
		VSub(
			attackTargetPos_,
			transform_.pos
		);

	float distance =
		VSize(dir);

	if (distance <= 80.0f)
	{
		isChasing_ = false;
		canChase_ = false;

		isAttack_ = true;
		combo_ = 1;
		nextAttack_ = false;

		attackTimer_ = 0.0f;
		hasAttackHit_ = false;
		attackTrigger_ = true;

		animationController_->Play(
			(int)ANIM_TYPE::ATTACK01,
			false
		);

		return;
	}

	if (distance <= 0.001f)
	{
		return;
	}

	dir = VNorm(dir);

	VECTOR lookDir = dir;
	lookDir.y = 0.0f;

	if (VSize(lookDir) > 0.001f)
	{
		lookDir = VNorm(lookDir);

		playerRotY_ =
			Quaternion::LookRotation(
				lookDir
			);

		goalQuaRot_ =
			playerRotY_;
	}

	float moveDistance =
		distance - 80.0f;

	if (moveDistance > 45.0f)
	{
		moveDistance = 45.0f;
	}

	movePow_ =
		VScale(
			dir,
			moveDistance
		);
}

void Player::UpdateKiBlast(void)
{
	auto& ins =
		InputManager::GetInstance();

	// 気弾開始
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

		animationController_->Play(
			(int)ANIM_TYPE::KI_BLAST,
			false
		);
	}

	// 気弾モーション中
	if (isKiBlast_)
	{
		kiBlastTimer_ +=
			scnMng_.GetDeltaTime();

		// 手を前に出したあたりで発射
		if (!isKiBlastShot_ &&
			kiBlastTimer_ >= 0.20f)
		{
			VECTOR shotPos =
				transform_.pos;

			if (rightHandFrame_ != -1)
			{
				shotPos =
					MV1GetFramePosition(
						transform_.modelId,
						rightHandFrame_
					);
			}

			VECTOR shotDir =
				GetForward();

			// ロックオンしている敵がいる場合
			if (isLockOn_ &&
				hasAttackTarget_)
			{
				shotDir =
					VSub(
						attackTargetPos_,
						shotPos
					);

				if (VSize(shotDir) > 0.001f)
				{
					shotDir =
						VNorm(shotDir);
				}
			}

			kiBlasts_.push_back(
				std::make_unique<KiBlast>(
					shotPos,
					shotDir
				)
			);

			isKiBlastShot_ = true;
		}

		// 気弾終了
		if (kiBlastTimer_ >= 0.45f)
		{
			isKiBlast_ = false;
			isKiBlastShot_ = false;
			kiBlastTimer_ = 0.0f;

			// 移動していない時だけIDLE
			if (!ins.IsNew(KEY_INPUT_W) &&
				!ins.IsNew(KEY_INPUT_A) &&
				!ins.IsNew(KEY_INPUT_S) &&
				!ins.IsNew(KEY_INPUT_D))
			{
				animationController_->Play(
					(int)ANIM_TYPE::IDLE
				);
			}
		}
	}

	// 飛んでいる気弾を更新
	for (auto& blast : kiBlasts_)
	{
		blast->Update();
	}

	// 消えた気弾を削除
	for (auto it = kiBlasts_.begin();
		it != kiBlasts_.end();)
	{
		if ((*it)->IsDead())
		{
			it =
				kiBlasts_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void Player::UpdateBoostChase(void)
{
	auto& ins =
		InputManager::GetInstance();

	// 高速接近開始
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

		movePow_ =
			AsoUtility::VECTOR_ZERO;

		animationController_->Play(
			(int)ANIM_TYPE::BOOST_CHASE
		);
	}

	// 攻撃したら終了
	if (isBoostChase_ &&
		isAttack_)
	{
		isBoostChase_ = false;
		boostChaseTimer_ = 0.0f;

		return;
	}

	if (!isBoostChase_)
	{
		return;
	}

	boostChaseTimer_ +=
		scnMng_.GetDeltaTime();


	// 敵への方向
	VECTOR dir =
		VSub(
			attackTargetPos_,
			transform_.pos
		);

	float distance =
		VSize(dir);

	if (distance <= 0.001f)
	{
		isBoostChase_ = false;
		boostChaseTimer_ = 0.0f;

		return;
	}

	dir =
		VNorm(dir);


	// 敵の方を向く
	playerRotY_ =
		Quaternion::LookRotation(dir);

	goalQuaRot_ =
		playerRotY_;


	// 最初は一瞬その場で構える
	if (boostChaseTimer_ < 0.10f)
	{
		movePow_ =
			AsoUtility::VECTOR_ZERO;

		return;
	}


	// 敵の手前で停止
	if (distance <= 70.0f)
	{
		isBoostChase_ = false;
		boostChaseTimer_ = 0.0f;

		movePow_ =
			AsoUtility::VECTOR_ZERO;

		animationController_->Play(
			(int)ANIM_TYPE::IDLE
		);

		return;
	}


	// 加速
	float boostSpeed = 45.0f;

	if (boostChaseTimer_ < 0.20f)
	{
		float rate =
			(boostChaseTimer_ - 0.10f) /
			0.10f;

		boostSpeed =
			45.0f * rate;
	}


	// 敵を通り抜けないようにする
	float moveDistance =
		distance - 70.0f;

	if (moveDistance > boostSpeed)
	{
		moveDistance =
			boostSpeed;
	}

	movePow_ =
		VScale(
			dir,
			moveDistance
		);


	// 高速移動中の残像
	boostAfterImageTimer_ -=
		scnMng_.GetDeltaTime();

	if (boostAfterImageTimer_ <= 0.0f)
	{
		BoostAfterImage image;

		image.matrix =
			MV1GetMatrix(
				transform_.modelId
			);

		image.timer = 0.15f;

		boostAfterImages_.push_back(
			image
		);

		boostAfterImageTimer_ = 0.03f;
	}
}

void Player::UpdateDodge(void)
{
	auto& ins =
		InputManager::GetInstance();

	if (!isDodge_)
	{
		if (isAttack_ ||
			isBoostChase_ ||
			isKamehame_)
		{
			return;
		}

		if (ins.IsTrgDown(KEY_INPUT_LSHIFT))
		{
			VECTOR forward;
			VECTOR right;

			// ロックオン中
			if (isLockOn_ &&
				hasAttackTarget_)
			{
				forward =
					VSub(
						attackTargetPos_,
						transform_.pos
					);

				forward.y = 0.0f;

				if (VSize(forward) <= 0.001f)
				{
					return;
				}

				forward =
					VNorm(forward);

				right =
					VGet(
						forward.z,
						0.0f,
						-forward.x
					);
			}
			else
			{
				Quaternion cameraRot =
					mainCamera.GetQuaRotOutX();

				forward =
					cameraRot.GetForward();

				right =
					cameraRot.GetRight();

				forward.y = 0.0f;
				right.y = 0.0f;

				if (VSize(forward) > 0.001f)
				{
					forward =
						VNorm(forward);
				}

				if (VSize(right) > 0.001f)
				{
					right =
						VNorm(right);
				}
			}
			dodgeDir_ =
				AsoUtility::VECTOR_ZERO;

			if (ins.IsNew(KEY_INPUT_W))
			{
				dodgeDir_ =
					forward;
			}
			else if (ins.IsNew(KEY_INPUT_S))
			{
				dodgeDir_ =
					VScale(
						forward,
						-1.0f
					);
			}
			else if (ins.IsNew(KEY_INPUT_A))
			{
				dodgeDir_ =
					VScale(
						right,
						-1.0f
					);
			}
			else if (ins.IsNew(KEY_INPUT_D))
			{
				dodgeDir_ =
					right;
			}
			else
			{
				// 方向入力なしなら後ろへ回避
				dodgeDir_ =
					VScale(
						forward,
						-1.0f
					);
			}

			isDodge_ = true;
			dodgeTimer_ = 0.0f;
		}
	}

	if (!isDodge_)
	{
		return;
	}

	dodgeTimer_ +=
		scnMng_.GetDeltaTime();

	movePow_ =
		VScale(
			dodgeDir_,
			12.0f
		);

	// 敵の方向は向いたまま
	if (isLockOn_ &&
		hasAttackTarget_)
	{
		VECTOR dir =
			VSub(
				attackTargetPos_,
				transform_.pos
			);

		if (VSize(dir) > 0.001f)
		{
			dir =
				VNorm(dir);

			playerRotY_ =
				Quaternion::LookRotation(dir);

			goalQuaRot_ =
				playerRotY_;
		}
	}

	if (dodgeTimer_ >= 0.18f)
	{
		isDodge_ = false;
		dodgeTimer_ = 0.0f;

		movePow_ =
			AsoUtility::VECTOR_ZERO;
	}
}

void Player::UpdateGuard(void)
{
	InputManager& ins =
		InputManager::GetInstance();

	float deltaTime =
		SceneManager::GetInstance().GetDeltaTime();

	// ガードブレイク中
	if (isGuardBreak_)
	{
		guardBreakTimer_ -= deltaTime;

		movePow_ =
			AsoUtility::VECTOR_ZERO;

		if (guardBreakTimer_ <= 0.0f)
		{
			guardBreakTimer_ = 0.0f;
			isGuardBreak_ = false;

			guardHp_ = 100.0f;

			animationController_->Play(
				(int)ANIM_TYPE::IDLE,
				true,
				0.0f,
				-1.0f,
				false,
				true
			);
		}

		return;
	}

	// ガード耐久値の回復
	if (guardHp_ < 100.0f)
	{
		if (guardRecoverTimer_ > 0.0f)
		{
			guardRecoverTimer_ -= deltaTime;
		}
		else
		{
			guardHp_ += 25.0f * deltaTime;

			if (guardHp_ > 100.0f)
			{
				guardHp_ = 100.0f;
			}
		}
	}

	// ガード開始
	if (!isGuard_ &&
		!isGuardBurst_ &&
		ins.IsNew(KEY_INPUT_L))
	{
		isGuard_ = true;

		movePow_ =
			AsoUtility::VECTOR_ZERO;

		animationController_->Play(
			(int)ANIM_TYPE::GUARD,
			false,
			0.0f,
			-1.0f,
			true
		);
	}
	// ガード解除
	else if (isGuard_ &&
		!ins.IsNew(KEY_INPUT_L))
	{
		isGuard_ = false;

		animationController_->Play(
			(int)ANIM_TYPE::IDLE
		);
	}
}

void Player::UpdateGuardBurst(void)
{
	auto& ins =
		InputManager::GetInstance();

	float deltaTime =
		SceneManager::GetInstance().GetDeltaTime();

	guardBurstTrigger_ = false;

	// バースト中
	if (isGuardBurst_)
	{
		guardBurstTimer_ -= deltaTime;

		movePow_ =
			AsoUtility::VECTOR_ZERO;

		if (guardBurstTimer_ <= 0.0f)
		{
			guardBurstTimer_ = 0.0f;
			isGuardBurst_ = false;

			animationController_->Play(
				(int)ANIM_TYPE::IDLE,
				true,
				0.0f,
				-1.0f,
				false,
				true
			);
		}

		return;
	}

	// ガード中にBでバースト
	if (isGuard_ &&
		ins.IsTrgDown(KEY_INPUT_B))
	{
		// 気が足りない場合は発動しない
		if (!UseKi(20.0f))
		{
			return;
		}

		isGuardBurst_ = true;
		guardBurstTrigger_ = true;
		guardBurstTimer_ = 0.5f;

		isGuard_ = false;

		movePow_ =
			AsoUtility::VECTOR_ZERO;

		animationController_->ClearEndLoop();

		animationController_->Play(
			(int)ANIM_TYPE::GUARD_BURST,
			false,
			0.0f,
			45.0f,
			false,
			true
		);

		return;
	}
}

bool Player::IsDodging(void) const
{
	return isDodge_;
}

bool Player::IsGuard(void) const
{
	return isGuard_;
}

void Player::GuardDamage(float damage)
{
	if (isGuardBreak_)
	{
		return;
	}

	// 回復開始までの時間をリセット
	guardRecoverTimer_ = 2.0f;

	guardHp_ -= damage;

	if (guardHp_ <= 0.0f)
	{
		guardHp_ = 0.0f;

		isGuard_ = false;
		isGuardBreak_ = true;
		guardBreakTimer_ = 3.0f;

		movePow_ =
			AsoUtility::VECTOR_ZERO;

		animationController_->Play(
			(int)ANIM_TYPE::GUARD_BREAK,
			true,
			0.0f,
			-1.0f,
			false,
			true
		);
	}
}

bool Player::IsGuardBreak(void) const
{
	return isGuardBreak_;
}

bool Player::IsGuardBurstTrigger(void) const
{
	return guardBurstTrigger_;
}