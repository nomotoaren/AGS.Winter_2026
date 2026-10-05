#pragma once
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <DxLib.h>
#include "ActorBase.h"
#include "KiBlast.h"

class AnimationController;
class Collider;
class Capsule;

class Player : public ActorBase
{
public:

	// スピード
	static constexpr float SPEED_MOVE = 5.0f;
	static constexpr float SPEED_RUN = 10.0f;

	// SHIFTで記録する時間
	static constexpr int SHIFT_RECORD_FRAME = 300;

	// 回転完了までの時間
	static constexpr float TIME_ROT = 1.0f;

	// ジャンプ力
	static constexpr float POW_JUMP = 35.0f;

	// ジャンプ受付時間
	static constexpr float TIME_JUMP_IN = 0.5f;

	static constexpr int ATTACK_DAMAGE = 1;

	static constexpr float ATTACK_END_WAIT = 0.15f;

	// かめはめ波の発射までの時間
	static constexpr float KAMEHAME_SHOT_TIME = 2.0f;
	static constexpr float KAMEHAME_END_TIME = 3.0f;
	static constexpr float KAMEHAME_BEAM_LENGTH = 2000.0f;
	static constexpr float KAMEHAME_BEAM_RADIUS = 25.0f;
	static constexpr float KAMEHAME_TRANSITION_TIME = 0.12f;

	// 気
	static constexpr float MAX_KI = 100.0f;
	static constexpr float KI_CHARGE_SPEED = 20.0f;
	static constexpr float KAMEHAME_KI_COST = 30.0f;

	// 状態
	enum class STATE
	{
		NONE,
		PLAY,
		WARP_RESERVE,
		WARP_MOVE,
		DEAD,
		VICTORY,
		END
	};

	// アニメーション種別
	enum class ANIM_TYPE
	{
		IDLE,
		RUN,
		FAST_RUN,
		LOCK_LEFT,
		LOCK_RIGHT,
		LOCK_LEFT_RUN,
		LOCK_RIGHT_RUN,
		LOCK_BACK,
		LOCK_BACK_RUN,
		BOOST_CHASE,
		ATTACK01,
		ATTACK02,
		ATTACK03,
		ATTACK04,
		ATTACK05,
		ATTACK06,
		ATTACK07,
		ATTACK08,
		KI_BLAST,
		KAMEHAME,
		CHARGE,
		DAMAGE,
		GUARD,
		GUARD_BREAK,
		GUARD_BURST,
	};

	struct BoostAfterImage
	{
		MATRIX matrix;
		float timer;
	};

	// コンストラクタ
	Player(void);

	// デストラクタ
	~Player(void);

	void Init(void) override;
	void Update(void) override;
	void Draw(void) override;

	void StopRecord(void);

	// 衝突判定に用いられるコライダ制御
	void AddCollider(std::weak_ptr<Collider> collider);
	void ClearCollider(void);

	// 衝突用カプセルの取得
	const Capsule& GetCapsule(void) const;

	// 攻撃中か
	bool IsAttack(void) const;
	bool IsAttackHitTiming(void) const;

	bool HasAttackHit(void) const;

	void SetAttackHit(void);

	// プレイヤーの前方向を取得
	VECTOR GetForward(void) const;

	// ダメージ
	void Damage(int damage);

	// ノックバック
	void AddKnockBack(VECTOR dir, float power);

	// 死亡しているか
	bool IsDead(void) const;

	// HP取得
	int GetHp(void) const;

	bool IsRecording(void) const;

	void DrawKamehame(void);

	bool IsKamehameBeam(void) const;
	VECTOR GetKamehameStartPos(void) const;
	VECTOR GetKamehameEndPos(void) const;
	float GetKamehameRadius(void) const;

	bool IsKamehame(void) const;

	int GetCombo(void) const;

	void SetAttackTarget(VECTOR pos);
	void ClearAttackTarget(void);
	void SetCanChase(bool canChase);
	bool CanChase(void) const;

	bool UseKi(float amount);

	void AddAttackMove(VECTOR dir, float power);

	const std::vector<std::unique_ptr<KiBlast>>&
		GetKiBlasts(void) const;

	void SetLockOn(bool lockOn);
	bool IsLockOn(void) const;

	void LookAtTarget(VECTOR targetPos);
	void LookAtDamageEnemy(VECTOR enemyPos);

	void UpdateBoostChase(void);

	bool IsDodging(void) const;

	// ガード
	bool IsGuard(void) const;
	void GuardDamage(float damage);
	bool IsGuardBreak(void) const;
	void UpdateGuardBurst(void);
	bool IsGuardBurst(void) const;
	bool IsGuardBurstTrigger(void) const;

private:

	// 定数グループ
	struct Constants
	{
		// 汎用しきい値 / 減衰
		static constexpr float KnockBackThreshold = 0.1f;
		static constexpr float KnockBackDamping = 0.8f;

		// ダメージ
		static constexpr float DamageStateTime = 0.35f;

		// 残像
		static constexpr float AfterImageDuration = 0.10f;
		static constexpr float BoostAfterImageDuration = 0.15f;
		static constexpr float BoostAfterImageInterval = 0.03f;

		// 移動 / 高さ
		static constexpr float VerticalMoveSpeed = 5.0f;
		static constexpr float MinHeight = 100.0f;
		static constexpr float MaxHeight = 2000.0f;

		// 影描画
		static constexpr float PlayerShadowHeight = 300.0f;
		static constexpr float PlayerShadowSize = 30.0f;

		// 回避
		static constexpr float DodgeSpeed = 12.0f;
		static constexpr float DodgeDuration = 0.18f;

		// ブースト関連
		static constexpr float BoostStopDistance = 70.0f;
		static constexpr float BoostMaxSpeed = 45.0f;
		static constexpr float BoostInitialDelay = 0.10f;
		static constexpr float BoostAccelWindow = 0.20f;

		// 追尾・攻撃
		static constexpr float AttackFollowRate = 0.25f;
		static constexpr float AttackDistance = 90.0f;
		static constexpr float ChaseSpeed = 18.0f;
		static constexpr float ChaseStopDistance = 80.0f;
		static constexpr float NextAttackPlayRateThreshold = 0.75f;
		static constexpr float Combo8FollowEnd = 0.50f;		// 8段目で敵への追従をやめる時間

		// コンボ中の瞬間移動
		static constexpr float WarpDistance = 90.0f;		// 4,6,7段目
		static constexpr float WarpBackDistance = 130.0f;	// 8段目の手前距離
		static constexpr float WarpHeight = 200.0f;			// 8段目の高さ

		// 気弾
		static constexpr float KiBlastShotTime = 0.20f;
		static constexpr float KiBlastEndTime = 0.45f;

		// ガード
		static constexpr float GuardMaxHp = 100.0f;
		static constexpr float GuardRecoverSpeed = 25.0f;
		static constexpr float GuardRecoverDelay = 2.0f;
		static constexpr float GuardBreakTime = 3.0f;
		static constexpr float GuardBurstKiCost = 20.0f;
		static constexpr float GuardBurstTime = 0.5f;

		// 汎用比較
		static constexpr float Epsilon = 0.001f;
	};

	// アニメーション
	std::unique_ptr<AnimationController> animationController_;

	// 気弾
	std::vector<std::unique_ptr<KiBlast>> kiBlasts_;

	// 状態管理
	STATE state_;
	// 状態管理(状態遷移時初期処理)
	std::map<STATE, std::function<void(void)>> stateChanges_;
	// 状態管理(更新ステップ)
	std::function<void(void)> stateUpdate_;

	// 移動スピード
	float speed_;

	// 移動方向
	VECTOR moveDir_;

	// 移動量
	VECTOR movePow_;

	Quaternion damageRot_;

	// 移動後の座標
	VECTOR movedPos_;
	float recordTime_;
	bool isRecording_;

	// 回転
	Quaternion playerRotY_;
	Quaternion goalQuaRot_;
	float stepRotTime_;

	// ジャンプ量
	VECTOR jumpPow_;

	// ジャンプ判定
	bool isJump_;

	// ジャンプの入力受付時間
	float stepJump_;

	// 衝突判定に用いられるコライダ
	std::vector<std::weak_ptr<Collider>> colliders_;
	std::unique_ptr<Capsule> capsule_;

	// 衝突チェック
	VECTOR gravHitPosDown_;
	VECTOR gravHitPosUp_;

	// 丸影
	int imgShadow_;

	// 攻撃中
	bool isAttack_;

	// 攻撃開始からの時間
	float attackTimer_;

	// 今回の攻撃が既にヒットしたか
	bool hasAttackHit_;
	// このフレームで攻撃を開始したか
	bool attackTrigger_;

	// コンボ
	int combo_;
	bool nextAttack_;
	bool hasAttackTarget_;
	VECTOR attackTargetPos_;
	bool canChase_;
	bool isChasing_;

	bool isAttack04Move_;
	float attack04MoveTimer_;
	// 高速移動の残像
	int afterImageModel_;
	bool isAfterImage_;
	float afterImageTimer_;

	VECTOR afterImagePos_;

	// 気を溜める
	bool isCharging_;
	bool isChargeEnding_;

	// 1回の攻撃時間
	static constexpr float ATTACK_TIME = 1.0f;

	// 実際に攻撃判定が出る時間
	static constexpr float ATTACK_HIT_START = 0.3f;
	static constexpr float ATTACK_HIT_END = 0.55f;

	// HP
	int hp_;

	// ノックバック
	VECTOR knockBackPow_;

	// ダメージ中
	bool isDamage_;
	float damageTimer_;

	// ガード中
	bool isGuard_;
	float guardHp_;
	bool isGuardBreak_;
	float guardBreakTimer_;
	float guardRecoverTimer_;
	bool isGuardBurst_;
	float guardBurstTimer_;
	bool guardBurstTrigger_;

	// 死亡
	bool isDead_;

	void InitAnimation(void);

	// 状態遷移
	void ChangeState(STATE state);
	void ChangeStateNone(void);
	void ChangeStatePlay(void);

	// 更新ステップ
	void UpdateNone(void);
	void UpdatePlay(void);

	// 更新
	void UpdateKnockBack(void);
	bool UpdateDamage(void);
	void UpdateAfterImages(void);

	// 描画系
	void DrawShadow(void);
	void DrawKiBlast(void);
	void DrawAfterImage(const MATRIX& matrix, float opacity, bool disableLighting);
	void DrawKamehameChargeModel(const VECTOR& pos, float scale, float rotSpeed);
	void UpdateKamehameLight(const VECTOR& chargePos);
	void SetKamehameLight(bool enable);

	// 操作
	void ProcessMove(void);
	void ProcessJump(void);
	void PlayMoveAnimation(bool isRun);

	// ベクトル・向きの補助
	static VECTOR ToHorizontal(VECTOR v);		// Y成分を捨てる
	static VECTOR NormalizeSafe(VECTOR v);		// 長さがあるときだけ正規化
	void FaceDirection(VECTOR dir);				// 正規化済みの方向を向く
	bool FaceHorizontal(VECTOR dir);			// 水平方向だけ見て向く
	void SetLockOnPitch(VECTOR dir);			// ロックオン時の上下の傾き
	bool GetMoveBasis(VECTOR& forward, VECTOR& right);

	// 回転
	void SetGoalRotate(double rotRad);
	void Rotate(void);

	// 衝突判定
	void Collision(void);
	void CollisionGravity(void);
	void CollisionCapsule(void);

	// 移動量の計算
	void CalcGravityPow(void);

	// 着地モーション終了
	bool IsEndLanding(void);

	// 攻撃関連
	void UpdateAttack(void);
	void StartAttack(void);
	void ResetAttackState(void);
	void FinishAttack(void);
	void FollowAttackTarget(void);
	void AdvanceCombo(void);
	void LeaveAfterImage(void);
	void UpdateKiBlast(void);
	void ShotKiBlast(void);
	void UpdateKamehame(void);

	// 高速追撃
	void UpdateCharge(void);

	// 気溜め
	void UpdateChase(void);
	void StopCharge(void);
	void BeginChargeEnding(void);

	void UpdateDodge(void);
	void UpdateGuard(void);

	float attackEndTimer_;

	// かめはめ波
	VECTOR kamehameDir_;
	bool isKamehame_;
	float kamehameTimer_;
	bool isKamehameBeam_;
	int leftHandFrame_;
	int rightHandFrame_;
	int kamehameLightHandle_;
	int kamehameChargeModel_;
	int kamehameBeamModel_;

	// 気
	float ki_;

	// 気弾
	bool isKiBlast_;
	bool isKiBlastShot_;
	float kiBlastTimer_;

	// 残像
	int afterImageAttachNo_;
	MATRIX afterImageMatrix_;
	std::vector<BoostAfterImage> boostAfterImages_;
	float boostAfterImageTimer_;

	// ロックオン
	bool isLockOn_;
	float lockOnPitch_;

	bool isBoostChase_;
	float boostChaseTimer_;

	// 回避
	bool isDodge_;
	float dodgeTimer_;
	VECTOR dodgeDir_;

	// 空中
	bool isFlying_;
};
