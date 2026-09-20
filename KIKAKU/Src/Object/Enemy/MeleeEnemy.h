#pragma once
#include "EnemyBase.h"
#include <memory>
#include "../Common/AnimationController.h"

class Player;

class MeleeEnemy : public EnemyBase
{
public:
    static constexpr int ATTACK_DAMAGE = 1;

    static constexpr float SURPRISE_TIME = 0.5f;

    // 高速接近
    static constexpr float BOOST_CHASE_DISTANCE = 400.0f;
    static constexpr float BOOST_CHASE_SPEED = 18.0f;
    static constexpr float BOOST_CHASE_TIME = 0.6f;

    enum class ANIM_TYPE
    {
        IDLE,
        RUN,
        FAST_RUN,
        JUMP,
        WARP_PAUSE,
        FLY,
        FALLING,
        VICTORY,
        ATTACK01,
        ATTACK02,
        ATTACK03,
        ATTACK04,
        KAMEHAME,
        DAMAGE,
        GUARD,
    };

    MeleeEnemy(
        Player& player
    );

    ~MeleeEnemy(void);

    void Init(void) override;
    void Update(void) override;
    void Draw(void) override;

    // 位置設定
    void SetPosition(VECTOR pos);

    // 叩き落とし開始
    void StartSlamDown(void);
    void GuardBurst(void);

    bool IsDown(void) const;

    bool IsAttackHitTiming(void) const;

    bool HasAttackHit(void) const;
    void SetAttackHit(void);

    int GetAttackCombo(void) const;
    bool IsGuard(void) const;

    void InitAnimation(void);
private:

    // 追跡対象
    Player& player_;

    std::unique_ptr<AnimationController> animationController_;

    VECTOR targetPos;

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

    static constexpr float ATTACK_TIME = 0.8f;
    static constexpr float ATTACK_COOL_TIME = 1.5f;
    static constexpr float ATTACK_HIT_START = 0.25f;
    static constexpr float ATTACK_HIT_END = 0.45f;

    // プレイヤーに当たったか
    bool hasAttackHit_;

    bool showSurprise_;

    float surpriseTimer_;

    int surpriseHandle_;

    void DrawSyncRing(void);

    // ダメージ中
    bool isDamage_;
    float damageTimer_;

    // ガード
    bool isGuard_;
    float guardTimer_;

    // 叩き落とし中
    bool isSlamDown_;

    // ダウン中
    bool isDown_;
    float downTimer_;
};