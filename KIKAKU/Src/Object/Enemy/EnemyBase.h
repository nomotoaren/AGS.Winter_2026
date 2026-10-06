#pragma once
#include <string>
#include "../ActorBase.h"
#include "EnemyHud.h"	

class EnemyBase : public ActorBase
{

public:

	// SYNC攻撃を受け付ける時間
	static constexpr float SYNC_TIME = 1.5f;

	EnemyBase(void);
	virtual ~EnemyBase(void);

	void Init(void) override = 0;
	void Update(void) override = 0;
	void Draw(void) override = 0;

	// ダメージ
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

	// SYNC受付開始
	void StartSyncWindow(void);

	// SYNC受付時間更新
	void UpdateSyncWindow(float deltaTime);

	// SYNC可能か
	bool IsSyncReady(void) const;

	// SYNC受付終了
	void EndSyncWindow(void);

	// SYNC受付の進行率
	float GetSyncRate(void) const;

protected:

	// HPバー(各敵の Init / Update / Draw から呼ぶ)
	void InitHud(const std::string& name, int maxHp);
	void UpdateHud(void);
	void DrawHud(void);

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

};