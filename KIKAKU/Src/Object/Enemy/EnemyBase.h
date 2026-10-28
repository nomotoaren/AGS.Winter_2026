#pragma once
#include <string>
#include "../ActorBase.h"
#include "../Enemy/definition/Enemyhud.h"
#include "../Enemy/definition/EnemyStunBar.h"
#include "../Enemy/definition/EnemyData.h"

class Player;

class EnemyBase : public ActorBase
{

public:

	// SYNC攻撃を受け付ける時間
	static constexpr float SYNC_TIME = 1.5f;

	// スタンゲージ(ダメージを受けると溜まり、満タンで敵が動けなくなる)
	static constexpr float STUN_MAX = 100.0f;			// ゲージの最大
	static constexpr float STUN_TIME = 5.0f;			// スタンしている時間(秒)
	static constexpr float STUN_DECAY_DELAY = 3.0f;		// 攻撃されなくなってから、ゲージが減り始めるまでの時間(秒)
	static constexpr float STUN_DECAY_SPEED = 0.0f;	// ゲージが減る速さ(1秒あたり)
	static constexpr float STUN_RESIST_TIME = 6.0f;		// スタンが終わったあと、ゲージが溜まらない時間(秒)
	static constexpr int FLINCH_CHANCE = 100;			// 弱い攻撃でひるむ確率(%)。低いと、殴られても行動を続ける
	static constexpr float HEAVY_KNOCK = 20.0f;		// これ以上の吹き飛ばしは必ずひるむ(フィニッシュ・かめはめ波など)
	static constexpr float ARMOR_KNOCK_RATE = 0.25f;	// ひるまないときの、飛ばされる強さの倍率
	static constexpr int STUN_DAMAGE_RATE = 2;			// スタン中に受けるダメージの倍率

	// プレイヤーに攻撃を当てた結果
	enum class HIT_RESULT
	{
		DODGED,		// 回避された(ダメージなし)
		GUARDED,	// ガードされた(ガード耐久値だけ減る)
		DAMAGED		// ダメージが入った
	};

	EnemyBase(Player& player);
	virtual ~EnemyBase(void);

	void Init(void) override = 0;
	void Update(void) override = 0;
	void Draw(void) override = 0;

	// ダメージ(スタン中は倍率がかかる)
	virtual void Damage(int damage);

	// 種類ごとのデータ(名前・HP・大きさなど)を反映する。Init の前に呼ぶ(EnemyFactory が呼ぶ)
	void ApplyInfo(const EnemyData::Info& info);

	// 敵のHPを決める(Init の前に呼ぶ。HPバーが最大HPを使うため)
	void SetMaxHp(int hp);

	// 位置を決める
	virtual void SetPosition(VECTOR pos);

	// 死亡判定
	bool IsDead(void) const;

	// HP取得
	int GetHp(void) const;

	// 攻撃を受ける半径
	float GetHitRadius(void) const;

	// ノックバック
	void AddKnockBack(VECTOR dir, float power);

	// プレイヤーと重ならないように位置をずらす(GameScene から呼ぶ)
	void PushOut(const VECTOR& offset);

	// スタンゲージを増やす(プレイヤーの攻撃が当たったときに GameScene から呼ぶ)
	void AddStun(float amount);

	// スタン中か
	bool IsStunned(void) const;

	// HPバーなどの2D表示(3Dより手前に出すため、Draw とは別に呼ぶ)
	void DrawOverlay(void) { DrawHud(); }

	// スタンゲージの割合(0~1)。スタン中は、残り時間の割合
	float GetStunRate(void) const;

	// 攻撃が当たらない状態か(大技の跳躍中など)。GameScene が判定の前に確認する
	virtual bool IsInvincible(void) const;

	// 攻撃中などで、弱い攻撃を受けても絶対にひるまない(スーパーアーマー)
	virtual bool IsSuperArmor(void) const { return false; }

	// ---- GameScene / 相殺処理(ClashSystem)から呼ばれる操作 ----
	//   使わない敵は、何もしないこの初期実装のままでよい(呼ぶ側は敵の種類を調べなくてよい)

	// プレイヤーの方を向く
	virtual void LookAtPlayer(void) {}

	// ガード
	virtual bool IsGuard(void) const { return false; }
	virtual void GuardDamage(float damage) { (void)damage; }
	virtual void GuardBurst(void) {}

	// 叩き落とし / 超必殺技で中断させる
	virtual void StartSlamDown(void) {}
	virtual void CancelAttacksForHeatUp(void) {}

	// かめはめ波を受けている(dir: 押し出す向き)
	virtual void SetKamehameHit(bool hit, VECTOR dir = { 0.0f, 0.0f, 0.0f }) { (void)hit; (void)dir; }

	// 大きな気弾(相殺用)。撃っていなければ false
	virtual bool GetBigBlast(VECTOR& pos, float& radius, int& damage) const { (void)pos; (void)radius; (void)damage; return false; }
	virtual void SetBigBlastFrozen(bool frozen) { (void)frozen; }
	virtual void KillBigBlast(void) {}
	virtual void SetBigBlastPos(const VECTOR& pos) { (void)pos; }

	// ビーム(相殺用)。撃っていなければ false
	virtual bool GetBeam(VECTOR& origin, VECTOR& dir, float& radius, int& damage) const { (void)origin; (void)dir; (void)radius; (void)damage; return false; }
	virtual void SetBeamClash(bool clash) { (void)clash; }
	virtual void EndBeam(void) {}

	// SYNC受付開始
	void StartSyncWindow(void);

	// SYNC受付時間更新
	void UpdateSyncWindow(float deltaTime);

	// SYNC可能か
	bool IsSyncReady(void) const;

	// SYNC受付終了
	void EndSyncWindow(void);

	// SYNC受付の残り割合
	float GetSyncRate(void) const;

protected:

	// HPバー(各敵の Init / Update / Draw から呼ぶ)
	void InitHud(const std::string& name, int maxHp);
	void UpdateHud(void);
	void DrawHud(void);

	// 技の名前を画面に出す(技を出した瞬間に各敵が呼ぶ)
	void ShowSkillName(const char* name);

	// プレイヤーに攻撃を当てる(回避・ガード・ダメージの判定をまとめて行う。どの敵でも同じ処理)
	//   fromPos: 攻撃の出どころ(押し出す向きを決める) / knockPower: ダメージ時に飛ばす強さ
	//   guardDamage, guardKnock: ガードされたときの、ガード耐久値の減り方と飛ばす強さ
	//   ダメージが入ったときは lastHitDir_ に当てた向きを残す
	HIT_RESULT HitPlayer(const VECTOR& fromPos, int damage, float knockPower,
		float guardDamage, float guardKnock);

	// スタンゲージの更新(各敵の Update で毎フレーム呼ぶ)
	//   戻り値: スタン中なら true
	bool UpdateStunGauge(float deltaTime);

	// スタンが始まった・終わったとき(各敵が、動きを止める・戻す処理を書く)
	virtual void OnStunStart(void) {}
	virtual void OnStunEnd(void) {}

	// ノックバック量
	VECTOR knockBackPow_;
	bool armoredHit_ = false;	// 今のノックバックは「ひるまない」弱い攻撃か

	// プレイヤー(狙う相手・攻撃を当てる相手)
	Player& player_;

	// HP
	int hp_;

	// 最大HP(HPバーと、HPが半分以下の強化状態の判定に使う)
	int maxHp_;

	// HPバーに出す名前
	std::string displayName_ = "ENEMY";

	// モデルの大きさ
	float modelScale_ = 1.0f;

	// プレイヤーに与えるダメージの倍率(1.0 が基準。敵ごと・難易度ごとに変えられる)
	float damageRate_ = 1.0f;

	// 直前にプレイヤーへダメージを与えたときの、当てた向き
	VECTOR lastHitDir_ = { 0.0f, 0.0f, 1.0f };

	// 死亡しているか
	bool isDead_;

	// 攻撃を受ける半径
	float hitRadius_;

	// 時間停止中に溜めたダメージ
	int pendingDamage_;

	// SYNC受付中か
	bool isSyncReady_;

	// SYNC受付の経過時間
	float syncTimer_;

private:

	// HPバー
	EnemyHud hud_;

	// スタンゲージ
	EnemyStunBar stunBar_;
	float stun_;				// ゲージの量(0~STUN_MAX)
	float stunDecayWait_;		// ゲージが減り始めるまでの待ち時間
	bool isStunned_;
	float stunTimer_;			// スタンの残り時間
	float stunResist_;			// スタン明けの、ゲージが溜まらない残り時間
	float stunBarTime_;			// ゲージの点滅用

	// 技の名前の帯
	std::string skillName_;
	int skillNameStartMs_ = 0;
	bool hasSkillName_ = false;

};