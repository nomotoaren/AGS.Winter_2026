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

    // ターゲットを固定時間
    static constexpr float TARGET_LOCK_TIME = 1.5f;

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
        DAMAGE
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

    bool IsAttackHitTiming(void) const;

    bool HasAttackHit(void) const;
    void SetAttackHit(void);
    bool IsTargetGhost(void) const;

    void InitAnimation(void);
private:

    // 追跡対象
    Player& player_;

    std::unique_ptr<AnimationController> animationController_;

    VECTOR targetPos;

    // 移動速度
    float moveSpeed_;

    // 索敵距離
    float searchRange_;

    // この距離まで近づいたら止まる
    float stopRange_;

    // 攻撃中
    bool isAttack_;

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

    // ターゲット固定時間
    float targetLockTimer_;

    bool showSurprise_;

    float surpriseTimer_;

    int surpriseHandle_;

    void DrawSyncRing(void);

    // ダメージ中
    bool isDamage_;
    float damageTimer_;
};