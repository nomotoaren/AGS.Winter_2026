#pragma once

#include <vector>
#include <DxLib.h>

#include "ActorBase.h"
#include "Player.h"

class AnimationController;
class EnemyBase;

class GhostPlayer : public ActorBase
{
public:

    static constexpr int MAX_HP = 3;

    static constexpr int ATTACK_DAMAGE = 1;

    static constexpr float DISAPPEAR_TIME = 0.5f;

    static constexpr float ATTACK_COOL_TIME = 1.2f;

    static constexpr float ATTACK_END_ROTATE_TIME = 0.15f;

    enum class GHOST_STATE
    {
        INACTIVE,   // いない
        ACTIVE,     // 通常
        DISAPPEAR   // 消滅中
    };

    GHOST_STATE ghostState_;

    GhostPlayer(void);
    ~GhostPlayer(void);

    void Init(void);
    void Update(void);
    void Draw(void);

    void Start(
        const std::vector<Player::PlayerRecord>& records
    );

    bool IsPlaying(void) const;

    // 攻撃判定が有効か
    bool IsAttackHitTiming(void) const;

    // 既に攻撃が当たったか
    bool HasAttackHit(void) const;

    // 攻撃が当たったことを記録
    void SetAttackHit(void);

    // Ghostの前方向
    VECTOR GetForward(void) const;

    // 自動攻撃更新
    void UpdateAutoAttack(
        const std::vector<std::unique_ptr<EnemyBase>>& enemies
    );

    bool IsActive(void) const;

    void Damage(int damage);

    void Destroy(void);

    int GetHp(void) const;

    bool IsTargetable(void) const;
    void CreateDisappearParticles(void);

private:

    struct GhostParticle
    {
        VECTOR pos;
        VECTOR velocity;
        float life;
    };

    std::vector<GhostParticle> particles_;

    std::vector<Player::PlayerRecord> records_;

    int playbackFrame_;
    int hp_;

    bool isPlaying_;
    bool isActive_;

    std::unique_ptr<AnimationController>
        animationController_;

    Player::ANIM_TYPE currentAnim_;

    // 攻撃
    bool isAttack_;

    float attackTimer_;

    bool hasAttackHit_;

    float attackCoolTimer_;

	// 消滅中のタイマー
    float disappearTimer_;
    float attackEndRotateTimer_ = 0.0f;

    static constexpr float ATTACK_TIME = 0.5f;
    static constexpr float ATTACK_HIT_START = 0.15f;
    static constexpr float ATTACK_HIT_END = 0.30f;
    static constexpr float ATTACK_RANGE = 120.0f;
};