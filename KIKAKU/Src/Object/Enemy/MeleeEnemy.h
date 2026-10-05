#pragma once
#include "EnemyBase.h"
#include <memory>
#include "../Common/AnimationController.h"

class Player;

class MeleeEnemy : public EnemyBase
{

public:

	// 攻撃のダメージ
	static constexpr int ATTACK_DAMAGE = 1;

	// 驚き表示の時間
	static constexpr float SURPRISE_TIME = 0.5f;

	// 高速接近
	static constexpr float BOOST_CHASE_DISTANCE = 400.0f;
	static constexpr float BOOST_CHASE_SPEED = 18.0f;
	static constexpr float BOOST_CHASE_TIME = 0.6f;

	// アニメーション種別
	enum class ANIM_TYPE
	{
		IDLE,
		RUN,
		FAST_RUN,
		ATTACK01,
		ATTACK02,
		ATTACK03,
		ATTACK04,
		KAMEHAME,
		DAMAGE,
		GUARD,
		GUARD_BREAK
	};

	MeleeEnemy(Player& player);
	~MeleeEnemy(void);

	void Init(void) override;
	void Update(void) override;
	void Draw(void) override;

	void InitAnimation(void);

	// 位置設定
	void SetPosition(VECTOR pos);

	void LookAtPlayer(void);

	// 攻撃
	bool IsAttackHitTiming(void) const;

	bool HasAttackHit(void) const;
	void SetAttackHit(void);

	int GetAttackCombo(void) const;

	// ガード
	bool IsGuard(void) const;
	void GuardDamage(float damage);
	void GuardBurst(void);

	// 叩き落とし開始
	void StartSlamDown(void);

	// ダウン中か
	bool IsDown(void) const;

	// かめはめ波を受けているか(dir: 押し出す方向)
	void SetKamehameHit(bool hit, VECTOR dir = { 0.0f, 0.0f, 0.0f });

private:

	// 行動パターン
	enum class AI_STATE
	{
		WAIT,			// 様子を見る
		MOVE,			// 間合い調整
		SIDE_MOVE,		// 横移動
		BOOST_ATTACK,	// 高速接近から攻撃
		ATTACK			// 近接攻撃
	};

	// 攻撃の時間
	static constexpr float ATTACK_TIME = 0.8f;
	static constexpr float ATTACK_COOL_TIME = 10.5f;
	static constexpr float ATTACK_HIT_START = 0.25f;
	static constexpr float ATTACK_HIT_END = 0.45f;

	// AIが使う入力情報
	struct AIContext
	{
		float deltaTime;
		float distance;			// プレイヤーとの距離
		VECTOR playerPos;
		VECTOR horizontalDir;	// プレイヤーへの水平方向(単位ベクトル)
	};

	void DrawSyncRing(void);

	// 更新
	bool UpdateKamehamePush(void);
	bool UpdateKnockBack(void);
	bool UpdateDown(float deltaTime);
	bool UpdateDamage(float deltaTime);
	bool UpdateGuardBreak(float deltaTime);
	bool UpdateGuard(float deltaTime);
	bool TryStartGuard(float distance);
	bool UpdateAttack(float deltaTime, const VECTOR& playerPos);

	// 攻撃の補助
	void FollowPlayerWhileAttacking(const VECTOR& playerPos);
	void AdvanceCombo(void);
	void LookAtPlayerWithPitchLimit(const VECTOR& playerPos);

	// 状態の補助
	void SetAIState(AI_STATE state);
	void ReturnToWait(void);
	void ResetAttack(void);
	void CancelBoostChase(void);

	// AI(現在は Update から呼んでいない)
	void UpdateAI(const AIContext& ctx);
	void UpdateAIWait(const AIContext& ctx);
	void UpdateAISideMove(const AIContext& ctx);
	void UpdateAIMove(const AIContext& ctx);
	void UpdateAIBoostAttack(const AIContext& ctx);
	void UpdateAIAttack(const AIContext& ctx);

	// AI
	AI_STATE aiState_ = AI_STATE::WAIT;
	float aiTimer_ = 0.0f;

	// 横移動の向き(1.0f / -1.0f)
	float sideMoveDir_ = 1.0f;

	Player& player_;

	std::unique_ptr<AnimationController> animationController_;

	VECTOR targetPos;

	Quaternion damageRot_;

	// かめはめ波で押し出される方向
	VECTOR kamehamePushDir_ = { 0.0f, 0.0f, 0.0f };

	// 移動速度
	float moveSpeed_;

	// 高速接近
	bool isBoostChase_;
	float boostChaseTimer_;

	// この距離まで近づいたら止まる
	float stopRange_;

	// 攻撃中
	bool isAttack_;

	// 何段目の攻撃か
	int attackCombo_;

	// 攻撃開始からの時間
	float attackTimer_;

	// 攻撃のクールタイム
	float attackCoolTimer_;
	bool isAttackWait_;
	float attackWaitTimer_;

	// プレイヤーに当たったか
	bool hasAttackHit_;

	// ダメージ中
	bool isDamage_;
	float damageTimer_;

	// かめはめ波を受けている
	bool isKamehameHit_ = false;
	float kamehamePushSpeed_ = 0.0f;

	// ガード
	bool isGuard_;
	float guardTimer_;
	float guardHp_;

	// ガードブレイク
	bool isGuardBreak_;
	float guardBreakTimer_;
	float guardCoolTimer_;

	// 叩き落とし中
	bool isSlamDown_;

	// ダウン中
	bool isDown_;
	float downTimer_;

};
