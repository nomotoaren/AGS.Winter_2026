#pragma once
#include "../ActorBase.h"

class EnemyBase : public ActorBase
{
public:

    EnemyBase(void);
    virtual ~EnemyBase(void);

    void Init(void) override = 0;
    void Update(void) override = 0;
    void Draw(void) override = 0;

    // ダメージ
    virtual void Damage(int damage);

    // 死亡判定
    bool IsDead(void) const;

    // HP
    int GetHp(void) const;

    // 攻撃を受ける半径
    float GetHitRadius(void) const;

    // 時間停止中のダメージを蓄積
    void AddPendingDamage(int damage);

    // 蓄積ダメージを適用
    void ApplyPendingDamage();

    // 蓄積ダメージ取得
    int GetPendingDamage(void) const;

    // ノックバック
    void AddKnockBack(
        VECTOR dir,
        float power
    );


	//-------------------------
    // SYNC ATTACK
	//-------------------------
    // SYNC受付開始
    void StartSyncWindow(void);

    // SYNC受付時間更新
    void UpdateSyncWindow(float deltaTime);

    // SYNC可能か
    bool IsSyncReady(void) const;

    // SYNC受付終了
    void EndSyncWindow(void);

    float GetSyncRate(void) const;

protected:

    VECTOR knockBackPow_;

    int hp_;

    bool isDead_;

    float hitRadius_;

    // 時間停止中に蓄積したダメージ
    int pendingDamage_;

	//-------------------------
    // SYNC ATTACK
	//------------------------- 
    // SYNC受付中か
    bool isSyncReady_;

    // SYNC受付時間
    float syncTimer_;

    // SYNC可能時間
    static constexpr float SYNC_TIME = 1.5f;
};