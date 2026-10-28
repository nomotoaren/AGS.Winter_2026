#include <DxLib.h>
#include <cmath>
#include <algorithm>
#include "MeleeEnemy.h"
#include "../../../Application.h"
#include "../../Player/Player.h"
#include "../../../Manager/Camera.h"
#include "../../../Manager/EffekseerEffect.h"
#include "../../../Manager/ResourceManager.h"
#include "../../../Manager/SceneManager.h"
#include "../../../Utility/AsoUtility.h"
#include "MeleeEnemy_Internal.h"

using namespace MeleeEnemyInternal;

MeleeEnemy::MeleeEnemy(Player& player)
	:
	EnemyBase(player),
	isBoostChase_(false),
	boostChaseTimer_(0.0f),
	isAttack_(false),
	attackCombo_(0),
	attackTimer_(0.0f),
	attackCoolTimer_(0.0f),
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
	// 基本のHP・大きさなどは EnemyData の表から決める(ステージごとのHPは EnemyFactory が上書きする)
	ApplyInfo(EnemyData::Get(EnemyData::TYPE::MELEE));
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
	transform_.pos = { 0.0f, 1000.0f, 300.0f };
	transform_.scl = { modelScale_, modelScale_, modelScale_ };

	transform_.quaRotLocal =
		Quaternion::Euler(0.0f, AsoUtility::Deg2RadF(180.0f), 0.0f);

	transform_.Update();

	InitHud(displayName_, hp_);
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

	// 撃った気弾は、敵が動けない間も飛び続ける
	UpdateBlasts(deltaTime);
	UpdateAreaBlasts(deltaTime);

	// 相殺勝負中は、その場から動かない(他の攻撃を受けても押されたりしない)
	if (clashLock_)
	{
		knockBackPow_ = AsoUtility::VECTOR_ZERO;
		animationController_->Update();
		transform_.Update();
		return;
	}

	animationController_->Update();

	CountDown(guardCoolTimer_, deltaTime);
	CountDown(attackCoolTimer_, deltaTime);
	CountDown(kiCoolTimer_, deltaTime);
	CountDown(rushCoolTimer_, deltaTime);
	CountDown(dodgeCoolTimer_, deltaTime);
	CountDown(invincibleTimer_, deltaTime);
	CountDown(bigCoolTimer_, deltaTime);
	CountDown(grabCoolTimer_, deltaTime);
	CountDown(chaseCoolTimer_, deltaTime);
	CountDown(beamCoolTimer_, deltaTime);
	CountDown(barrageCoolTimer_, deltaTime);
	CountDown(slamCoolTimer_, deltaTime);

	UpdateTrail(deltaTime);

	hoverTime_ += deltaTime;

	// 追撃の予定は、しばらくしたら消す
	if (pendingChase_)
	{
		pendingChaseTimer_ -= deltaTime;

		if (pendingChaseTimer_ <= 0.0f)
		{
			pendingChase_ = false;
		}
	}

	// スタンゲージの更新(スタンが終わると OnStunEnd が呼ばれる)
	const bool isStunned = UpdateStunGauge(deltaTime);

	// 以下の関数は、このフレームの更新を終えたときに true を返す
	if (UpdateKamehamePush()) { return; }

	UpdateCounterAndKi(deltaTime, isStunned);

	const VECTOR playerPos = player_.GetTransform().pos;
	const float distance = VSize(VSub(playerPos, transform_.pos));

	// プレイヤーが攻撃を始めた瞬間(コンボ数が 0 から増えたとき)だけ、反応を決める
	const int playerCombo = player_.GetCombo();
	const bool playerAttackStarted = (playerCombo > 0 && prevPlayerCombo_ == 0);
	prevPlayerCombo_ = playerCombo;

	UpdateGuardWatch(deltaTime, distance);

	if (isStunned) { UpdateStunned(); return; }
	if (UpdateKnockBack()) { return; }
	if (UpdateDown(deltaTime)) { return; }
	if (UpdateDamage(deltaTime)) { return; }
	if (UpdateGuardBreak(deltaTime)) { return; }
	if (UpdateGuard(deltaTime)) { return; }
	if (playerAttackStarted && TryReactToPlayerAttack(distance)) { return; }
	if (UpdateAttack(deltaTime, playerPos)) { return; }
	if (UpdateKiCharge(deltaTime, playerPos)) { return; }
	if (UpdateRush(deltaTime, playerPos)) { return; }
	if (UpdateBigCharge(deltaTime, playerPos)) { return; }
	if (UpdateGrab(deltaTime, playerPos)) { return; }
	if (UpdateChase(deltaTime, playerPos)) { return; }
	if (UpdateBeam(deltaTime, playerPos)) { return; }
	if (UpdateKiBarrage(deltaTime, playerPos)) { return; }
	if (UpdateSlam(deltaTime, playerPos)) { return; }

	// プレイヤーを見る
	LookAtPlayerWithPitchLimit(playerPos);

	// AI
	const AIContext ai =
	{
		deltaTime,
		distance,
		playerPos,
		HorizontalDirOrForward(VSub(playerPos, transform_.pos), transform_.quaRot)
	};
	UpdateAI(ai);

	transform_.Update();
}

// かめはめ波で押されている間の処理
bool MeleeEnemy::UpdateKamehamePush(void)
{
	if (!isKamehameHit_)
	{
		return false;
	}

	const float fs = FrameScale();

	kamehamePushSpeed_ += kKamehamePushAccel * fs;

	if (kamehamePushSpeed_ > kKamehamePushMaxSpeed)
	{
		kamehamePushSpeed_ = kKamehamePushMaxSpeed;
	}

	transform_.pos =
		VAdd(transform_.pos, VScale(kamehamePushDir_, kamehamePushSpeed_ * fs));

	transform_.Update();
	return true;
}

// ノックバック中の処理(叩き落とされた場合は、地面へ着いたらダウン)
bool MeleeEnemy::UpdateKnockBack(void)
{
	if (VSize(knockBackPow_) <= kKnockBackThreshold)
	{
		knockBackPow_ = AsoUtility::VECTOR_ZERO;
		armoredHit_ = false;
		return false;
	}

	const float fs = FrameScale();

	transform_.pos = VAdd(transform_.pos, VScale(knockBackPow_, fs));

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

	// 減衰も、fps が変わっても同じ減り方になるようにする
	knockBackPow_ = VScale(knockBackPow_, powf(kKnockBackDamping, fs));

	// ひるまない攻撃: 少し押されるだけで、行動は中断しない
	if (armoredHit_)
	{
		transform_.Update();
		return false;
	}

	if (isDamage_)
	{
		// 殴られている間は、怯みを延長する(動けない)
		damageTimer_ = kKnockBackDamageTime;
	}
	else
	{
		isDamage_ = true;
		damageTimer_ = kKnockBackDamageTime;

		// 殴られたら、気弾や突進の途中でも中断される
		CancelSpecials();

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

// ガードブレイク中の処理(終わったら続けて次の処理へ)
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

// ガード中の処理(終わったら続けて次の処理へ)
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

// スタン中の処理(動けない。押されて下がるだけ)
bool MeleeEnemy::UpdateStunned(void)
{
	if (VSize(knockBackPow_) > kKnockBackThreshold)
	{
		const float fs = FrameScale();

		transform_.pos = VAdd(transform_.pos, VScale(knockBackPow_, fs));

		if (transform_.pos.y < kMinHeight) { transform_.pos.y = kMinHeight; }
		if (transform_.pos.y > kMaxHeight) { transform_.pos.y = kMaxHeight; }

		knockBackPow_ = VScale(knockBackPow_, powf(kKnockBackDamping, fs));
	}
	else
	{
		knockBackPow_ = AsoUtility::VECTOR_ZERO;
	}

	transform_.Update();

	// スタン中のモーション(GuardBreak)を進める
	animationController_->Update();

	return true;
}

// スタンが始まった: 今の行動をすべてやめて、怯む
void MeleeEnemy::OnStunStart(void)
{
	ResetAttack();
	CancelBoostChase();
	CancelSpecials();

	isGuard_ = false;
	guardTimer_ = 0.0f;
	isDamage_ = false;
	damageTimer_ = 0.0f;

	animationController_->Play((int)ANIM_TYPE::GUARD_BREAK, false);
}

// スタンが終わった: 様子見に戻る
void MeleeEnemy::OnStunEnd(void)
{
	isSlamDown_ = false;

	SetAIState(AI_STATE::WAIT);
	animationController_->Play((int)ANIM_TYPE::IDLE);
}

//----------------------------------------------------------------------
// プレイヤーの攻撃への反応(ガード / 回避)
//----------------------------------------------------------------------

// 気を溜めている間・大技の直前だけ、弱い攻撃で怯まない(スーパーアーマー)
bool MeleeEnemy::IsSuperArmor(void) const
{
	return aiState_ == AI_STATE::KI_CHARGE ||
		aiState_ == AI_STATE::BIG_CHARGE ||
		aiState_ == AI_STATE::BEAM_CHARGE ||
		aiState_ == AI_STATE::RUSH_READY ||
		aiState_ == AI_STATE::SLAM_RISE ||
		// 必殺技の本番中も、殴られても止まらない
		aiState_ == AI_STATE::BEAM_FIRE ||
		aiState_ == AI_STATE::KI_BARRAGE ||
		aiState_ == AI_STATE::SLAM_DIVE ||
		aiState_ == AI_STATE::RUSH;
}

bool MeleeEnemy::IsInvincible(void) const
{
	return invincibleTimer_ > 0.0f || clashLock_;
}

void MeleeEnemy::Damage(int damage)
{
	// 回避の直後は攻撃を受けない
	if (IsInvincible())
	{
		return;
	}

	// 殴られた回数を数える(反撃のきっかけ)
	if (hitGate_ <= 0.0f)
	{
		hitStreak_++;
		hitGate_ = 0.1f;
		hitStreakTimer_ = kBurstWindow;
	}

	EnemyBase::Damage(damage);
}

//----------------------------------------------------------------------
// 近接攻撃
//----------------------------------------------------------------------

VECTOR MeleeEnemy::GetChestPos(void) const
{
	return VAdd(transform_.pos, VGet(0.0f, kChestHeight, 0.0f));
}

//----------------------------------------------------------------------
// 気弾
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// 高速移動の軌跡
//----------------------------------------------------------------------
void MeleeEnemy::UpdateTrail(float deltaTime)
{
	for (auto& p : trail_)
	{
		p.life -= deltaTime;
	}

	trail_.erase(
		std::remove_if(trail_.begin(), trail_.end(),
			[](const TrailPoint& p) { return p.life <= 0.0f; }),
		trail_.end());

	// 突進・急降下・高速接近のあいだ、軌跡を足していく
	const bool emit =
		!isHidden_ &&
		(aiState_ == AI_STATE::RUSH ||
			aiState_ == AI_STATE::SLAM_DIVE ||
			aiState_ == AI_STATE::BOOST_ATTACK);

	if (!emit)
	{
		return;
	}

	TrailPoint p{};
	p.pos = GetChestPos();
	p.life = kTrailLife;
	p.cut = !trail_.empty() && VSize(VSub(p.pos, trail_.back().pos)) > kTrailCut;

	trail_.push_back(p);

	if (trail_.size() > kTrailMax)
	{
		trail_.erase(trail_.begin());
	}
}

// 気弾・突進などの途中なら、やめて様子見に戻る(殴られたときなど)
void MeleeEnemy::CancelSpecials(void)
{
	// 高速接近(BOOST_ATTACK)は CancelBoostChase で止めるので、ここでは対象外
	if (IsDoingSpecial() && aiState_ != AI_STATE::BOOST_ATTACK)
	{
		SetAIState(AI_STATE::WAIT);
	}

	beamLocked_ = false;
	slamLocked_ = false;

	// 追撃の途中で殴られたときなどは、姿を戻して予定も消す
	isHidden_ = false;
	pendingChase_ = false;
}

// 状態の補助
void MeleeEnemy::SetAIState(AI_STATE state)
{
	aiState_ = state;
	aiTimer_ = 0.0f;

	// ビーム発射以外になったら、相殺勝負は終わり
	if (state != AI_STATE::BEAM_FIRE)
	{
		if (beamClash_) { clashLock_ = false; }
		beamClash_ = false;
	}

	// 大技を出すと、気を使い切る
	if (state == AI_STATE::BEAM_CHARGE || state == AI_STATE::KI_BARRAGE ||
		state == AI_STATE::BIG_CHARGE || state == AI_STATE::SLAM_RISE)
	{
		enemyKi_ = 0.0f;
	}

	// 大技は、名前を画面に出す
	switch (state)
	{
	case AI_STATE::BEAM_CHARGE:	ShowSkillName(kNameBeam);		break;
	case AI_STATE::KI_BARRAGE:	ShowSkillName(kNameBarrage);	break;
	case AI_STATE::BIG_CHARGE:	ShowSkillName(kNameBigBlast);	break;
	case AI_STATE::SLAM_RISE:	ShowSkillName(kNameSlam);		break;
	default: break;
	}
}

// 待機状態に戻して、IDLEアニメーションにする
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

// 位置・向き
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
	CancelSpecials();

	animationController_->Play((int)ANIM_TYPE::GUARD_BREAK, false);

	// ガードを崩されると、スタンゲージが大きく溜まる
	AddStun(kGuardBreakStun);
}

void MeleeEnemy::GuardBurst(void)
{
	// 攻撃・高速接近・特殊攻撃を中断
	ResetAttack();
	CancelBoostChase();
	CancelSpecials();

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

	CancelSpecials();
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

	// 押され始めた瞬間
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
	CancelSpecials();
}
