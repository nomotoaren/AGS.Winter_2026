#pragma once
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <DxLib.h>
#include "../ActorBase.h"
#include "../KiBlast.h"
#include "PlayerHud.h"
#include "PlayerConfig.h"
#include "PlayerKamehameha.h"
#include "PlayerTransform.h"
#include "PlayerBoostChase.h"
#include "PlayerGuard.h"
#include "PlayerAttack.h"
#include "../../Manager/ResourceManager.h"	// FormData で ResourceManager::SRC を使うため

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

	// 最大HP(HUDの表示にも使う)
	static constexpr int MAX_HP = 100;

	static constexpr float ATTACK_END_WAIT = 0.15f;

	// かめはめ波の発射までの時間
	static constexpr float KAMEHAME_SHOT_TIME = PlayerConfig::Kamehame::SHOT_TIME;
	static constexpr float KAMEHAME_END_TIME = PlayerConfig::Kamehame::END_TIME;
	static constexpr float KAMEHAME_BEAM_LENGTH = PlayerConfig::Kamehame::BEAM_LENGTH;
	static constexpr float KAMEHAME_BEAM_RADIUS = PlayerConfig::Kamehame::BEAM_RADIUS;
	static constexpr float KAMEHAME_TRANSITION_TIME = PlayerConfig::Kamehame::TRANSITION_TIME;

	// 気
	static constexpr float MAX_KI = 100.0f;
	static constexpr float KI_CHARGE_SPEED = 20.0f;
	static constexpr float KAMEHAME_KI_COST = PlayerConfig::Kamehame::KI_COST;

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

	// 形態(Player.cpp の kFormData から参照するので public)
	enum class FORM
	{
		BASE,
		SUPER,
		MAX
	};

	// 形態ごとのデータ
	struct FormData
	{
		ResourceManager::SRC model;	// モデル
		const char* animDir;		// アニメーションのフォルダ
		float scale;				// モデルのスケール
		float speedRate;			// 移動速度の倍率
		float attackRate;			// 攻撃力の倍率
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

	// 衝突判定に用いるコライダ制御
	void AddCollider(std::weak_ptr<Collider> collider);
	void ClearCollider(void);

	// 衝突用カプセルの取得
	const Capsule& GetCapsule(void) const;

	// 攻撃判定
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

	// 敵と重ならないように位置をずらす(GameScene から呼ぶ)
	void PushOut(const VECTOR& offset);

	// 死亡しているか
	bool IsDead(void) const;

	// HP取得
	int GetHp(void) const;

	bool IsRecording(void) const;

	// かめはめ波
	void DrawKamehame(void);

	bool IsKamehameBeam(void) const;
	VECTOR GetKamehameStartPos(void) const;
	VECTOR GetKamehameEndPos(void) const;
	VECTOR GetKamehameDir(void) const { return kamehame_.GetDir(); }	// 敵側の判定用
	float GetKamehameRadius(void) const;

	bool IsKamehame(void) const;

	int GetCombo(void) const;

	void SetAttackTarget(VECTOR pos);
	void ClearAttackTarget(void);
	void SetCanChase(bool canChase);
	bool CanChase(void) const;

	bool UseKi(float amount);
	float GetKi(void) const { return ki_; }	// HUD表示用

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
	bool IsGuardBurst(void) const;
	bool IsGuardBurstTrigger(void) const;

	// 変身
	bool IsTransforming(void) const;
	float GetAttackRate(void) const;	// 形態ごとの攻撃力倍率
	// 敵に当てたときのダメージ数字(worldPos = 3D位置)
	void AddDamagePopup(const VECTOR& worldPos, int damage) { hud_.AddDamagePopup(worldPos, damage); }

	FORM GetForm(void) const { return form_; }	// HUD表示用
	int GetAttackDamage(int baseDamage) const;	// 基本ダメージに倍率をかけた値(最低1)

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

		// 気弾
		static constexpr float KiBlastShotTime = 0.20f;
		static constexpr float KiBlastEndTime = 0.45f;

		// かめはめ波の狙う位置
		static constexpr float EnemyCenterHeight = 100.0f;	// 敵の足元から中心までの高さ

		// 汎用比較
		static constexpr float Epsilon = 0.001f;
	};

	// アニメーション
	std::unique_ptr<AnimationController> animationController_;

	// 気弾
	std::vector<std::unique_ptr<KiBlast>> kiBlasts_;

	// 状態管理
	STATE state_;
	// 状態管理(状態遷移時の初期処理)
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

	// 衝突判定に用いるコライダ
	std::vector<std::weak_ptr<Collider>> colliders_;
	std::unique_ptr<Capsule> capsule_;

	// 衝突チェック
	VECTOR gravHitPosDown_;
	VECTOR gravHitPosUp_;

	// 丸影
	int imgShadow_;

	// 近接攻撃(追撃・コンボの状態は PlayerAttack に任せる)
	PlayerAttack attack_;
	PlayerAttack::Context MakeAttackContext(void) const;


	bool hasAttackTarget_;
	VECTOR attackTargetPos_;
	bool canChase_;

	// 攻撃時の残像
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
	PlayerGuard guard_;		// ガード(状態と耐久値は PlayerGuard に任せる)

	// 死亡
	bool isDead_;

	// 初期化
	void InitAnimation(const std::string& animDir);
	void InitFrames(void);

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

	// 浮遊の見た目(当たり判定には影響しない)
	void UpdateFloatLean(void);
	void ApplyVisualTransform(const Quaternion& logicRot);
	bool IsFloatSelfMove(void) const;			// 自分で移動している状態か(攻撃・回避などではない)

	// 浮遊移動中の脚のポーズ(アニメの上から脚のボーンだけ寄せる)
	void InitLegFrames(void);
	void UpdateLegPose(void);

	// 描画系
	void DrawShadow(void);
	void DrawKiBlast(void);
	void DrawAfterImage(const MATRIX& matrix, float opacity, bool disableLighting);

	// 操作
	void ProcessMove(void);
	void ProcessJump(void);
	void PlayMoveAnimation(bool isRun);

	// ベクトル・向きの補助
	static VECTOR ToHorizontal(VECTOR v);		// Y成分を捨てる
	static VECTOR NormalizeSafe(VECTOR v);		// 長さがあるときだけ正規化
	void FaceDirection(VECTOR dir);				// 正規化済みの方向を向く
	bool FaceHorizontal(VECTOR dir);			// 水平成分だけで向く
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
	void FinishAttack(void);
	void AdvanceCombo(void);
	void LeaveAfterImage(void);
	void UpdateKiBlast(void);
	void ShotKiBlast(void);
	void UpdateKamehame(void);

	// 高速追撃
	void UpdateCharge(void);
	void EndChase(bool startAttack);
	void EndBoostChase(void);

	// 気溜め
	void UpdateChase(void);
	void StopCharge(void);
	void BeginChargeEnding(void);

	void UpdateDodge(void);
	void UpdateGuard(void);

	// 変身
	void UpdateTransform(void);
	void ApplyForm(FORM form);

	float attackEndTimer_;

	// かめはめ波
	PlayerKamehameha kamehame_;		// かめはめ波(流れは PlayerKamehameha に任せる)
	PlayerKamehameha::Context MakeKamehameContext(void) const;
	int rightHandFrame_;

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

	// ロックオン
	bool isLockOn_;
	float lockOnPitch_;

	PlayerBoostChase boostChase_;	// 高速接近(軌道の計算は PlayerBoostChase に任せる)

	// 回避
	bool isDodge_;
	float dodgeTimer_;
	VECTOR dodgeDir_;

	// 飛行
	bool isFlying_;

	// 浮遊の見た目
	float floatPitch_ = 0.0f;		// 前後の傾き(前傾がプラス)
	float floatRoll_ = 0.0f;		// 左右の傾き
	float floatBobTime_ = 0.0f;		// 上下ゆれの時間

	// 脚のポーズ
	enum LEG_FRAME
	{
		LEG_L_UP, LEG_L_KNEE, LEG_L_FOOT,
		LEG_R_UP, LEG_R_KNEE, LEG_R_FOOT,
		LEG_MAX
	};
	int legFrames_[LEG_MAX] = { -1, -1, -1, -1, -1, -1 };
	float legPoseRate_ = 0.0f;		// 0:アニメのまま ~ 1:完全にポーズ
	VECTOR legAxisSide_[LEG_MAX] = {};	// 各ボーンから見た「キャラの左右の軸」(脚を前後に振る軸)
	VECTOR legAxisFront_[LEG_MAX] = {};	// 各ボーンから見た「キャラの前後の軸」(脚を横に開く軸)

	// 脚のポーズの値(ゲーム中に調整できるよう変数で持つ。初期値は PlayerConfig::LegPose)
	enum LEG_TUNE
	{
		TUNE_L_THIGH, TUNE_L_KNEE, TUNE_L_OPEN, TUNE_R_THIGH, TUNE_R_KNEE, TUNE_R_OPEN, TUNE_FOOT, TUNE_LEAN,
		TUNE_MAX
	};
	float legTune_[TUNE_MAX] = {};
	int legTuneSel_ = 0;			// 調整中の項目
	bool isLegTune_ = false;		// 調整モード中か(F1)
	void UpdateLegTune(void);		// 調整モードの入力(Debug ビルドのみ)
	void DrawLegTune(void) const;	// 調整モードの表示(Debug ビルドのみ)

	// HUD(HP・気ゲージ)
	PlayerHud hud_;

	// 変身
	FORM form_;
	FORM nextForm_;
	PlayerTransform formChange_;	// 変身の流れ(流れは PlayerTransform に任せる)
	float chargeEndTimer_ = 0.0f;			// 気溜め終了モーションの経過時間(止まり防止用)
};