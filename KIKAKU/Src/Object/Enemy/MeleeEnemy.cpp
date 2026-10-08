#include <DxLib.h>
#include <cmath>
#include <algorithm>
#include "MeleeEnemy.h"
#include "../../Application.h"
#include "../Player/Player.h"
#include "../../Manager/Camera.h"
#include "../../Manager/EffekseerEffect.h"
#include "../../Manager/ResourceManager.h"
#include "../../Manager/SceneManager.h"
#include "../../Utility/AsoUtility.h"

//======================================================================
// 定数・ヘルパー(このcppの中だけで使う)
//======================================================================
namespace
{
	// 「ゼロ」とみなす長さ
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
	constexpr float kGuardTime = 0.8f;
	constexpr float kGuardBreakTime = 2.0f;
	constexpr float kGuardCoolTime = 2.0f;
	constexpr float kGuardBreakStun = 35.0f;		// ガードを崩されたときに増えるスタンゲージ

	// プレイヤーの攻撃への反応(攻撃が始まった瞬間に、1回だけ決める)
	constexpr float kReactDistance = 220.0f;		// この距離以内で攻撃されたら反応する
	constexpr int   kDodgeChance = 35;				// 回避する確率(%)
	constexpr int   kGuardChance = 35;				// ガードする確率(%)。残りは何もしない
	constexpr float kDodgeCoolTime = 5.0f;			// 回避の連発を防ぐ時間(秒)
	constexpr float kDodgeBehindDistance = 110.0f;	// 回避で、プレイヤーの背後のこの距離へ移動する
	constexpr float kDodgeInvincibleTime = 0.35f;	// 回避の直後に、攻撃を受けない時間(秒)

	// 近接攻撃
	constexpr int   kMaxCombo = 4;
	constexpr int   kFollowComboMax = 3;			// 1~3段目はプレイヤーに付いてくる
	constexpr float kComboInterval = 0.55f;
	constexpr float kAttackDistance = 90.0f;
	constexpr float kAttackFollowRate = 0.25f;
	constexpr float kLookPitchRate = 0.35f;			// 見上げ・見下ろしの最大比率
	constexpr float kMeleeHitRange = 140.0f;		// 近接攻撃が当たる水平距離
	constexpr float kMeleeHitHeight = 150.0f;		// 近接攻撃が当たる高さの差
	constexpr int   kComboDamage = 1;
	constexpr int   kFinisherDamage = 3;			// 4段目

	// 気弾
	constexpr float kKiChargeTime = 0.8f;			// 気を溜める時間(秒)。この間に避ける・近づける
	constexpr float kKiSpeed = 14.0f;				// 気弾の速さ(1/60秒あたり)
	constexpr float kKiRadius = 18.0f;
	constexpr float kKiLife = 4.0f;					// 気弾が消えるまでの時間(秒)
	constexpr float kKiCoolTime = 4.0f;				// 次に気弾を撃てるまでの時間(秒)
	constexpr int   kKiDamage = 3;
	constexpr float kBlastHitMargin = 35.0f;		// プレイヤーの体の大きさ(気弾との当たり判定に足す)
	constexpr float kPlayerCenterHeight = 70.0f;	// プレイヤーの足元から体の中心までの高さ
	constexpr float kChestHeight = 90.0f;			// 敵の足元から胸までの高さ

	// 突進
	constexpr float kRushReadyTime = 0.5f;			// 構えの時間(秒)。この間に避けられる
	constexpr float kRushSpeed = 32.0f;				// 突進の速さ(1/60秒あたり)
	constexpr float kRushTime = 0.6f;				// 突進する最大時間(秒)
	constexpr float kRushHitRange = 90.0f;			// プレイヤーに当たる距離
	constexpr float kRushCoolTime = 6.0f;
	constexpr int   kRushDamage = 5;

	// 距離による行動の選び方
	constexpr float kNearDistance = 350.0f;			// これより近い: 近接戦
	constexpr float kFarDistance = 650.0f;			// これより遠い: 高速接近か気弾

	// 足元の円(SYNC)
	constexpr int   kRingSegment = 48;
	constexpr float kRingRadius = 80.0f;

	// AI
	constexpr float kNormalBattleDistance = 280.0f;	// 普段戦う距離
	constexpr float kWaitTime = 0.5f;
	constexpr float kSideMoveTime = 1.3f;
	constexpr float kArriveDistance = 10.0f;
	constexpr float kMoveWaitTime = 0.5f;
	constexpr float kBoostStopDistance = 110.0f;
	constexpr float kAttackApproachSpeed = 5.0f;
	constexpr float kRecoverTime = 0.4f;			// 気弾・突進のあとの隙(秒)

	// 空中での旋回(プレイヤーの周りを、高さも変えながら回る)
	constexpr float kOrbitAngularSpeed = 0.9f;		// 1秒に回る角度(ラジアン)
	constexpr float kOrbitMaxSpeed = 7.0f;			// 1/60秒あたりの最大移動量
	constexpr float kOrbitMinRadius = 220.0f;
	constexpr int   kOrbitRadiusRange = 180;		// 半径は kOrbitMinRadius ~ +この値
	constexpr float kOrbitHeightMin = -80.0f;		// プレイヤーとの高さの差の最小
	constexpr int   kOrbitHeightRange = 300;		// 高さの差は kOrbitHeightMin ~ +この値
	constexpr float kOrbitRadiusLerp = 0.05f;
	constexpr float kOrbitHeightLerp = 0.04f;
	constexpr float kWaitDrift = 0.35f;				// 様子見の間は、旋回をこの倍率でゆっくり行う
	constexpr float kHoverBobAmp = 25.0f;			// 様子見の間の上下のふらつき
	constexpr float kHoverBobSpeed = 2.0f;
	constexpr float kWaitMinRadius = 220.0f;
	constexpr float kWaitMaxRadius = 420.0f;

	// 追撃(吹き飛ばした相手の先へ回り込んで、叩き落とす)
	constexpr int   kChaseChance = 70;				// 追撃に入る確率(%)
	constexpr float kChaseCoolTime = 7.0f;
	constexpr float kChaseStartDelay = 0.1f;		// 様子見に戻ってから追撃に入るまで
	constexpr float kChasePendingLimit = 2.0f;		// 追撃の予定が残る時間
	constexpr float kChaseVanishTime = 0.35f;		// 姿を消している時間
	constexpr float kChaseAppearDistance = 130.0f;	// 吹き飛ぶ向きの先、この距離に現れる
	constexpr float kChaseAppearHeight = 70.0f;		// プレイヤーより上に現れる高さ
	constexpr float kChaseWindupTime = 0.4f;		// 現れてから叩き落とすまで(ガード・回避できる)
	constexpr float kChaseRecoverTime = 0.7f;
	constexpr float kSmashRange = 190.0f;
	constexpr float kSmashFromHeight = 150.0f;		// 上から叩くので、吹き飛ぶ向きは下になる
	constexpr int   kSmashDamage = 2;

	// ガード崩し(投げ)
	constexpr float kGuardWatchRange = 450.0f;		// この距離以内でガードされ続けると数える
	constexpr float kGuardWatchTrigger = 1.2f;		// この秒数ガードされたら、投げに来る
	constexpr float kGrabCoolTime = 9.0f;
	constexpr float kGrabReadyTime = 0.6f;			// 構えて接近する時間(ここで避けられる)
	constexpr float kGrabApproachSpeed = 11.0f;
	constexpr float kGrabHitRange = 150.0f;
	constexpr int   kGrabDamage = 3;
	constexpr float kGrabKnock = 45.0f;
	constexpr float kGrabRecover = 0.6f;
	constexpr float kGrabMissRecover = 1.0f;		// 外したときの大きな隙

	// 範囲大技
	constexpr float kBigChargeTime = 1.3f;			// 溜め。ここで殴れば中断できる
	constexpr float kBigCoolTime = 12.0f;
	constexpr float kBigRadius = 190.0f;
	constexpr float kBigWarnTime = 1.2f;			// 警告が出てから爆発するまで
	constexpr float kBigStagger = 0.3f;				// 複数出すときの、爆発のずれ
	constexpr float kBigSpread = 260.0f;			// 複数出すときの、横のずれ
	constexpr int   kBigDamage = 6;
	constexpr float kBigRecoverTime = 0.9f;
	constexpr float kBigRiseSpeed = 0.6f;			// 溜めている間に浮き上がる(1/60秒あたり)

	// 行動の種類(DecideAction の中だけで使う)
	enum ACTION
	{
		ACT_ATTACK,		// 近づいて近接攻撃
		ACT_SIDE,		// 横移動
		ACT_KI,			// 気弾
		ACT_RUSH,		// 突進
		ACT_BOOST,		// 高速接近
		ACT_GRAB,		// 投げ(ガード不能)
		ACT_BIG,		// 範囲大技
		ACT_MAX
	};

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

	// 0.0~1.0 に収める
	float Saturate(float v)
	{
		if (v < 0.0f) { return 0.0f; }
		if (v > 1.0f) { return 1.0f; }
		return v;
	}

	// 重みの大きいものほど選ばれやすいランダム。選べるものが無ければ -1
	int PickWeighted(const int weights[], int count)
	{
		int total = 0;

		for (int i = 0; i < count; i++)
		{
			total += weights[i];
		}

		if (total <= 0)
		{
			return -1;
		}

		int r = GetRand(total - 1);

		for (int i = 0; i < count; i++)
		{
			if (r < weights[i])
			{
				return i;
			}

			r -= weights[i];
		}

		return count - 1;
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
	maxHp_ = hp_;
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

	hoverTime_ += deltaTime;

	// 追撃の予定は、しばらくしたら消える
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

	// 以下の関数は、そのフレームの更新を終えたときに true を返す
	if (UpdateKamehamePush()) { return; }

	const VECTOR playerPos = player_.GetTransform().pos;
	const float distance = VSize(VSub(playerPos, transform_.pos));

	// プレイヤーが攻撃を始めた瞬間(コンボ数が 0 から増えたとき)だけ、反応を決める
	const int playerCombo = player_.GetCombo();
	const bool playerAttackStarted = (playerCombo > 0 && prevPlayerCombo_ == 0);
	prevPlayerCombo_ = playerCombo;

	UpdateGuardWatch(deltaTime, distance);

	if (isStunned) { UpdateStunned(deltaTime); return; }
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

// ノックバック中の処理(叩きつけられた場合は、地面へ着いたらダウンへ)
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

		// 叩きつけられて地面に着いたらダウン
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

// スタン中の処理(動けない。殴られて押されるだけ)
bool MeleeEnemy::UpdateStunned(float deltaTime)
{
	(void)deltaTime;

	if (VSize(knockBackPow_) > kKnockBackThreshold)
	{
		transform_.pos = VAdd(transform_.pos, knockBackPow_);

		if (transform_.pos.y < kMinHeight) { transform_.pos.y = kMinHeight; }
		if (transform_.pos.y > kMaxHeight) { transform_.pos.y = kMaxHeight; }

		knockBackPow_ = VScale(knockBackPow_, kKnockBackDamping);
	}
	else
	{
		knockBackPow_ = AsoUtility::VECTOR_ZERO;
	}

	transform_.Update();
	return true;
}

// スタンが始まった: 今の行動をすべてやめて、よろめく
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

// プレイヤーが攻撃を始めたときに、1回だけ「回避する / ガードする / 何もしない」を決める
bool MeleeEnemy::TryReactToPlayerAttack(float distance)
{
	if (distance > kReactDistance)
	{
		return false;
	}

	const int roll = GetRand(99);

	if (dodgeCoolTimer_ <= 0.0f && roll < kDodgeChance)
	{
		StartDodge();
		return true;
	}

	if (guardCoolTimer_ <= 0.0f && roll < kDodgeChance + kGuardChance)
	{
		StartGuard();
		return true;
	}

	return false;
}

void MeleeEnemy::StartGuard(void)
{
	isGuard_ = true;
	guardTimer_ = kGuardTime;

	ResetAttack();
	CancelBoostChase();
	CancelSpecials();

	SetAIState(AI_STATE::WAIT);

	animationController_->Play((int)ANIM_TYPE::GUARD, true, 0.0f, -1.0f, true, true);

	transform_.Update();
}

// プレイヤーの背後へ瞬間移動して、すぐ反撃する
void MeleeEnemy::StartDodge(void)
{
	dodgeCoolTimer_ = kDodgeCoolTime;
	invincibleTimer_ = kDodgeInvincibleTime;
	justDodged_ = true;

	ResetAttack();
	CancelBoostChase();
	CancelSpecials();

	// プレイヤーの背後へ
	const VECTOR playerPos = player_.GetTransform().pos;
	const VECTOR forward = HorizontalDirOrForward(player_.GetForward(), transform_.quaRot);

	VECTOR newPos = VSub(playerPos, VScale(forward, kDodgeBehindDistance));
	newPos.y = playerPos.y;

	if (newPos.y < kMinHeight)
	{
		newPos.y = kMinHeight;
	}

	transform_.pos = newPos;
	knockBackPow_ = AsoUtility::VECTOR_ZERO;

	LookAtPlayerWithPitchLimit(playerPos);

	// 反撃
	StartMeleeAttack();

	transform_.Update();
}

bool MeleeEnemy::ConsumeDodged(void)
{
	const bool result = justDodged_;
	justDodged_ = false;
	return result;
}

bool MeleeEnemy::IsInvincible(void) const
{
	return invincibleTimer_ > 0.0f;
}

void MeleeEnemy::Damage(int damage)
{
	// 回避の直後は攻撃を受けない
	if (IsInvincible())
	{
		return;
	}

	EnemyBase::Damage(damage);
}

//----------------------------------------------------------------------
// 近接攻撃
//----------------------------------------------------------------------

// 近接攻撃を1段目から始める
void MeleeEnemy::StartMeleeAttack(void)
{
	isAttack_ = true;
	attackCombo_ = 1;
	attackTimer_ = 0.0f;
	hasAttackHit_ = false;

	isAttackWait_ = false;
	attackWaitTimer_ = 0.0f;

	animationController_->Play((int)ANIM_TYPE::ATTACK01, false);
}

// 攻撃中の処理
bool MeleeEnemy::UpdateAttack(float deltaTime, const VECTOR& playerPos)
{
	if (!isAttack_)
	{
		return false;
	}

	attackTimer_ += deltaTime;

	// 1~3段目は攻撃中もプレイヤーについていく
	if (attackCombo_ >= 1 && attackCombo_ <= kFollowComboMax)
	{
		FollowPlayerWhileAttacking(playerPos);
	}

	// 当たり判定のタイミングで、プレイヤーに当たったか調べる
	if (IsAttackHitTiming() && !hasAttackHit_)
	{
		CheckMeleeHit();
	}

	// 次の攻撃へ
	if (attackTimer_ >= kComboInterval)
	{
		AdvanceCombo();
	}

	transform_.Update();
	return true;
}

// 近接攻撃がプレイヤーに当たるか調べて、当てる
void MeleeEnemy::CheckMeleeHit(void)
{
	const VECTOR toPlayer = VSub(player_.GetTransform().pos, transform_.pos);

	if (VSize(ToHorizontal(toPlayer)) > kMeleeHitRange ||
		fabsf(toPlayer.y) > kMeleeHitHeight)
	{
		return;
	}

	// 射程内なら、当たった・防がれた・避けられたのどれでも、この段の判定は終わり
	hasAttackHit_ = true;

	const bool isFinisher = (attackCombo_ >= kMaxCombo);

	ApplyHitToPlayer(
		transform_.pos,
		isFinisher ? kFinisherDamage : kComboDamage,
		isFinisher ? 30.0f : 5.0f,		// 吹き飛ばす強さ
		isFinisher ? 80.0f : 20.0f,		// ガードされたときの、ガード耐久値の減り
		isFinisher ? 5.0f : 1.5f);		// ガードされたときの、押し戻す強さ

	// 4連撃の締めで吹き飛ばしたら、追撃に行く
	if (isFinisher)
	{
		QueueChase();
	}
}

// プレイヤーに攻撃を当てる(回避されたら false)
bool MeleeEnemy::ApplyHitToPlayer(const VECTOR& fromPos, int damage, float knockPower,
	float guardDamage, float guardKnock)
{
	lastHitDamaged_ = false;

	// 回避中は当たらない
	if (player_.IsDodging())
	{
		return false;
	}

	const VECTOR playerPos = player_.GetTransform().pos;

	VECTOR hitDir = VSub(playerPos, fromPos);

	if (VSize(hitDir) > kEpsilon)
	{
		hitDir = VNorm(hitDir);
	}
	else
	{
		hitDir = VGet(0.0f, 0.0f, 1.0f);
	}

	VECTOR effectPos = playerPos;
	effectPos.y += 80.0f;

	// ガード中は、ダメージなしでガード耐久値だけが減る
	if (player_.IsGuard())
	{
		// ガードされ続けているほど、ガード崩しの技を使いたくなる
		guardWatchTimer_ += 0.6f;

		player_.GuardDamage(guardDamage);
		player_.AddKnockBack(hitDir, guardKnock);

		EffekseerEffect::GetInstance()->PlayHitEffect(effectPos, 0.0f);

		return true;
	}

	player_.LookAtDamageEnemy(transform_.pos);
	player_.Damage(damage);
	player_.AddKnockBack(hitDir, knockPower);

	EffekseerEffect::GetInstance()->PlayHitEffect(effectPos, 0.0f);
	mainCamera.StartShake(0.1f, 4.0f);

	lastHitDamaged_ = true;
	lastHitDir_ = hitDir;

	return true;
}

// 攻撃中、プレイヤーを一定の距離の位置へ近づきながら見る
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
		// ATTACK01~04 は連番(2段目なら ATTACK02)
		const int animType = (int)ANIM_TYPE::ATTACK01 + (attackCombo_ - 1);
		animationController_->Play(animType, false);
		return;
	}

	// 攻撃終了
	ResetAttack();

	attackCoolTimer_ = ATTACK_COOL_TIME;

	ReturnToWait();
}

// プレイヤーを向く(上下の角度は制限する)
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

VECTOR MeleeEnemy::GetChestPos(void) const
{
	return VAdd(transform_.pos, VGet(0.0f, kChestHeight, 0.0f));
}

//----------------------------------------------------------------------
// 気弾
//----------------------------------------------------------------------

// 気を溜めている間の処理。溜め終わったら気弾を撃つ
bool MeleeEnemy::UpdateKiCharge(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::KI_CHARGE)
	{
		return false;
	}

	animationController_->Play((int)ANIM_TYPE::IDLE);

	LookAtPlayerWithPitchLimit(playerPos);

	aiTimer_ += deltaTime;

	if (aiTimer_ >= kKiChargeTime)
	{
		FireKiBlast();

		kiCoolTimer_ = kKiCoolTime;

		// 撃ったあとに少し隙を作る
		SetAIState(AI_STATE::WAIT);
		aiTimer_ = -kRecoverTime;
	}

	transform_.Update();
	return true;
}

// プレイヤーへ向けて気弾を撃つ
void MeleeEnemy::FireKiBlast(void)
{
	const VECTOR forward = transform_.quaRot.GetForward();
	const VECTOR origin = VAdd(GetChestPos(), VScale(forward, 50.0f));

	const VECTOR target =
		VAdd(player_.GetTransform().pos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	EnemyKiBlast blast;
	blast.Init(origin, VSub(target, origin), kKiSpeed, kKiRadius, kKiLife);

	blasts_.push_back(blast);
}

// 撃った気弾を動かして、プレイヤーに当たったか調べる
void MeleeEnemy::UpdateBlasts(float deltaTime)
{
	const VECTOR playerCenter =
		VAdd(player_.GetTransform().pos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	for (auto& blast : blasts_)
	{
		blast.Update(deltaTime);

		if (blast.IsDead())
		{
			continue;
		}

		const float distance = VSize(VSub(blast.GetPos(), playerCenter));

		if (distance > blast.GetRadius() + kBlastHitMargin)
		{
			continue;
		}

		// 当たった(回避されたときは、そのまま飛び続ける)
		if (ApplyHitToPlayer(blast.GetPos(), kKiDamage, 12.0f, 25.0f, 3.0f))
		{
			blast.Kill();
		}
	}

	// 消えた気弾を片付ける
	blasts_.erase(
		std::remove_if(blasts_.begin(), blasts_.end(),
			[](const EnemyKiBlast& b) { return b.IsDead(); }),
		blasts_.end());
}

//----------------------------------------------------------------------
// 突進
//----------------------------------------------------------------------

// 構え → 突進
bool MeleeEnemy::UpdateRush(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::RUSH_READY && aiState_ != AI_STATE::RUSH)
	{
		return false;
	}

	aiTimer_ += deltaTime;

	// 構え: その場でプレイヤーを見て溜める(この間は避けやすい)
	if (aiState_ == AI_STATE::RUSH_READY)
	{
		animationController_->Play((int)ANIM_TYPE::IDLE);

		LookAtPlayerWithPitchLimit(playerPos);

		if (aiTimer_ >= kRushReadyTime)
		{
			// 突進する向きは、構えが終わった瞬間のプレイヤーの位置で決まる
			const VECTOR target =
				VAdd(playerPos, VGet(0.0f, kPlayerCenterHeight * 0.5f, 0.0f));

			rushDir_ = VSub(target, transform_.pos);
			rushDir_ = (VSize(rushDir_) > kEpsilon) ? VNorm(rushDir_) : VGet(0.0f, 0.0f, 1.0f);

			aiState_ = AI_STATE::RUSH;
			aiTimer_ = 0.0f;

			animationController_->Play((int)ANIM_TYPE::FAST_RUN);
		}

		transform_.Update();
		return true;
	}

	// 突進中
	transform_.pos = VAdd(transform_.pos, VScale(rushDir_, kRushSpeed * deltaTime * 60.0f));

	if (transform_.pos.y < kMinHeight) { transform_.pos.y = kMinHeight; }
	if (transform_.pos.y > kMaxHeight) { transform_.pos.y = kMaxHeight; }

	transform_.quaRot = Quaternion::LookRotation(rushDir_);

	bool finished = (aiTimer_ >= kRushTime);

	// プレイヤーに当たったら終わり(回避されたら、そのまま通り過ぎる)
	if (VSize(VSub(playerPos, transform_.pos)) <= kRushHitRange)
	{
		if (ApplyHitToPlayer(transform_.pos, kRushDamage, 28.0f, 40.0f, 6.0f))
		{
			finished = true;
			QueueChase();
		}
	}

	if (finished)
	{
		rushCoolTimer_ = kRushCoolTime;

		// 突進のあとは大きな隙になる
		SetAIState(AI_STATE::WAIT);
		aiTimer_ = -kRecoverTime;

		animationController_->Play((int)ANIM_TYPE::IDLE);
	}

	transform_.Update();
	return true;
}

//----------------------------------------------------------------------
// 範囲大技
//----------------------------------------------------------------------

// 溜め → 警告の球を出す
bool MeleeEnemy::UpdateBigCharge(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::BIG_CHARGE)
	{
		return false;
	}

	animationController_->Play((int)ANIM_TYPE::IDLE);

	LookAtPlayerWithPitchLimit(playerPos);

	aiTimer_ += deltaTime;

	// 溜めている間は、ゆっくり浮き上がる
	transform_.pos.y += kBigRiseSpeed * deltaTime * 60.0f;

	if (transform_.pos.y > kMaxHeight)
	{
		transform_.pos.y = kMaxHeight;
	}

	if (aiTimer_ >= kBigChargeTime)
	{
		LaunchAreaBlasts(playerPos);

		bigCoolTimer_ = kBigCoolTime;

		// 撃ったあとの隙
		SetAIState(AI_STATE::WAIT);
		aiTimer_ = -kBigRecoverTime;
	}

	transform_.Update();
	return true;
}

// プレイヤーの今いる場所に警告の球を出す。HPが半分以下なら、左右にもずらして出す
void MeleeEnemy::LaunchAreaBlasts(const VECTOR& playerPos)
{
	const bool enraged = (hp_ * 2 <= maxHp_);
	const int count = enraged ? 3 : 1;

	const VECTOR center = VAdd(playerPos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	const VECTOR toPlayer =
		HorizontalDirOrForward(VSub(playerPos, transform_.pos), transform_.quaRot);
	const VECTOR side = VGet(toPlayer.z, 0.0f, -toPlayer.x);

	for (int i = 0; i < count; i++)
	{
		// 0番目はプレイヤーの位置、1・2番目は左右
		float offset = 0.0f;

		if (i == 1) { offset = kBigSpread; }
		if (i == 2) { offset = -kBigSpread; }

		EnemyAreaBlast blast;
		blast.Init(
			VAdd(center, VScale(side, offset)),
			kBigRadius,
			kBigWarnTime + kBigStagger * (float)i);

		areaBlasts_.push_back(blast);
	}
}

// 範囲攻撃を進めて、爆発したときにプレイヤーが範囲内ならダメージを与える
void MeleeEnemy::UpdateAreaBlasts(float deltaTime)
{
	const VECTOR playerCenter =
		VAdd(player_.GetTransform().pos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	for (auto& blast : areaBlasts_)
	{
		blast.Update(deltaTime);

		if (!blast.ConsumeExplode())
		{
			continue;
		}

		mainCamera.StartShake(0.2f, 6.0f);

		const float distance = VSize(VSub(blast.GetPos(), playerCenter));

		if (distance <= blast.GetRadius())
		{
			ApplyHitToPlayer(blast.GetPos(), kBigDamage, 35.0f, 80.0f, 8.0f);
		}
	}

	areaBlasts_.erase(
		std::remove_if(areaBlasts_.begin(), areaBlasts_.end(),
			[](const EnemyAreaBlast& b) { return b.IsDead(); }),
		areaBlasts_.end());
}

//----------------------------------------------------------------------
// ガード崩し(投げ)
//----------------------------------------------------------------------

// プレイヤーがガードしている時間を数える(ガードをやめると、ゆっくり減る)
void MeleeEnemy::UpdateGuardWatch(float deltaTime, float distance)
{
	if (player_.IsGuard() && distance < kGuardWatchRange)
	{
		guardWatchTimer_ += deltaTime;
		return;
	}

	CountDown(guardWatchTimer_, deltaTime * 0.5f);
}

// 構え → 接近 → 掴む。ガードしていても防げないが、回避なら避けられる
bool MeleeEnemy::UpdateGrab(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::GRAB_READY)
	{
		return false;
	}

	aiTimer_ += deltaTime;

	LookAtPlayerWithPitchLimit(playerPos);

	// 構えの間に、プレイヤーへ素早く近づく
	const VECTOR toPlayer = VSub(playerPos, transform_.pos);
	const float distance = VSize(toPlayer);

	if (distance > kGrabHitRange * 0.6f)
	{
		float step = kGrabApproachSpeed * deltaTime * 60.0f;

		if (step > distance)
		{
			step = distance;
		}

		transform_.pos = VAdd(transform_.pos, VScale(VNorm(toPlayer), step));

		animationController_->Play((int)ANIM_TYPE::FAST_RUN);
	}
	else
	{
		animationController_->Play((int)ANIM_TYPE::IDLE);
	}

	if (aiTimer_ >= kGrabReadyTime)
	{
		DoGrab();
	}

	transform_.Update();
	return true;
}

// 掴む(ガードを崩してダメージ。回避中・遠すぎるときは空振り)
void MeleeEnemy::DoGrab(void)
{
	grabCoolTimer_ = kGrabCoolTime;
	guardWatchTimer_ = 0.0f;

	const VECTOR playerPos = player_.GetTransform().pos;
	const VECTOR toPlayer = VSub(playerPos, transform_.pos);

	bool hit = false;

	if (VSize(toPlayer) <= kGrabHitRange && !player_.IsDodging())
	{
		hit = true;

		VECTOR dir = (VSize(toPlayer) > kEpsilon) ? VNorm(toPlayer) : VGet(0.0f, 0.0f, 1.0f);

		// ガード中なら、ガードを一撃で崩す
		if (player_.IsGuard())
		{
			player_.GuardDamage(9999.0f);
		}

		player_.LookAtDamageEnemy(transform_.pos);
		player_.Damage(kGrabDamage);
		player_.AddKnockBack(dir, kGrabKnock);

		EffekseerEffect::GetInstance()->PlayHitEffect(
			VAdd(playerPos, VGet(0.0f, 80.0f, 0.0f)), 0.0f);
		mainCamera.StartShake(0.2f, 8.0f);
	}

	// 当たっても外しても隙ができる(外した方が大きい)
	SetAIState(AI_STATE::WAIT);
	aiTimer_ = hit ? -kGrabRecover : -kGrabMissRecover;

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

//----------------------------------------------------------------------
// 追撃
//----------------------------------------------------------------------

// 直前の攻撃でダメージが入っていたら、追撃の予定を入れる
void MeleeEnemy::QueueChase(void)
{
	if (!lastHitDamaged_ || chaseCoolTimer_ > 0.0f)
	{
		return;
	}

	if (GetRand(99) >= kChaseChance)
	{
		return;
	}

	pendingChase_ = true;
	pendingChaseTimer_ = kChasePendingLimit;
}

// 姿を消す(吹き飛ぶ向きの先へ回り込むために)
void MeleeEnemy::StartChase(void)
{
	pendingChase_ = false;
	chaseCoolTimer_ = kChaseCoolTime;

	chaseDir_ = HorizontalDirOrForward(lastHitDir_, transform_.quaRot);
	vanishFrom_ = GetChestPos();

	ResetAttack();
	CancelBoostChase();

	isHidden_ = true;
	invincibleTimer_ = kChaseVanishTime + 0.1f;

	SetAIState(AI_STATE::CHASE_VANISH);
}

// 消える → 先回りして現れる → 叩き落とす
bool MeleeEnemy::UpdateChase(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::CHASE_VANISH && aiState_ != AI_STATE::CHASE_APPEAR)
	{
		return false;
	}

	aiTimer_ += deltaTime;

	if (aiState_ == AI_STATE::CHASE_VANISH)
	{
		if (aiTimer_ >= kChaseVanishTime)
		{
			// 吹き飛ばされているプレイヤーの、進む先へ現れる
			VECTOR newPos = VAdd(playerPos, VScale(chaseDir_, kChaseAppearDistance));
			newPos.y = playerPos.y + kChaseAppearHeight;

			if (newPos.y < kMinHeight) { newPos.y = kMinHeight; }
			if (newPos.y > kMaxHeight) { newPos.y = kMaxHeight; }

			transform_.pos = newPos;
			isHidden_ = false;

			SetAIState(AI_STATE::CHASE_APPEAR);

			LookAtPlayerWithPitchLimit(playerPos);
			animationController_->Play((int)ANIM_TYPE::ATTACK03, false);
		}
	}
	else
	{
		// 現れてから叩くまでの間は、ガードや回避ができる
		LookAtPlayerWithPitchLimit(playerPos);

		if (aiTimer_ >= kChaseWindupTime)
		{
			DoSmash();
		}
	}

	transform_.Update();
	return true;
}

// 上から叩き落とす
void MeleeEnemy::DoSmash(void)
{
	const VECTOR playerPos = player_.GetTransform().pos;

	if (VSize(VSub(playerPos, transform_.pos)) <= kSmashRange)
	{
		// 上から叩くので、プレイヤーは下へ飛ぶ
		ApplyHitToPlayer(
			VAdd(playerPos, VGet(0.0f, kSmashFromHeight, 0.0f)),
			kSmashDamage, 38.0f, 60.0f, 6.0f);
	}

	attackCoolTimer_ = ATTACK_COOL_TIME * 0.6f;

	SetAIState(AI_STATE::WAIT);
	aiTimer_ = -kChaseRecoverTime;

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

// 気弾・突進の途中なら、やめて様子見に戻る(殴られたときなど)
void MeleeEnemy::CancelSpecials(void)
{
	if (aiState_ == AI_STATE::KI_CHARGE ||
		aiState_ == AI_STATE::RUSH_READY ||
		aiState_ == AI_STATE::RUSH ||
		aiState_ == AI_STATE::BIG_CHARGE ||
		aiState_ == AI_STATE::GRAB_READY ||
		aiState_ == AI_STATE::CHASE_VANISH ||
		aiState_ == AI_STATE::CHASE_APPEAR)
	{
		SetAIState(AI_STATE::WAIT);
	}

	// 追撃の途中で殴られたときなどは、姿を戻して予定も消す
	isHidden_ = false;
	pendingChase_ = false;
}

// 状態の補助
void MeleeEnemy::SetAIState(AI_STATE state)
{
	aiState_ = state;
	aiTimer_ = 0.0f;
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

//----------------------------------------------------------------------
// AI
//----------------------------------------------------------------------
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

	default:
		// KI_CHARGE / RUSH_READY / RUSH は、Update の前半で処理している
		break;
	}
}

// 様子を見る。時間が経ったら、次の行動を選ぶ
void MeleeEnemy::UpdateAIWait(const AIContext& ctx)
{
	aiTimer_ += ctx.deltaTime;

	animationController_->Play((int)ANIM_TYPE::IDLE);

	// 隙の間(aiTimer_ がマイナス)は動かない。それ以外は、プレイヤーの周りをゆっくり漂う
	if (aiTimer_ >= 0.0f)
	{
		float radius = ctx.distance;

		if (radius < kWaitMinRadius) { radius = kWaitMinRadius; }
		if (radius > kWaitMaxRadius) { radius = kWaitMaxRadius; }

		OrbitPlayer(ctx, kWaitDrift, radius,
			kHoverBobAmp * sinf(hoverTime_ * kHoverBobSpeed));
	}

	// 吹き飛ばした直後なら、追撃に入る
	if (pendingChase_ && aiTimer_ >= kChaseStartDelay)
	{
		StartChase();
		return;
	}

	if (aiTimer_ < kWaitTime)
	{
		return;
	}

	DecideAction(ctx);
}

// 距離とHPから、次の行動を選ぶ
//   近い: 近接攻撃が中心 / 中くらい: 気弾・突進 / 遠い: 高速接近・気弾
//   HPが半分以下になると、気弾と突進を多く使う
void MeleeEnemy::DecideAction(const AIContext& ctx)
{
	const bool enraged = (hp_ * 2 <= maxHp_);

	const bool canAttack = (attackCoolTimer_ <= 0.0f);
	const bool canKi = (kiCoolTimer_ <= 0.0f);
	const bool canRush = (rushCoolTimer_ <= 0.0f);
	const bool canBig = (bigCoolTimer_ <= 0.0f);

	// ガードされ続けているなら、ガード不能の投げを選ぶ
	const bool wantGrab =
		guardWatchTimer_ >= kGuardWatchTrigger &&
		grabCoolTimer_ <= 0.0f &&
		ctx.distance < kGuardWatchRange + 100.0f;

	int weights[ACT_MAX] = {};

	if (ctx.distance < kNearDistance)
	{
		weights[ACT_ATTACK] = canAttack ? 5 : 0;
		weights[ACT_SIDE] = 2;
		weights[ACT_RUSH] = canRush ? (enraged ? 3 : 1) : 0;
		weights[ACT_BIG] = canBig ? (enraged ? 2 : 1) : 0;
	}
	else if (ctx.distance < kFarDistance)
	{
		weights[ACT_KI] = canKi ? (enraged ? 5 : 3) : 0;
		weights[ACT_RUSH] = canRush ? (enraged ? 5 : 3) : 0;
		weights[ACT_SIDE] = 2;
		weights[ACT_BOOST] = 1;
		weights[ACT_BIG] = canBig ? (enraged ? 4 : 2) : 0;
	}
	else
	{
		weights[ACT_BOOST] = 3;
		weights[ACT_KI] = canKi ? (enraged ? 5 : 3) : 0;
		weights[ACT_BIG] = canBig ? (enraged ? 4 : 2) : 0;
	}

	if (wantGrab)
	{
		weights[ACT_GRAB] = 15;
	}

	int pick = PickWeighted(weights, ACT_MAX);

	// 選べるものが無ければ、横に動いて時間を稼ぐ
	if (pick < 0)
	{
		pick = ACT_SIDE;
	}

	switch (pick)
	{
	case ACT_ATTACK:
		SetAIState(AI_STATE::MOVE);
		break;

	case ACT_SIDE:
		SetAIState(AI_STATE::SIDE_MOVE);

		// 回る向き・距離・高さを決める(同じ向きに続けることもある)
		if (GetRand(99) < 70)
		{
			sideMoveDir_ *= -1.0f;
		}

		strafeRadius_ = kOrbitMinRadius + (float)GetRand(kOrbitRadiusRange);
		strafeHeight_ = kOrbitHeightMin + (float)GetRand(kOrbitHeightRange);
		break;

	case ACT_KI:
		SetAIState(AI_STATE::KI_CHARGE);
		break;

	case ACT_RUSH:
		SetAIState(AI_STATE::RUSH_READY);
		break;

	case ACT_BOOST:
		SetAIState(AI_STATE::BOOST_ATTACK);
		break;

	case ACT_GRAB:
		SetAIState(AI_STATE::GRAB_READY);
		break;

	case ACT_BIG:
		SetAIState(AI_STATE::BIG_CHARGE);
		break;
	}
}

// 空中の旋回(プレイヤーの周りを、距離と高さを変えながら回る)
void MeleeEnemy::UpdateAISideMove(const AIContext& ctx)
{
	aiTimer_ += ctx.deltaTime;

	OrbitPlayer(ctx, 1.0f, strafeRadius_, strafeHeight_);

	animationController_->Play((int)ANIM_TYPE::RUN);

	// 旋回が終わったら、また次の行動を選ぶ
	if (aiTimer_ >= kSideMoveTime)
	{
		SetAIState(AI_STATE::WAIT);
	}
}

// プレイヤーを中心に、角度を進めた位置へ向かって移動する
void MeleeEnemy::OrbitPlayer(const AIContext& ctx, float speedScale, float radius, float heightOffset)
{
	const VECTOR rel = ToHorizontal(VSub(transform_.pos, ctx.playerPos));
	const float currentRadius = VSize(rel);

	float angle = 0.0f;

	if (currentRadius > kEpsilon)
	{
		angle = atan2f(rel.z, rel.x);
	}

	// 角度を進める
	angle += sideMoveDir_ * kOrbitAngularSpeed * speedScale * ctx.deltaTime;

	// 半径と高さは、目標へ少しずつ寄せる
	const float frameRate = ctx.deltaTime * 60.0f;
	const float r = currentRadius + (radius - currentRadius) * Saturate(kOrbitRadiusLerp * frameRate);

	const float targetY = ctx.playerPos.y + heightOffset;
	const float y = transform_.pos.y + (targetY - transform_.pos.y) * Saturate(kOrbitHeightLerp * frameRate);

	const VECTOR desired = VGet(
		ctx.playerPos.x + cosf(angle) * r,
		y,
		ctx.playerPos.z + sinf(angle) * r);

	// 速すぎないように、1フレームの移動量を制限する
	VECTOR move = VSub(desired, transform_.pos);
	const float maxStep = kOrbitMaxSpeed * frameRate;

	if (VSize(move) > maxStep)
	{
		move = VScale(VNorm(move), maxStep);
	}

	transform_.pos = VAdd(transform_.pos, move);

	if (transform_.pos.y < kMinHeight) { transform_.pos.y = kMinHeight; }
	if (transform_.pos.y > kMaxHeight) { transform_.pos.y = kMaxHeight; }
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

// 高速接近(次の行動を選んだ時だけ入る)
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

// 攻撃距離まで近づいて、4連撃を開始する
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
	StartMeleeAttack();
}

// 描画
void MeleeEnemy::Draw(void)
{
	if (isDead_)
	{
		return;
	}

	// 敵本体
	if (!isHidden_)
	{
		SetUseLighting(FALSE);
		MV1DrawModel(transform_.modelId);
		SetUseLighting(TRUE);
	}

	// 撃った気弾
	for (const auto& blast : blasts_)
	{
		blast.Draw();
	}

	// 範囲攻撃(警告・爆発)
	for (const auto& blast : areaBlasts_)
	{
		blast.Draw();
	}

	// 気弾・突進の予告(プレイヤーが避ける目印)
	DrawTelegraph();

	// HPバー・スタンゲージ(2D)
	DrawHud();
}

// 気を溜めている光・突進の構えの光
void MeleeEnemy::DrawTelegraph(void) const
{
	if (aiState_ == AI_STATE::KI_CHARGE)
	{
		const float rate = aiTimer_ / kKiChargeTime;
		const VECTOR forward = transform_.quaRot.GetForward();
		const VECTOR pos = VAdd(GetChestPos(), VScale(forward, 50.0f));

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 110);
		DrawSphere3D(pos, 8.0f + 36.0f * rate, 16, GetColor(170, 90, 255), GetColor(170, 90, 255), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawSphere3D(pos, 4.0f + 14.0f * rate, 16, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);

		SetUseLighting(TRUE);
	}

	if (aiState_ == AI_STATE::RUSH_READY)
	{
		// だんだん赤く、大きくなる
		const float rate = aiTimer_ / kRushReadyTime;
		const float pulse = 0.5f + 0.5f * sinf(aiTimer_ * 40.0f);

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90 + (int)(80.0f * pulse));
		DrawSphere3D(GetChestPos(), 30.0f + 30.0f * rate, 16, GetColor(255, 60, 40), GetColor(255, 60, 40), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetUseLighting(TRUE);
	}

	// 範囲大技の溜め: 頭の上に大きな球が育つ
	if (aiState_ == AI_STATE::BIG_CHARGE)
	{
		const float rate = Saturate(aiTimer_ / kBigChargeTime);
		const VECTOR pos = VAdd(transform_.pos, VGet(0.0f, 170.0f, 0.0f));

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 110);
		DrawSphere3D(pos, 10.0f + 60.0f * rate, 20, GetColor(255, 90, 50), GetColor(255, 90, 50), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawSphere3D(pos, 5.0f + 28.0f * rate, 16, GetColor(255, 230, 200), GetColor(255, 230, 200), TRUE);

		SetUseLighting(TRUE);
	}

	// 投げの構え: オレンジに光る(ガードしても防げない合図)
	if (aiState_ == AI_STATE::GRAB_READY)
	{
		const float pulse = 0.5f + 0.5f * sinf(aiTimer_ * 36.0f);

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 100 + (int)(70.0f * pulse));
		DrawSphere3D(GetChestPos(), 50.0f + 20.0f * pulse, 16, GetColor(255, 170, 30), GetColor(255, 170, 30), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetUseLighting(TRUE);
	}

	// 追撃: 消えた場所に残像の光 / 現れたら黄色く光って叩く構え
	if (aiState_ == AI_STATE::CHASE_VANISH)
	{
		const float rate = Saturate(aiTimer_ / kChaseVanishTime);

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(180.0f * (1.0f - rate)));
		DrawSphere3D(vanishFrom_, 60.0f * (1.0f - rate) + 10.0f, 16, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetUseLighting(TRUE);
	}

	if (aiState_ == AI_STATE::CHASE_APPEAR)
	{
		const float pulse = 0.5f + 0.5f * sinf(aiTimer_ * 40.0f);

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 100 + (int)(80.0f * pulse));
		DrawSphere3D(GetChestPos(), 55.0f, 16, GetColor(255, 240, 80), GetColor(255, 240, 80), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetUseLighting(TRUE);
	}
}

// 足元に、残り時間で縮む円を描く
void MeleeEnemy::DrawSyncRing(void)
{
	if (!IsSyncReady())
	{
		return;
	}

	VECTOR center = transform_.pos;

	// 床より少し上に
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

	// 外側の線
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
	CancelSpecials();

	animationController_->Play((int)ANIM_TYPE::GUARD_BREAK, false);

	// ガードを崩されると、スタンゲージも大きく増える
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