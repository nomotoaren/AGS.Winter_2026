#include <DxLib.h>
#include "EnemyBase.h"
#include "../Player/Player.h"
#include "../../Manager/Camera.h"
#include "../../Manager/EffekseerEffect.h"
#include "../../Manager/SceneManager.h"
#include "../../Utility/AsoUtility.h"

namespace
{
	// 「ゼロ」とみなす長さ
	constexpr float kEpsilon = 0.001f;
}

EnemyBase::EnemyBase(Player& player)
	:
	knockBackPow_(AsoUtility::VECTOR_ZERO),
	player_(player),
	hp_(1),
	maxHp_(1),
	isDead_(false),
	hitRadius_(50.0f),
	pendingDamage_(0),
	isSyncReady_(false),
	syncTimer_(0.0f),
	stun_(0.0f),
	stunDecayWait_(0.0f),
	isStunned_(false),
	stunTimer_(0.0f),
	stunResist_(0.0f),
	stunBarTime_(0.0f)
{
}

EnemyBase::~EnemyBase(void)
{
}

//------------------------------------------------------------
// 設定
//------------------------------------------------------------
void EnemyBase::ApplyInfo(const EnemyData::Info& info)
{
	displayName_ = info.name;
	hp_ = info.hp;
	maxHp_ = info.hp;
	hitRadius_ = info.hitRadius;
	modelScale_ = info.modelScale;
	damageRate_ = info.damageRate;
}

void EnemyBase::SetMaxHp(int hp)
{
	hp_ = hp;
	maxHp_ = hp;
}

void EnemyBase::SetPosition(VECTOR pos)
{
	transform_.pos = pos;

	transform_.Update();
}

//------------------------------------------------------------
// ダメージ・HP
//------------------------------------------------------------
void EnemyBase::Damage(int damage)
{
	if (isDead_)
	{
		return;
	}

	// スタン中は無防備なので、ダメージが大きくなる
	if (isStunned_)
	{
		damage *= STUN_DAMAGE_RATE;
	}

	hp_ -= damage;

	if (hp_ <= 0)
	{
		hp_ = 0;
		isDead_ = true;
	}
}

bool EnemyBase::IsDead(void) const
{
	return isDead_;
}

int EnemyBase::GetHp(void) const
{
	return hp_;
}

float EnemyBase::GetHitRadius(void) const
{
	return hitRadius_;
}

void EnemyBase::AddKnockBack(VECTOR dir, float power)
{
	if (isDead_ || VSize(dir) <= kEpsilon)
	{
		return;
	}

	// 弱い攻撃ではほとんどひるまない(殴られても行動を続ける)
	armoredHit_ = false;

	if (!isStunned_ && (IsSuperArmor() || (power < HEAVY_KNOCK && GetRand(99) >= FLINCH_CHANCE)))
	{
		armoredHit_ = true;
		power *= ARMOR_KNOCK_RATE;
	}

	knockBackPow_ = VScale(VNorm(dir), power);
}

void EnemyBase::PushOut(const VECTOR& offset)
{
	if (isDead_)
	{
		return;
	}

	transform_.pos = VAdd(transform_.pos, offset);
}

bool EnemyBase::IsInvincible(void) const
{
	return false;
}

//------------------------------------------------------------
// スタンゲージ
//------------------------------------------------------------
void EnemyBase::AddStun(float amount)
{
	// スタン中・スタン明け・無敵中・死亡時は、ゲージが溜まらない
	if (isDead_ || isStunned_ || stunResist_ > 0.0f || IsInvincible())
	{
		return;
	}

	stun_ += amount;
	stunDecayWait_ = STUN_DECAY_DELAY;

	if (stun_ >= STUN_MAX)
	{
		stun_ = STUN_MAX;

		isStunned_ = true;
		stunTimer_ = STUN_TIME;

		OnStunStart();
	}
}

bool EnemyBase::UpdateStunGauge(float deltaTime)
{
	stunBarTime_ += deltaTime;

	if (stunResist_ > 0.0f)
	{
		stunResist_ -= deltaTime;

		if (stunResist_ < 0.0f)
		{
			stunResist_ = 0.0f;
		}
	}

	// スタン中は、残り時間を数える
	if (isStunned_)
	{
		stunTimer_ -= deltaTime;

		if (stunTimer_ <= 0.0f)
		{
			stunTimer_ = 0.0f;
			isStunned_ = false;

			stun_ = 0.0f;
			stunResist_ = STUN_RESIST_TIME;

			OnStunEnd();
		}

		return isStunned_;
	}

	// しばらく攻撃されなければ、ゲージが減っていく
	if (stun_ > 0.0f)
	{
		if (stunDecayWait_ > 0.0f)
		{
			stunDecayWait_ -= deltaTime;
		}
		else
		{
			stun_ -= STUN_DECAY_SPEED * deltaTime;

			if (stun_ < 0.0f)
			{
				stun_ = 0.0f;
			}
		}
	}

	return false;
}

bool EnemyBase::IsStunned(void) const
{
	return isStunned_;
}

float EnemyBase::GetStunRate(void) const
{
	if (isStunned_)
	{
		// スタン中: 空のゲージが、時間をかけて回復していく
		return 1.0f - stunTimer_ / STUN_TIME;
	}

	// 普段は満タン。攻撃されるほど減る
	return 1.0f - stun_ / STUN_MAX;
}

//------------------------------------------------------------
// HPバー(HUD)
//------------------------------------------------------------

// 各敵の Init の最後に1回呼ぶ。maxHp はその時点の hp_ を渡すとよい
void EnemyBase::InitHud(const std::string& name, int maxHp)
{
	hud_.Init();
	hud_.SetName(name);
	hud_.SetIconText(name.substr(0, 1));
	hud_.SetMaxHp(maxHp);
}

// 各敵の Update で、isDead_ チェックの直後に呼ぶ
void EnemyBase::UpdateHud(void)
{
	hud_.Update(hp_);
}

// 各敵の Draw の最後で呼ぶ(2D描画なので3D描画のあと)
void EnemyBase::DrawHud(void)
{
	if (isDead_)
	{
		return;
	}

	hud_.Draw();

	// スタンゲージはHPバーの下に出す
	stunBar_.Draw(GetStunRate(), isStunned_, stunBarTime_);

	// 技の名前の帯
	if (hasSkillName_)
	{
		const float elapsed = (float)(GetNowCount() - skillNameStartMs_) / 1000.0f;

		if (elapsed >= EnemyStunBar::SKILL_BANNER_TIME)
		{
			hasSkillName_ = false;
		}
		else
		{
			stunBar_.DrawSkillBanner(skillName_.c_str(), elapsed);
		}
	}
}

void EnemyBase::ShowSkillName(const char* name)
{
	skillName_ = name;
	skillNameStartMs_ = GetNowCount();
	hasSkillName_ = true;
}

//------------------------------------------------------------
// プレイヤーへの攻撃
//------------------------------------------------------------
EnemyBase::HIT_RESULT EnemyBase::HitPlayer(const VECTOR& fromPos, int damage, float knockPower,
	float guardDamage, float guardKnock)
{
	// 回避中は当たらない(ステップを始めた直後なら、ジャスト回避になる)
	if (player_.IsDodging())
	{
		player_.TryJustDodge();
		return HIT_RESULT::DODGED;
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

	// ガード中は、ダメージなしでガード耐久値だけ減らす
	if (player_.IsGuard())
	{
		player_.GuardDamage(guardDamage);
		player_.AddKnockBack(hitDir, guardKnock);

		EffekseerEffect::GetInstance()->PlayHitEffect(effectPos, 0.0f);

		return HIT_RESULT::GUARDED;
	}

	player_.LookAtDamageEnemy(transform_.pos);
	{
		int scaled = (int)(damage * damageRate_);
		if (scaled < 1) { scaled = 1; }

		player_.Damage(scaled);
	}
	player_.AddKnockBack(hitDir, knockPower);

	EffekseerEffect::GetInstance()->PlayHitEffect(effectPos, 0.0f);
	mainCamera.StartShake(0.1f, 4.0f);

	lastHitDir_ = hitDir;

	return HIT_RESULT::DAMAGED;
}

//------------------------------------------------------------
// SYNC ATTACK
//------------------------------------------------------------
void EnemyBase::StartSyncWindow(void)
{
	isSyncReady_ = true;
	syncTimer_ = 0.0f;
}

void EnemyBase::UpdateSyncWindow(float deltaTime)
{
	if (!isSyncReady_)
	{
		return;
	}

	syncTimer_ += deltaTime;

	// 受付時間が過ぎたら終了
	if (syncTimer_ >= SYNC_TIME)
	{
		EndSyncWindow();
	}
}

bool EnemyBase::IsSyncReady(void) const
{
	return isSyncReady_;
}

void EnemyBase::EndSyncWindow(void)
{
	isSyncReady_ = false;
	syncTimer_ = 0.0f;
}

// 受付開始時は 1.0、受付終了時は 0.0 になる残り時間の割合
float EnemyBase::GetSyncRate(void) const
{
	if (!isSyncReady_)
	{
		return 0.0f;
	}

	float rate = 1.0f - syncTimer_ / SYNC_TIME;

	if (rate < 0.0f) { rate = 0.0f; }
	if (rate > 1.0f) { rate = 1.0f; }

	return rate;
}