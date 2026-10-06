#include <DxLib.h>
#include <cmath>
#include "MeleeEnemy.h"
#include "../../Application.h"
#include "../Player.h"
#include "../../Manager/ResourceManager.h"
#include "../../Manager/SceneManager.h"
#include "../../Utility/AsoUtility.h"

//======================================================================
// 定数・ヘルパー(このcppの中だけで使う)
//======================================================================
namespace
{
	// 「ゼロ扱い」にする長さ
	constexpr float kEpsilon = 0.001f;

	// 高さの範囲
	constexpr float kMinHeight = 100.0f;
	constexpr float kMaxHeight = 2000.0f;

	// かめはめ波で押される速さ
	constexpr float kKamehamePushStartSpeed = 3.0f;
	constexpr float kKamehamePushAccel = 0.35f;
	constexpr float kKamehamePushMaxSpeed = 12.0f;

	// ノックバック
	constexpr float kKnockBackThreshold = 0.1f;
	constexpr float kKnockBackDamping = 0.85f;
	constexpr float kKnockBackDamageTime = 0.18f;

	// ダウン
	constexpr float kDownTime = 1.0f;

	// ガード
	constexpr float kGuardMaxHp = 100.0f;
	constexpr float kGuardStartDistance = 150.0f;	// この距離でプレイヤーが攻撃中ならガード
	constexpr float kGuardTime = 0.8f;
	constexpr float kGuardBreakTime = 2.0f;
	constexpr float kGuardCoolTime = 2.0f;

	// 攻撃
	constexpr int   kMaxCombo = 4;
	constexpr int   kFollowComboMax = 3;			// 1～3段目はプレイヤーに付いていく
	constexpr float kComboInterval = 0.55f;
	constexpr float kAttackDistance = 90.0f;
	constexpr float kAttackFollowRate = 0.25f;
	constexpr float kLookPitchRate = 0.35f;			// 見上げ・見下ろしの最大比率

	// 同期リング
	constexpr int   kRingSegment = 48;
	constexpr float kRingRadius = 80.0f;

	// AI(現在は無効)
	constexpr float kNormalBattleDistance = 280.0f;	// 普段戦う距離
	constexpr float kFarDistance = 650.0f;			// これ以上離れたら高速接近
	constexpr float kWaitTime = 0.8f;
	constexpr float kSideMoveTime = 1.0f;
	constexpr float kSideMoveSpeed = 2.0f;
	constexpr float kSideMoveCorrectRate = 0.08f;
	constexpr float kSideMoveMaxSpeed = 3.0f;
	constexpr float kVerticalSpeed = 2.0f;
	constexpr float kArriveDistance = 10.0f;
	constexpr float kMoveWaitTime = 0.5f;
	constexpr float kBoostStopDistance = 110.0f;
	constexpr float kAttackApproachSpeed = 5.0f;

	// Y成分を捨てる
	VECTOR ToHorizontal(VECTOR v)
	{
		v.y = 0.0f;
		return v;
	}

	// タイマーを減らして、0未満にならないようにする
	void CountDown(float& timer, float deltaTime)
	{
		if (timer > 0.0f)
		{
			timer -= deltaTime;

			if (timer < 0.0f)
			{
				timer = 0.0f;
			}
		}
	}

	// 水平方向の単位ベクトル。ほぼ真上・真下なら向いている方向を使う
	VECTOR HorizontalDirOrForward(VECTOR toTarget, const Quaternion& rot)
	{
		toTarget = ToHorizontal(toTarget);

		if (VSize(toTarget) > kEpsilon)
		{
			return VNorm(toTarget);
		}

		VECTOR forward = ToHorizontal(rot.GetForward());

		if (VSize(forward) <= kEpsilon)
		{
			forward = VGet(0.0f, 0.0f, 1.0f);
		}

		return VNorm(forward);
	}
}

MeleeEnemy::MeleeEnemy(Player& player)
	:
	player_(player),
	targetPos({ 0.0f, 0.0f, 0.0f }),
	moveSpeed_(2.0f),
	isBoostChase_(false),
	boostChaseTimer_(0.0f),
	stopRange_(80.0f),
	isAttack_(false),
	attackCombo_(0),
	attackTimer_(0.0f),
	attackCoolTimer_(0.0f),
	isAttackWait_(false),
	attackWaitTimer_(0.0f),
	hasAttackHit_(false),
	isDamage_(false),
	damageTimer_(0.0f),
	isGuard_(false),
	guardTimer_(0.0f),
	guardHp_(kGuardMaxHp),
	isGuardBreak_(false),
	guardBreakTimer_(0.0f),
	guardCoolTimer_(0.0f),
	isSlamDown_(false),
	isDown_(false),
	downTimer_(0.0f)
{
	hp_ = 100;
	hitRadius_ = 50.0f;
}

MeleeEnemy::~MeleeEnemy(void)
{
}

void MeleeEnemy::Init(void)
{
	// モデル
	transform_.SetModel(resMng_.LoadModelDuplicate(ResourceManager::SRC::ENEMY));

	InitAnimation();

	// 初期位置
	transform_.pos = { 300.0f, 1000.0f, 0.0f };
	transform_.scl = { 0.8f, 0.8f, 0.8f };

	transform_.quaRotLocal =
		Quaternion::Euler(0.0f, AsoUtility::Deg2RadF(180.0f), 0.0f);

	transform_.Update();

	InitHud("ENEMY", hp_);
}

void MeleeEnemy::InitAnimation(void)
{
	std::string path = Application::PATH_MODEL + "Enemy/Animation/";
	animationController_ = std::make_unique<AnimationController>(transform_.modelId);

	animationController_->Add((int)ANIM_TYPE::IDLE, path + "Idle.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::RUN, path + "Running.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::FAST_RUN, path + "Fast Run.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK01, path + "Attack01.mv1", 45.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK02, path + "Attack02.mv1", 45.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK03, path + "Attack03.mv1", 45.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK04, path + "Attack04.mv1", 45.0f);
	animationController_->Add((int)ANIM_TYPE::KAMEHAME, path + "かめはめ波.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::DAMAGE, path + "Damege.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::GUARD, path + "Block.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::GUARD_BREAK, path + "GuardBreak.mv1", 20.0f);

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

void MeleeEnemy::Update(void)
{
	if (isDead_)
	{
		return;
	}

	UpdateHud();

	const float deltaTime = SceneManager::GetInstance().GetDeltaTime();

	animationController_->Update();

	// 以下の関数は、今フレームの更新を終えるときに true を返す
	if (UpdateKamehamePush()) { return; }

	CountDown(guardCoolTimer_, deltaTime);
	CountDown(attackCoolTimer_, deltaTime);

	const VECTOR playerPos = player_.GetTransform().pos;
	const float distance = VSize(VSub(playerPos, transform_.pos));

	if (UpdateKnockBack()) { return; }
	if (UpdateDown(deltaTime)) { return; }
	if (UpdateDamage(deltaTime)) { return; }
	if (UpdateGuardBreak(deltaTime)) { return; }
	if (UpdateGuard(deltaTime)) { return; }
	if (TryStartGuard(distance)) { return; }
	if (UpdateAttack(deltaTime, playerPos)) { return; }

	// プレイヤーを見る
	LookAtPlayerWithPitchLimit(playerPos);

	// AI(現在は無効)。有効にするときは、次の2行のコメントを外す
	// const AIContext ai = { deltaTime, distance, playerPos, HorizontalDirOrForward(VSub(playerPos, transform_.pos), transform_.quaRot) };
	// UpdateAI(ai);

	transform_.Update();
}

// かめはめ波で押されている間の処理
bool MeleeEnemy::UpdateKamehamePush(void)
{
	if (!isKamehameHit_)
	{
		return false;
	}

	kamehamePushSpeed_ += kKamehamePushAccel;

	if (kamehamePushSpeed_ > kKamehamePushMaxSpeed)
	{
		kamehamePushSpeed_ = kKamehamePushMaxSpeed;
	}

	transform_.pos =
		VAdd(transform_.pos, VScale(kamehamePushDir_, kamehamePushSpeed_));

	transform_.Update();
	return true;
}

// ノックバック中の処理(叩き落とし中に地面へ着いたらダウンへ)
bool MeleeEnemy::UpdateKnockBack(void)
{
	if (VSize(knockBackPow_) <= kKnockBackThreshold)
	{
		knockBackPow_ = AsoUtility::VECTOR_ZERO;
		return false;
	}

	transform_.pos = VAdd(transform_.pos, knockBackPow_);

	// 下限(地面)
	if (transform_.pos.y <= kMinHeight)
	{
		transform_.pos.y = kMinHeight;

		// 叩き落とされて地面に着いたらダウン
		if (isSlamDown_)
		{
			isSlamDown_ = false;
			isDown_ = true;
			downTimer_ = kDownTime;

			knockBackPow_ = AsoUtility::VECTOR_ZERO;

			animationController_->Play((int)ANIM_TYPE::DAMAGE, false);

			transform_.Update();
			return true;
		}

		knockBackPow_.y = 0.0f;
	}

	// 上限
	if (transform_.pos.y > kMaxHeight)
	{
		transform_.pos.y = kMaxHeight;
		knockBackPow_.y = 0.0f;
	}

	knockBackPow_ = VScale(knockBackPow_, kKnockBackDamping);

	if (!isDamage_)
	{
		isDamage_ = true;
		damageTimer_ = kKnockBackDamageTime;

		animationController_->Play((int)ANIM_TYPE::DAMAGE, false);
	}

	transform_.Update();
	return true;
}

// ダウン中の処理
bool MeleeEnemy::UpdateDown(float deltaTime)
{
	if (!isDown_)
	{
		return false;
	}

	downTimer_ -= deltaTime;

	if (downTimer_ <= 0.0f)
	{
		downTimer_ = 0.0f;
		isDown_ = false;
		isDamage_ = false;
		damageTimer_ = 0.0f;

		ReturnToWait();
	}

	transform_.Update();
	return true;
}

// ダメージモーション中の処理
bool MeleeEnemy::UpdateDamage(float deltaTime)
{
	if (!isDamage_)
	{
		return false;
	}

	transform_.quaRot = damageRot_;

	damageTimer_ -= deltaTime;

	if (damageTimer_ <= 0.0f)
	{
		damageTimer_ = 0.0f;
		isDamage_ = false;

		ReturnToWait();
		return false;
	}

	transform_.Update();
	animationController_->Update();
	return true;
}

// ガードブレイク中の処理(終わったら続けて他の処理へ)
bool MeleeEnemy::UpdateGuardBreak(float deltaTime)
{
	if (!isGuardBreak_)
	{
		return false;
	}

	guardBreakTimer_ -= deltaTime;

	if (guardBreakTimer_ <= 0.0f)
	{
		guardBreakTimer_ = 0.0f;
		isGuardBreak_ = false;

		guardHp_ = kGuardMaxHp;
		guardCoolTimer_ = kGuardCoolTime;

		ReturnToWait();
		return false;
	}

	transform_.Update();
	return true;
}

// ガード中の処理(終わったら続けて他の処理へ)
bool MeleeEnemy::UpdateGuard(float deltaTime)
{
	if (!isGuard_)
	{
		return false;
	}

	guardTimer_ -= deltaTime;

	if (guardTimer_ <= 0.0f)
	{
		guardTimer_ = 0.0f;
		isGuard_ = false;

		ReturnToWait();
		return false;
	}

	transform_.Update();
	return true;
}

// プレイヤーが近くで攻撃していたらガードを始める
bool MeleeEnemy::TryStartGuard(float distance)
{
	const bool shouldGuard =
		distance <= kGuardStartDistance &&
		player_.GetCombo() > 0 &&
		guardCoolTimer_ <= 0.0f;

	if (!shouldGuard)
	{
		return false;
	}

	isGuard_ = true;
	guardTimer_ = kGuardTime;

	ResetAttack();
	CancelBoostChase();

	SetAIState(AI_STATE::WAIT);

	animationController_->Play((int)ANIM_TYPE::GUARD, true, 0.0f, -1.0f, true, true);

	transform_.Update();
	return true;
}

// 攻撃中の処理
bool MeleeEnemy::UpdateAttack(float deltaTime, const VECTOR& playerPos)
{
	if (!isAttack_)
	{
		return false;
	}

	attackTimer_ += deltaTime;

	// 1～3段目は攻撃中もプレイヤーについていく
	if (attackCombo_ >= 1 && attackCombo_ <= kFollowComboMax)
	{
		FollowPlayerWhileAttacking(playerPos);
	}

	// 次の攻撃へ
	if (attackTimer_ >= kComboInterval)
	{
		AdvanceCombo();
	}

	transform_.Update();
	return true;
}

// 攻撃中、プレイヤーから一定距離の位置へ近づきながら見る
void MeleeEnemy::FollowPlayerWhileAttacking(const VECTOR& playerPos)
{
	VECTOR horizontalDir =
		HorizontalDirOrForward(VSub(playerPos, transform_.pos), transform_.quaRot);

	VECTOR attackPos =
		VSub(playerPos, VScale(horizontalDir, kAttackDistance));

	VECTOR follow = VSub(attackPos, transform_.pos);

	transform_.pos =
		VAdd(transform_.pos, VScale(follow, kAttackFollowRate));

	LookAtPlayerWithPitchLimit(playerPos);
}

// 次の段の攻撃へ。最後まで終わったら待機に戻る
void MeleeEnemy::AdvanceCombo(void)
{
	attackTimer_ = 0.0f;
	hasAttackHit_ = false;

	attackCombo_++;

	if (attackCombo_ <= kMaxCombo)
	{
		// ATTACK01～04 は連番(2段目なら ATTACK02)
		const int animType = (int)ANIM_TYPE::ATTACK01 + (attackCombo_ - 1);
		animationController_->Play(animType, false);
		return;
	}

	// 攻撃終了
	ResetAttack();

	attackCoolTimer_ = ATTACK_COOL_TIME;

	ReturnToWait();
}

// プレイヤーを見る(上下の角度は制限する)
void MeleeEnemy::LookAtPlayerWithPitchLimit(const VECTOR& playerPos)
{
	VECTOR lookDir = VSub(playerPos, transform_.pos);

	float lookDistance = VSize(ToHorizontal(lookDir));

	if (lookDistance > kEpsilon)
	{
		float maxY = lookDistance * kLookPitchRate;

		if (lookDir.y > maxY) { lookDir.y = maxY; }
		if (lookDir.y < -maxY) { lookDir.y = -maxY; }
	}

	if (VSize(lookDir) > kEpsilon)
	{
		transform_.quaRot = Quaternion::LookRotation(VNorm(lookDir));
	}
}

// 状態の補助
void MeleeEnemy::SetAIState(AI_STATE state)
{
	aiState_ = state;
	aiTimer_ = 0.0f;
}

// 待機状態に戻って、IDLEアニメーションにする
void MeleeEnemy::ReturnToWait(void)
{
	SetAIState(AI_STATE::WAIT);

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

// 攻撃状態をリセット
void MeleeEnemy::ResetAttack(void)
{
	isAttack_ = false;
	attackCombo_ = 0;
	attackTimer_ = 0.0f;
	hasAttackHit_ = false;
}

void MeleeEnemy::CancelBoostChase(void)
{
	isBoostChase_ = false;
	boostChaseTimer_ = 0.0f;
}

// AI
void MeleeEnemy::UpdateAI(const AIContext& ctx)
{
	switch (aiState_)
	{
	case AI_STATE::WAIT:
		UpdateAIWait(ctx);
		break;

	case AI_STATE::SIDE_MOVE:
		UpdateAISideMove(ctx);
		break;

	case AI_STATE::MOVE:
		UpdateAIMove(ctx);
		break;

	case AI_STATE::BOOST_ATTACK:
		UpdateAIBoostAttack(ctx);
		break;

	case AI_STATE::ATTACK:
		UpdateAIAttack(ctx);
		break;
	}
}

// 様子を見る
void MeleeEnemy::UpdateAIWait(const AIContext& ctx)
{
	aiTimer_ += ctx.deltaTime;

	animationController_->Play((int)ANIM_TYPE::IDLE);

	if (aiTimer_ < kWaitTime)
	{
		return;
	}

	aiTimer_ = 0.0f;

	if (ctx.distance >= kFarDistance)
	{
		aiState_ = AI_STATE::BOOST_ATTACK;
	}
	else
	{
		aiState_ = AI_STATE::SIDE_MOVE;
		sideMoveDir_ *= -1.0f;
	}
}

// 横移動
void MeleeEnemy::UpdateAISideMove(const AIContext& ctx)
{
	aiTimer_ += ctx.deltaTime;

	VECTOR sideDir = VGet(ctx.horizontalDir.z, 0.0f, -ctx.horizontalDir.x);
	sideDir = VScale(sideDir, sideMoveDir_);

	VECTOR normalBattlePos =
		VSub(ctx.playerPos, VScale(ctx.horizontalDir, kNormalBattleDistance));

	VECTOR distanceCorrection =
		ToHorizontal(VSub(normalBattlePos, transform_.pos));

	VECTOR move =
		VAdd(
			VScale(sideDir, kSideMoveSpeed),
			VScale(distanceCorrection, kSideMoveCorrectRate));

	if (VSize(move) > kSideMoveMaxSpeed)
	{
		move = VScale(VNorm(move), kSideMoveMaxSpeed);
	}

	transform_.pos = VAdd(transform_.pos, move);

	// 高さはゆっくり合わせる
	float heightDiff = ctx.playerPos.y - transform_.pos.y;

	if (heightDiff > kVerticalSpeed)
	{
		transform_.pos.y += kVerticalSpeed;
	}
	else if (heightDiff < -kVerticalSpeed)
	{
		transform_.pos.y -= kVerticalSpeed;
	}
	else
	{
		transform_.pos.y = ctx.playerPos.y;
	}

	animationController_->Play((int)ANIM_TYPE::RUN);

	if (aiTimer_ >= kSideMoveTime)
	{
		aiTimer_ = 0.0f;
		aiState_ = AI_STATE::MOVE;
	}
}

// 攻撃前に、普段戦う距離へ間合いを整える
void MeleeEnemy::UpdateAIMove(const AIContext& ctx)
{
	VECTOR normalBattlePos =
		VSub(ctx.playerPos, VScale(ctx.horizontalDir, kNormalBattleDistance));

	VECTOR moveDir = VSub(normalBattlePos, transform_.pos);
	float moveDistance = VSize(moveDir);

	if (moveDistance > kEpsilon)
	{
		moveDir = VNorm(moveDir);
	}

	if (moveDistance > kArriveDistance)
	{
		float moveAmount = moveDistance;

		if (moveAmount > moveSpeed_)
		{
			moveAmount = moveSpeed_;
		}

		transform_.pos = VAdd(transform_.pos, VScale(moveDir, moveAmount));

		animationController_->Play((int)ANIM_TYPE::RUN);
		return;
	}

	// 間合いが整ったので、少し待ってから攻撃か待機へ
	aiTimer_ += ctx.deltaTime;

	animationController_->Play((int)ANIM_TYPE::IDLE);

	if (aiTimer_ >= kMoveWaitTime)
	{
		aiTimer_ = 0.0f;

		aiState_ = (attackCoolTimer_ <= 0.0f) ? AI_STATE::ATTACK : AI_STATE::WAIT;
	}
}

// 高速接近(この行動を選んだ時だけ)
void MeleeEnemy::UpdateAIBoostAttack(const AIContext& ctx)
{
	if (!isBoostChase_)
	{
		isBoostChase_ = true;
		boostChaseTimer_ = 0.0f;

		animationController_->Play((int)ANIM_TYPE::FAST_RUN);
	}

	boostChaseTimer_ += ctx.deltaTime;

	VECTOR boostTarget =
		VSub(ctx.playerPos, VScale(ctx.horizontalDir, kBoostStopDistance));

	VECTOR boostDir = VSub(boostTarget, transform_.pos);
	float boostDistance = VSize(boostDir);

	if (boostDistance > kEpsilon)
	{
		boostDir = VNorm(boostDir);
	}

	// 近づいたか、時間切れなら攻撃へ
	if (boostDistance <= kArriveDistance ||
		boostChaseTimer_ >= BOOST_CHASE_TIME)
	{
		CancelBoostChase();

		aiState_ = AI_STATE::ATTACK;
		aiTimer_ = 0.0f;

		animationController_->Play((int)ANIM_TYPE::IDLE);
		return;
	}

	float boostMove = boostDistance;

	if (boostMove > BOOST_CHASE_SPEED)
	{
		boostMove = BOOST_CHASE_SPEED;
	}

	transform_.pos = VAdd(transform_.pos, VScale(boostDir, boostMove));

	animationController_->Play((int)ANIM_TYPE::FAST_RUN);
}

// 攻撃距離まで近づいて、4段攻撃を開始する
void MeleeEnemy::UpdateAIAttack(const AIContext& ctx)
{
	VECTOR attackPos =
		VSub(ctx.playerPos, VScale(ctx.horizontalDir, kAttackDistance));

	VECTOR attackMoveDir = VSub(attackPos, transform_.pos);
	float attackMoveDistance = VSize(attackMoveDir);

	if (attackMoveDistance > kEpsilon)
	{
		attackMoveDir = VNorm(attackMoveDir);
	}

	if (attackMoveDistance > kArriveDistance)
	{
		float moveAmount = attackMoveDistance;

		if (moveAmount > kAttackApproachSpeed)
		{
			moveAmount = kAttackApproachSpeed;
		}

		transform_.pos = VAdd(transform_.pos, VScale(attackMoveDir, moveAmount));

		animationController_->Play((int)ANIM_TYPE::RUN);
		return;
	}

	// クールタイム中なら待機へ戻る
	if (attackCoolTimer_ > 0.0f)
	{
		SetAIState(AI_STATE::WAIT);

		animationController_->Play((int)ANIM_TYPE::IDLE);
		return;
	}

	// 攻撃距離まで来たので攻撃開始
	isAttack_ = true;
	attackCombo_ = 1;
	attackTimer_ = 0.0f;
	hasAttackHit_ = false;

	isAttackWait_ = false;
	attackWaitTimer_ = 0.0f;

	animationController_->Play((int)ANIM_TYPE::ATTACK01, false);
}

// 描画
void MeleeEnemy::Draw(void)
{
	if (isDead_)
	{
		return;
	}

	// 敵本体
	SetUseLighting(FALSE);
	MV1DrawModel(transform_.modelId);
	SetUseLighting(TRUE);

	// デバッグ表示
	//const int white = GetColor(255, 255, 255);

	//DrawFormatString(20, 180, white, "Enemy HP : %d", hp_);
	//DrawFormatString(20, 200, white, "Attack:%d Combo:%d Timer:%.2f Cool:%.2f",
	//	isAttack_, attackCombo_, attackTimer_, attackCoolTimer_);

	// HPバー(2D)
	DrawHud();
}

// 足元に、残り時間で縮む同期リングを描く
void MeleeEnemy::DrawSyncRing(void)
{
	if (!IsSyncReady())
	{
		return;
	}

	VECTOR center = transform_.pos;

	// 足元より少し上
	center.y += 3.0f;

	// 1.0 → 0.0 (残り時間で円が縮む)
	float radius = kRingRadius * GetSyncRate();

	// index 番目の円周上の点
	auto ringPoint = [&](int index, float r, float yOffset) -> VECTOR
		{
			float angle =
				DX_TWO_PI_F *
				static_cast<float>(index) /
				static_cast<float>(kRingSegment);

			return VGet(
				center.x + cosf(angle) * r,
				center.y + yOffset,
				center.z + sinf(angle) * r);
		};

	// 半透明の塗りつぶし(中心から三角形を並べて円を作る)
	unsigned int fillColor = GetColor(80, 220, 255);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90);

	for (int i = 0; i < kRingSegment; i++)
	{
		VECTOR p1 = ringPoint(i, radius, 0.0f);
		VECTOR p2 = ringPoint(i + 1, radius, 0.0f);

		DrawTriangle3D(center, p2, p1, fillColor, true);
	}

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 255);

	// 外周の線
	float outlineRadius = radius + 3.0f;
	unsigned int lineColor = GetColor(180, 245, 255);

	for (int i = 0; i < kRingSegment; i++)
	{
		VECTOR p1 = ringPoint(i, outlineRadius, 0.5f);
		VECTOR p2 = ringPoint(i + 1, outlineRadius, 0.5f);

		DrawLine3D(p1, p2, lineColor);
	}
}

// 位置・向き
void MeleeEnemy::SetPosition(VECTOR pos)
{
	transform_.pos = pos;

	transform_.Update();
}

void MeleeEnemy::LookAtPlayer(void)
{
	VECTOR dir = ToHorizontal(VSub(player_.GetTransform().pos, transform_.pos));

	if (VSize(dir) <= kEpsilon)
	{
		return;
	}

	damageRot_ = Quaternion::LookRotation(VNorm(dir));

	transform_.quaRot = damageRot_;
}

// 攻撃
bool MeleeEnemy::IsAttackHitTiming(void) const
{
	return
		isAttack_ &&
		attackTimer_ >= ATTACK_HIT_START &&
		attackTimer_ <= ATTACK_HIT_END;
}

bool MeleeEnemy::HasAttackHit(void) const
{
	return hasAttackHit_;
}

void MeleeEnemy::SetAttackHit(void)
{
	hasAttackHit_ = true;
}

int MeleeEnemy::GetAttackCombo(void) const
{
	return attackCombo_;
}

// ガード
bool MeleeEnemy::IsGuard(void) const
{
	return isGuard_;
}

void MeleeEnemy::GuardDamage(float damage)
{
	if (!isGuard_ || isGuardBreak_)
	{
		return;
	}

	guardHp_ -= damage;

	if (guardHp_ > 0.0f)
	{
		return;
	}

	// ガードブレイク
	guardHp_ = 0.0f;

	isGuard_ = false;
	guardTimer_ = 0.0f;

	isGuardBreak_ = true;
	guardBreakTimer_ = kGuardBreakTime;

	ResetAttack();
	CancelBoostChase();

	animationController_->Play((int)ANIM_TYPE::GUARD_BREAK, false);
}

void MeleeEnemy::GuardBurst(void)
{
	// 攻撃・高速接近を中断
	ResetAttack();
	CancelBoostChase();

	// すぐに再攻撃しないようにする
	attackCoolTimer_ = ATTACK_COOL_TIME;
}

// プレイヤーからの攻撃による状態変化
void MeleeEnemy::StartSlamDown(void)
{
	isSlamDown_ = true;
	isDown_ = false;
	downTimer_ = 0.0f;

	isAttack_ = false;
	attackTimer_ = 0.0f;

	isDamage_ = false;
	damageTimer_ = 0.0f;
}

bool MeleeEnemy::IsDown(void) const
{
	return isDown_;
}

void MeleeEnemy::SetKamehameHit(bool hit, VECTOR dir)
{
	if (!hit)
	{
		isKamehameHit_ = false;
		kamehamePushDir_ = AsoUtility::VECTOR_ZERO;
		kamehamePushSpeed_ = 0.0f;
		return;
	}

	// 当たり始めた瞬間
	if (!isKamehameHit_)
	{
		kamehamePushSpeed_ = kKamehamePushStartSpeed;

		animationController_->Play((int)ANIM_TYPE::DAMAGE, true);
	}

	isKamehameHit_ = true;

	if (VSize(dir) > kEpsilon)
	{
		kamehamePushDir_ = VNorm(dir);
	}

	ResetAttack();
	CancelBoostChase();
}
