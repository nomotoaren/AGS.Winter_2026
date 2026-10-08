#pragma once
#include <string>
#include "../ActorBase.h"
#include "EnemyHud.h"
#include "EnemyStunBar.h"

class EnemyBase : public ActorBase
{

public:

	// SYNC攻撃を受け付ける時間
	static constexpr float SYNC_TIME = 1.5f;

	// スタンゲージ(ダメージを受けるとたまり、満タンで敵が動けなくなる)
	static constexpr float STUN_MAX = 100.0f;			// ゲージの最大
	static constexpr float STUN_TIME = 3.0f;			// スタンしている時間(秒)
	static constexpr float STUN_DECAY_DELAY = 3.0f;		// 攻撃されなくなってから、ゲージが減り始めるまでの時間(秒)
	static constexpr float STUN_DECAY_SPEED = 15.0f;	// ゲージが減る速さ(1秒あたり)
	static constexpr float STUN_RESIST_TIME = 6.0f;		// スタンが終わったあと、ゲージがたまらない時間(秒)
	static constexpr int STUN_DAMAGE_RATE = 2;			// スタン中に受けるダメージの倍率

	EnemyBase(void);
	virtual ~EnemyBase(void);

	void Init(void) override = 0;
	void Update(void) override = 0;
	void Draw(void) override = 0;

	// ダメージ(スタン中は倍率がかかる)
	virtual void Damage(int damage);

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

	// スタンゲージの割合(0~1)。スタン中は、残り時間の割合
	float GetStunRate(void) const;

	// 攻撃が当たらない状態か(回避の直後など)。GameScene が判定の前に確認する
	virtual bool IsInvincible(void) const;

	// SYNC受付開始
	void StartSyncWindow(void);

	// SYNC受付時間更新
	void UpdateSyncWindow(float deltaTime);

	// SYNC表示
	bool IsSyncReady(void) const;

	// SYNC受付終了
	void EndSyncWindow(void);

	// SYNC受付の進行率
	float GetSyncRate(void) const;

protected:

	// HPバー(各敵が Init / Update / Draw から呼ぶ)
	void InitHud(const std::string& name, int maxHp);
	void UpdateHud(void);
	void DrawHud(void);

	// スタンゲージの更新(各敵が Update で毎フレーム呼ぶ)
	//   戻り値: スタン中なら true
	bool UpdateStunGauge(float deltaTime);

	// スタンが始まった・終わったとき(各敵が、動きを止める・戻す処理を書く)
	virtual void OnStunStart(void) {}
	virtual void OnStunEnd(void) {}

	// ノックバック量
	VECTOR knockBackPow_;

	// HP
	int hp_;

	// 死亡しているか
	bool isDead_;

	// 攻撃を受ける半径
	float hitRadius_;

	// 時間停止中に蓄積したダメージ
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
	float stunResist_;			// スタン直後の、ゲージがたまらない残り時間
	float stunBarTime_;			// ゲージの点滅用

};