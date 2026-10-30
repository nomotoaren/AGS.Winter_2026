#pragma once
#include <vector>
#include <memory>
#include "../EnemyBase.h"
#include "../definition/EnemyKiBlast.h"
#include "../definition/EnemyAreaBlast.h"
#include "../../Common/AnimationController.h"

class Player;

class MeleeEnemy : public EnemyBase
{

public:

	// 攻撃のダメージ(互換のため残している)
	static constexpr int ATTACK_DAMAGE = 1;

	// 驚きの表示の時間
	static constexpr float SURPRISE_TIME = 0.5f;

	// 高速接近
	static constexpr float BOOST_CHASE_DISTANCE = 400.0f;
	static constexpr float BOOST_CHASE_SPEED = 11.0f;
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
	bool IsSuperArmor(void) const override;

	void InitAnimation(void);

	void LookAtPlayer(void) override;

	// 攻撃
	bool IsAttackHitTiming(void) const;

	bool HasAttackHit(void) const;
	void SetAttackHit(void);

	int GetAttackCombo(void) const;

	// ガード
	bool IsGuard(void) const override;
	void GuardDamage(float damage) override;
	void GuardBurst(void) override;

	// 叩き落とし開始
	void StartSlamDown(void) override;

	// 大きな気弾(相殺勝負用)
	bool GetBigBlast(VECTOR& pos, float& radius, int& damage) const override;	// あれば true
	void SetBigBlastFrozen(bool frozen) override;	// 止める/再開
	void KillBigBlast(void) override;
	void SetBigBlastPos(const VECTOR& pos) override;
	void CancelAttacksForHeatUp(void) override;

	// ビーム(相殺勝負用)
	bool GetBeam(VECTOR& origin, VECTOR& dir, float& radius, int& damage) const override;	// 発射中なら true
	void SetBeamClash(bool clash) override;	// ぶつかっている間は、止まって当たらない
	void EndBeam(void) override;				// ビームを終わらせる(大きな隙ができる)

	// ダウン中か
	bool IsDown(void) const;

	// かめはめ波を受けているか(dir: 押し出す向き)
	void SetKamehameHit(bool hit, VECTOR dir = { 0.0f, 0.0f, 0.0f }) override;

	// 今かわしたか(1回読むと false に戻る。「DODGED!」の表示などに使える)
	bool ConsumeDodged(void);

protected:

	// スタンが始まった・終わったとき
	void OnStunStart(void) override;
	void OnStunEnd(void) override;

private:

	// 行動パターン
	enum class AI_STATE
	{
		WAIT,			// 様子見
		SIDE_MOVE,		// 横移動
		BOOST_ATTACK,	// 高速接近してから攻撃
		ATTACK,			// 近接攻撃(近づいて4連撃)
		KI_CHARGE,		// 気を溜めて、気弾を撃つ
		RUSH_READY,		// 突進の構え
		RUSH,			// 突進
		BIG_CHARGE,		// 範囲大技の溜め(殴ると中断できる)
		GRAB_READY,		// 投げ(ガード不能)の構えと接近
		CHASE_VANISH,	// 追撃: 姿を消している
		CHASE_APPEAR,	// 追撃: 先回りして現れ、叩き落とす予告
		BEAM_CHARGE,	// ビームの溜め(途中で狙いを固定する)
		BEAM_FIRE,		// ビーム発射
		KI_BARRAGE,		// 気弾の連射
		SLAM_RISE,		// 上昇して溜める → 狙いを固定
		SLAM_DIVE		// 急降下
	};

	// 攻撃の時間
	static constexpr float ATTACK_COOL_TIME = 4.0f;		// 近接攻撃が終わってから、次の近接攻撃までの時間(秒)
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

	// 高速移動の軌跡の1点
	struct TrailPoint
	{
		VECTOR pos;
		float life;
		bool cut;		// 前の点とつながない(瞬間移動したとき)
	};

	void DrawTelegraph(void) const;
	void DrawTrail(void) const;
	void DrawAuraColumn(const VECTOR& base, float rate) const;

	// 更新
	bool UpdateKamehamePush(void);
	bool UpdateKnockBack(void);
	bool UpdateDown(float deltaTime);
	bool UpdateDamage(float deltaTime);
	bool UpdateGuardBreak(float deltaTime);
	bool UpdateGuard(float deltaTime);
	bool UpdateStunned(void);
	bool UpdateAttack(float deltaTime, const VECTOR& playerPos);
	bool UpdateKiCharge(float deltaTime, const VECTOR& playerPos);
	bool UpdateRush(float deltaTime, const VECTOR& playerPos);
	bool UpdateBigCharge(float deltaTime, const VECTOR& playerPos);
	bool UpdateGrab(float deltaTime, const VECTOR& playerPos);
	bool UpdateChase(float deltaTime, const VECTOR& playerPos);
	bool UpdateBeam(float deltaTime, const VECTOR& playerPos);
	bool UpdateKiBarrage(float deltaTime, const VECTOR& playerPos);
	bool UpdateSlam(float deltaTime, const VECTOR& playerPos);
	void UpdateTrail(float deltaTime);
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
	void FireBarrageShot(void);
	void FireBarrageBig(void);
	void SlamImpact(bool directHit);
	void LaunchAreaBlasts(const VECTOR& playerPos);
	void DoGrab(void);
	void DoSmash(void);

	// 追撃
	void QueueChase(void);
	void StartChase(void);

	// 空中での旋回(radius: プレイヤーとの距離 / heightOffset: プレイヤーとの高さの差 / speedScale: 速さの倍率)
	void OrbitPlayer(const AIContext& ctx, float speedScale, float radius, float heightOffset);

	// プレイヤーへの攻撃を当てる(かわされたら false)
	bool ApplyHitToPlayer(const VECTOR& fromPos, int damage, float knockPower,
		float guardDamage, float guardKnock);

	// 攻撃の補助
	void FollowPlayerWhileAttacking(const VECTOR& playerPos);
	void AdvanceCombo(void);
	void LookAtPlayerWithPitchLimit(const VECTOR& playerPos);
	VECTOR GetChestPos(void) const;
	VECTOR GetBeamOrigin(void) const;

	// 状態の補助
	void SetAIState(AI_STATE state);
	void ReturnToWait(void);
	void ResetAttack(void);
	void CancelBoostChase(void);
	void CancelSpecials(void);

	// 反撃(殴られ続けたら吹き飛ばす)と、見えない気(溜まると、ひるまずに大技を出す)
	void UpdateCounterAndKi(float deltaTime, bool isStunned);
	void DoCounterBurst(void);
	bool ForceSpecial(void);
	void StartBeamCharge(void);
	void StartBarrage(void);
	bool IsDoingSpecial(void) const;		// 大技・突進などの最中か

	// AI
	void UpdateAI(const AIContext& ctx);
	void UpdateAIWait(const AIContext& ctx);
	void UpdateAISideMove(const AIContext& ctx);
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

	std::unique_ptr<AnimationController> animationController_;

	Quaternion damageRot_;

	// かめはめ波で押し出される向き
	VECTOR kamehamePushDir_ = { 0.0f, 0.0f, 0.0f };

	// 高速接近
	bool isBoostChase_;
	float boostChaseTimer_;

	// 攻撃中
	bool isAttack_;

	// 何段目の攻撃か
	int attackCombo_;

	// 攻撃開始からの時間
	float attackTimer_;

	// 攻撃のクールタイム
	float attackCoolTimer_;

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

	// 叩き落とされ中
	bool isSlamDown_;

	// ダウン中
	bool isDown_;
	float downTimer_;

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

	// 気弾の連射
	float barrageCoolTimer_ = 0.0f;
	int barrageShotsLeft_ = 0;
	float barrageShotTimer_ = 0.0f;
	bool barrageBigPhase_ = false;		// 連射のあと、大きな気弾を溜めている
	float barrageBigTimer_ = 0.0f;

	// 急降下
	float slamCoolTimer_ = 0.0f;
	bool slamLocked_ = false;
	VECTOR slamTarget_ = { 0.0f, 0.0f, 0.0f };
	VECTOR slamDir_ = { 0.0f, -1.0f, 0.0f };

	// 高速移動の軌跡
	std::vector<TrailPoint> trail_;

	// ビーム
	float beamCoolTimer_ = 0.0f;
	int hitStreak_ = 0;				// 短い間に殴られた回数
	float hitStreakTimer_ = 0.0f;
	float hitGate_ = 0.0f;			// 連続で数えすぎないための間隔
	float burstCool_ = 0.0f;		// 反撃の再使用までの時間
	float burstFx_ = 0.0f;			// 反撃の衝撃波の表示時間
	float enemyKi_ = 0.0f;			// 敵の気(見えない)。自動で回復し、溜まると大技を出す
	bool clashLock_ = false;			// 相殺勝負中: 敵は動かず、攻撃を受けない
	bool beamClash_ = false;			// 相殺勝負中(ビームは止まり、ダメージも出ない)
	bool beamLocked_ = false;			// 狙いを固定したか
	float beamHitCool_ = 0.0f;			// 次にダメージが入るまでの時間(発射中、当たり続けると一定間隔で入る)
	VECTOR beamOrigin_ = { 0.0f, 0.0f, 0.0f };
	VECTOR beamDir_ = { 0.0f, 0.0f, 1.0f };

	// 追撃
	float chaseCoolTimer_ = 0.0f;
	bool pendingChase_ = false;			// 追撃する予定
	float pendingChaseTimer_ = 0.0f;
	VECTOR chaseDir_ = { 0.0f, 0.0f, 1.0f };
	VECTOR vanishFrom_ = { 0.0f, 0.0f, 0.0f };
	bool isHidden_ = false;

	// 直前の攻撃がダメージとして入ったか(ガード・回避なら false)と、そのときの向き
	bool lastHitDamaged_ = false;

	// プレイヤーが攻撃を始めた瞬間を知るための、前のフレームのコンボ数
	int prevPlayerCombo_ = 0;

};