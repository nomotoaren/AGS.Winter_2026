#pragma once
#include <vector>
#include <memory>
#include "EnemyBase.h"
#include "EnemyKiBlast.h"
#include "EnemyAreaBlast.h"
#include "../Common/AnimationController.h"

class Player;

// 近接戦闘が得意な敵
//   行動: 空中を旋回しながら様子見 → 近接4連撃 / 気弾 / 突進 / 範囲大技 / 投げ を、距離とHPで選ぶ
//   追撃: 4連撃の締めや突進でプレイヤーを吹き飛ばしたら、消えて先回りして叩き落とす
//   ガード崩し: プレイヤーがガードばかりしていると、ガード不能の投げに来る
//   範囲大技: 赤い警告の球が出て、少し後に爆発する(範囲の外へ逃げる)
//   反応: プレイヤーが攻撃してくると、ガードか回避(背後へ瞬間移動して反撃)をする
//   スタン: スタンゲージが満タンになると動けなくなり、大ダメージを受ける
class MeleeEnemy : public EnemyBase
{

public:

	// 攻撃のダメージ(互換のため残している)
	static constexpr int ATTACK_DAMAGE = 1;

	// 驚きの表示の時間
	static constexpr float SURPRISE_TIME = 0.5f;

	// 高速接近
	static constexpr float BOOST_CHASE_DISTANCE = 400.0f;
	static constexpr float BOOST_CHASE_SPEED = 18.0f;
	static constexpr float BOOST_CHASE_TIME = 0.6f;

	// アニメーション種類
	enum class ANIM_TYPE
	{
		IDLE,
		RUN,
		FAST_RUN,
		ATTACK01,
		ATTACK02,
		ATTACK03,
		ATTACK04,
		KAMEHAME,		// 未使用(番号を変えないために残している)
		DAMAGE,
		GUARD,
		GUARD_BREAK
	};

	MeleeEnemy(Player& player);
	~MeleeEnemy(void);

	void Init(void) override;
	void Update(void) override;
	void Draw(void) override;

	// 回避中は攻撃を受けない
	void Damage(int damage) override;
	bool IsInvincible(void) const override;

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

	// 叩きつけ開始
	void StartSlamDown(void);

	// ダウン中か
	bool IsDown(void) const;

	// かめはめ波を受けているか(dir: 押し出す方向)
	void SetKamehameHit(bool hit, VECTOR dir = { 0.0f, 0.0f, 0.0f });

	// 回避した直後か(1回読むと false に戻る。「DODGED!」の表示などに使える)
	bool ConsumeDodged(void);

protected:

	// スタンが始まった・終わったとき
	void OnStunStart(void) override;
	void OnStunEnd(void) override;

private:

	// 行動パターン
	enum class AI_STATE
	{
		WAIT,			// 様子を見る
		MOVE,			// 間合いを整える
		SIDE_MOVE,		// 横移動
		BOOST_ATTACK,	// 高速接近してから攻撃
		ATTACK,			// 近接攻撃(近づいて4連撃)
		KI_CHARGE,		// 気を溜めて、気弾を撃つ
		RUSH_READY,		// 突進の構え
		RUSH,			// 突進
		BIG_CHARGE,		// 範囲大技の溜め(殴ると中断できる)
		GRAB_READY,		// 投げ(ガード不能)の構えと接近
		CHASE_VANISH,	// 追撃: 姿を消している
		CHASE_APPEAR	// 追撃: 先回りして現れ、叩き落とす構え
	};

	// 攻撃の時間
	static constexpr float ATTACK_TIME = 0.8f;
	static constexpr float ATTACK_COOL_TIME = 2.5f;		// 近接攻撃を終えてから、次の近接攻撃までの時間(秒)
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
	void DrawTelegraph(void) const;

	// 更新(戻り値: そのフレームの更新を終えたとき true)
	bool UpdateKamehamePush(void);
	bool UpdateKnockBack(void);
	bool UpdateDown(float deltaTime);
	bool UpdateDamage(float deltaTime);
	bool UpdateGuardBreak(float deltaTime);
	bool UpdateGuard(float deltaTime);
	bool UpdateStunned(float deltaTime);
	bool UpdateAttack(float deltaTime, const VECTOR& playerPos);
	bool UpdateKiCharge(float deltaTime, const VECTOR& playerPos);
	bool UpdateRush(float deltaTime, const VECTOR& playerPos);
	bool UpdateBigCharge(float deltaTime, const VECTOR& playerPos);
	bool UpdateGrab(float deltaTime, const VECTOR& playerPos);
	bool UpdateChase(float deltaTime, const VECTOR& playerPos);
	void UpdateBlasts(float deltaTime);
	void UpdateAreaBlasts(float deltaTime);
	void UpdateGuardWatch(float deltaTime, float distance);

	// プレイヤーの攻撃への反応(ガード / 回避)
	bool TryReactToPlayerAttack(float distance);
	void StartGuard(void);
	void StartDodge(void);

	// 攻撃
	void StartMeleeAttack(void);
	void CheckMeleeHit(void);
	void FireKiBlast(void);
	void LaunchAreaBlasts(const VECTOR& playerPos);
	void DoGrab(void);
	void DoSmash(void);

	// 追撃
	void QueueChase(void);
	void StartChase(void);

	// 空中での旋回(radius: プレイヤーとの距離 / heightOffset: プレイヤーとの高さの差 / speedScale: 速さの倍率)
	void OrbitPlayer(const AIContext& ctx, float speedScale, float radius, float heightOffset);

	// プレイヤーへの攻撃を当てる(回避されたら false)
	bool ApplyHitToPlayer(const VECTOR& fromPos, int damage, float knockPower,
		float guardDamage, float guardKnock);

	// 攻撃の補助
	void FollowPlayerWhileAttacking(const VECTOR& playerPos);
	void AdvanceCombo(void);
	void LookAtPlayerWithPitchLimit(const VECTOR& playerPos);
	VECTOR GetChestPos(void) const;

	// 状態の補助
	void SetAIState(AI_STATE state);
	void ReturnToWait(void);
	void ResetAttack(void);
	void CancelBoostChase(void);
	void CancelSpecials(void);		// 気弾・突進の途中なら、やめて様子見に戻る

	// AI
	void UpdateAI(const AIContext& ctx);
	void UpdateAIWait(const AIContext& ctx);
	void UpdateAISideMove(const AIContext& ctx);
	void UpdateAIMove(const AIContext& ctx);
	void UpdateAIBoostAttack(const AIContext& ctx);
	void UpdateAIAttack(const AIContext& ctx);
	void DecideAction(const AIContext& ctx);

	// AI
	AI_STATE aiState_ = AI_STATE::WAIT;
	float aiTimer_ = 0.0f;

	// 旋回の向き(1.0f / -1.0f)
	float sideMoveDir_ = 1.0f;

	// 旋回の目標(横移動に入るたびに決め直す)
	float strafeRadius_ = 300.0f;
	float strafeHeight_ = 0.0f;

	// ふわふわ浮く動きの時間
	float hoverTime_ = 0.0f;

	Player& player_;

	std::unique_ptr<AnimationController> animationController_;

	VECTOR targetPos;

	Quaternion damageRot_;

	// かめはめ波で押し出される向き
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

	// 叩きつけられ中
	bool isSlamDown_;

	// ダウン中
	bool isDown_;
	float downTimer_;

	// 最大HP(HPが半分以下になったら、気弾や突進を多く使う)
	int maxHp_ = 100;

	// 気弾
	float kiCoolTimer_ = 0.0f;
	std::vector<EnemyKiBlast> blasts_;

	// 突進
	float rushCoolTimer_ = 0.0f;
	VECTOR rushDir_ = { 0.0f, 0.0f, 1.0f };

	// 回避
	float dodgeCoolTimer_ = 0.0f;
	float invincibleTimer_ = 0.0f;
	bool justDodged_ = false;

	// 範囲大技
	float bigCoolTimer_ = 0.0f;
	std::vector<EnemyAreaBlast> areaBlasts_;

	// ガード崩し(投げ)
	float grabCoolTimer_ = 0.0f;
	float guardWatchTimer_ = 0.0f;		// プレイヤーがガードしている時間(長いほど投げに来る)

	// 追撃
	float chaseCoolTimer_ = 0.0f;
	bool pendingChase_ = false;			// 追撃する予定
	float pendingChaseTimer_ = 0.0f;
	VECTOR chaseDir_ = { 0.0f, 0.0f, 1.0f };
	VECTOR vanishFrom_ = { 0.0f, 0.0f, 0.0f };
	bool isHidden_ = false;

	// 直前の攻撃がダメージとして入ったか(ガード・回避なら false)と、そのときの向き
	bool lastHitDamaged_ = false;
	VECTOR lastHitDir_ = { 0.0f, 0.0f, 1.0f };

	// プレイヤーが攻撃を始めた瞬間を知るための、前のフレームのコンボ数
	int prevPlayerCombo_ = 0;

};