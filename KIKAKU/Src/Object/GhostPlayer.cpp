#include "../Application.h"
#include "GhostPlayer.h"
#include "../Manager/ResourceManager.h"
#include "../Utility/AsoUtility.h"
#include "Common/AnimationController.h"
#include "../Manager/SceneManager.h"
#include "Enemy/EnemyBase.h"

GhostPlayer::GhostPlayer(void)
    :
    playbackFrame_(0),
    isPlaying_(false),
    isActive_(false),
    hp_(MAX_HP),
    isAttack_(false),
    attackTimer_(0.0f),
    hasAttackHit_(false),
    attackCoolTimer_(0.0f),
    ghostState_(GHOST_STATE::INACTIVE),
    disappearTimer_(0.0f)
{
}

GhostPlayer::~GhostPlayer(void)
{
}

void GhostPlayer::Init(void)
{
    records_.clear();

    playbackFrame_ = 0;
    isPlaying_ = false;
    attackCoolTimer_ = 0.0f;

    isPlaying_ = false;
    isActive_ = false;
    hp_ = MAX_HP;

    transform_.SetModel(
        resMng_.LoadModelDuplicate(
            ResourceManager::SRC::PLAYER
        )
    );

    transform_.scl =
    {
        150.54f,
        150.54f,
        150.54f
    };

    transform_.pos = AsoUtility::VECTOR_ZERO;
    transform_.quaRot = Quaternion();

    transform_.quaRotLocal =
        Quaternion::Euler({
            0.0f,
            AsoUtility::Deg2RadF(180.0f),
            0.0f
        });

    transform_.Update();

    // アニメーション
    std::string path =
        Application::PATH_MODEL + "Player/";

    animationController_ =
        std::make_unique<AnimationController>(
            transform_.modelId
        );

    animationController_->Add(
        (int)Player::ANIM_TYPE::IDLE,
        path + "Idle.mv1",
        20.0f
    );

    animationController_->Add(
        (int)Player::ANIM_TYPE::RUN,
        path + "Run.mv1",
        20.0f
    );

    animationController_->Add(
        (int)Player::ANIM_TYPE::FAST_RUN,
        path + "FastRun.mv1",
        20.0f
    );

    animationController_->Add(
        (int)Player::ANIM_TYPE::JUMP,
        path + "Jump.mv1",
        60.0f
    );

    animationController_->Add(
        (int)Player::ANIM_TYPE::WARP_PAUSE,
        path + "WarpPose.mv1",
        60.0f
    );

    animationController_->Add(
        (int)Player::ANIM_TYPE::FLY,
        path + "Flying.mv1",
        60.0f
    );

    animationController_->Add(
        (int)Player::ANIM_TYPE::FALLING,
        path + "Falling.mv1",
        80.0f
    );

    animationController_->Add(
        (int)Player::ANIM_TYPE::VICTORY,
        path + "Victory.mv1",
        60.0f
    );

 //   animationController_->Add(
 //       (int)Player::ANIM_TYPE::ATTACK,
 //       path + "Punching.mv1",
 //       45.0f
	//);

    currentAnim_ = Player::ANIM_TYPE::IDLE;

    animationController_->Play(
        (int)currentAnim_
    );
}

void GhostPlayer::Update(void)
{
    float deltaTime =
        SceneManager::GetInstance().GetDeltaTime();

    if (attackEndRotateTimer_ > 0.0f)
    {
        attackEndRotateTimer_ -= deltaTime;

        if (attackEndRotateTimer_ < 0.0f)
        {
            attackEndRotateTimer_ = 0.0f;
        }
    }

    // 粒子更新
    for (auto& particle : particles_)
    {
        if (particle.life <= 0.0f)
        {
            continue;
        }

        particle.pos =
            VAdd(
                particle.pos,
                particle.velocity
            );

        particle.life -= deltaTime;

        if (particle.life < 0.0f)
        {
            particle.life = 0.0f;
        }
    }

    // Ghost本体がいなければここで終了
    if (!isActive_)
    {
        return;
    }

    // 消滅中
    if (ghostState_ == GHOST_STATE::DISAPPEAR)
    {
        disappearTimer_ += deltaTime;

        if (disappearTimer_ >= DISAPPEAR_TIME)
        {
            Destroy();
        }

        transform_.Update();

        return;
    }

    // 攻撃中
    if (isAttack_)
    {
        attackTimer_ += deltaTime;

        if (animationController_->IsEnd())
        {
            isAttack_ = false;
            attackTimer_ = 0.0f;

            attackEndRotateTimer_ =
                ATTACK_END_ROTATE_TIME;

            // 次の記録があるなら、
            // そのアニメーションへ即切り替える
            if (isPlaying_ &&
                playbackFrame_ <
                static_cast<int>(records_.size()))
            {
                const Player::PlayerRecord& nextRecord =
                    records_[playbackFrame_];

                currentAnim_ =
                    nextRecord.animType;

                animationController_->Play(
                    static_cast<int>(
                        currentAnim_
                        ),
                    true,
                    0.0f,
                    -1.0f,
                    false,
                    true
                );
            }
            else
            {
                currentAnim_ =
                    Player::ANIM_TYPE::IDLE;

                animationController_->Play(
                    (int)Player::ANIM_TYPE::IDLE,
                    true,
                    0.0f,
                    -1.0f,
                    false,
                    true
                );
            }
        }
    }

    // ルート再生
    if (isPlaying_)
    {
        if (playbackFrame_ >=
            static_cast<int>(records_.size()))
        {
            isPlaying_ = false;
        }
        else
        {
            const Player::PlayerRecord& record =
                records_[playbackFrame_];

            if (!isAttack_)
            {
                transform_.pos =
                    record.position;

                // 攻撃直後は敵を向いた方向を少し維持
                if (attackEndRotateTimer_ <= 0.0f)
                {
                    transform_.quaRot =
                        record.rotation;
                }

                if (currentAnim_ != record.animType)
                {
                    currentAnim_ =
                        record.animType;

                    animationController_->Play(
                        static_cast<int>(
                            currentAnim_
                            )
                    );
                }

                playbackFrame_++;
            }
        }
    }

    transform_.Update();

    animationController_->Update();
}

void GhostPlayer::Draw(void)
{
    // Ghost本体
    if (isActive_)
    {
        float alphaRate = 1.0f;

        if (ghostState_ == GHOST_STATE::DISAPPEAR)
        {
            float rate =
                disappearTimer_ /
                DISAPPEAR_TIME;

            if (rate > 1.0f)
            {
                rate = 1.0f;
            }

            alphaRate =
                1.0f - rate;
        }

        MV1SetOpacityRate(
            transform_.modelId,
            alphaRate
        );

        MV1DrawModel(
            transform_.modelId
        );

        // 青白い発光
        if (ghostState_ == GHOST_STATE::DISAPPEAR)
        {
            float rate =
                disappearTimer_ /
                DISAPPEAR_TIME;

            if (rate > 1.0f)
            {
                rate = 1.0f;
            }

            int alpha =
                static_cast<int>(
                    180.0f *
                    (1.0f - rate)
                    );

            SetDrawBlendMode(
                DX_BLENDMODE_ADD,
                alpha
            );

            MV1DrawModel(
                transform_.modelId
            );

            SetDrawBlendMode(
                DX_BLENDMODE_NOBLEND,
                255
            );
        }

        MV1SetOpacityRate(
            transform_.modelId,
            1.0f
        );
    }

    // 粒子
    for (const auto& particle : particles_)
    {
        if (particle.life <= 0.0f)
        {
            continue;
        }

        float rate =
            particle.life / 0.5f;

        if (rate > 1.0f)
        {
            rate = 1.0f;
        }

        int alpha =
            static_cast<int>(
                255.0f * rate
                );

        SetDrawBlendMode(
            DX_BLENDMODE_ADD,
            alpha
        );

        DrawSphere3D(
            particle.pos,
            3.0f,
            8,
            GetColor(150, 220, 255),
            GetColor(100, 180, 255),
            true
        );
    }

    SetDrawBlendMode(
        DX_BLENDMODE_NOBLEND,
        255
    );
}

void GhostPlayer::Start(
    const std::vector<Player::PlayerRecord>& records)
{
    if (records.empty())
    {
        return;
    }

	// Ghostの状態を初期化
    particles_.clear();

    records_ = records;

    playbackFrame_ = 0;

    ghostState_ = GHOST_STATE::ACTIVE;
    disappearTimer_ = 0.0f;

    // Ghost出現
    isActive_ = true;

    // 記録再生開始
    isPlaying_ = true;

    // 新しいGhostなのでHP全回復
    hp_ = MAX_HP;

    isAttack_ = false;
    attackTimer_ = 0.0f;
    hasAttackHit_ = false;
    attackCoolTimer_ = 0.0f;
}

bool GhostPlayer::IsPlaying(void) const
{
    return isPlaying_;
}

bool GhostPlayer::IsAttackHitTiming(void) const
{
    return
        isAttack_ &&
        attackTimer_ >= ATTACK_HIT_START &&
        attackTimer_ <= ATTACK_HIT_END;
}

bool GhostPlayer::HasAttackHit(void) const
{
    return hasAttackHit_;
}

void GhostPlayer::SetAttackHit(void)
{
    hasAttackHit_ = true;
}

VECTOR GhostPlayer::GetForward(void) const
{
    return transform_.quaRot.GetForward();
}

void GhostPlayer::UpdateAutoAttack(
    const std::vector<std::unique_ptr<EnemyBase>>& enemies)
{
    // Ghost再生中じゃなければ何もしない
    if (!isActive_)
    {
        return;
    }

    float deltaTime =
        SceneManager::GetInstance().GetDeltaTime();

    // クールタイム
    if (attackCoolTimer_ > 0.0f)
    {
        attackCoolTimer_ -= deltaTime;

        if (attackCoolTimer_ < 0.0f)
        {
            attackCoolTimer_ = 0.0f;
        }
    }

    // 攻撃中なら新しい攻撃はしない
    if (isAttack_)
    {
        return;
    }

    // クールタイム中
    if (attackCoolTimer_ > 0.0f)
    {
        return;
    }

    // 一番近いEnemyを探す
    EnemyBase* targetEnemy = nullptr;

    float nearestDistance =
        ATTACK_RANGE;

    for (auto& enemy : enemies)
    {
        if (!enemy)
        {
            continue;
        }

        if (enemy->IsDead())
        {
            continue;
        }

        VECTOR ghostPos =
            transform_.pos;

        VECTOR enemyPos =
            enemy->GetTransform().pos;

        VECTOR toEnemy =
            VSub(
                enemyPos,
                ghostPos
            );

        toEnemy.y = 0.0f;

        float distance =
            VSize(toEnemy);

        if (distance < nearestDistance)
        {
            nearestDistance =
                distance;

            targetEnemy =
                enemy.get();
        }
    }

    // 敵が近くにいない
    if (targetEnemy == nullptr)
    {
        return;
    }

    // 敵の方向を向く
    VECTOR toEnemy =
        VSub(
            targetEnemy->GetTransform().pos,
            transform_.pos
        );

    toEnemy.y = 0.0f;


    if (VSize(toEnemy) > 0.001f)
    {
        VECTOR dir =
            VNorm(toEnemy);

        transform_.quaRot =
            Quaternion::LookRotation(dir);

        transform_.Update();
    }

    // 自動攻撃開始
    isAttack_ = true;

    attackTimer_ = 0.0f;

    hasAttackHit_ = false;

    attackCoolTimer_ =
        ATTACK_COOL_TIME;


    //currentAnim_ =
    //    Player::ANIM_TYPE::ATTACK;

    //animationController_->Play(
    //    (int)Player::ANIM_TYPE::ATTACK,
    //    false
    //);
}

bool GhostPlayer::IsActive(void) const
{
    return isActive_;
}

int GhostPlayer::GetHp(void) const
{
    return hp_;
}

void GhostPlayer::Damage(int damage)
{
    if (!isActive_)
    {
        return;
    }

    // 消滅中はダメージを受けない
    if (ghostState_ == GHOST_STATE::DISAPPEAR)
    {
        return;
    }

    hp_ -= damage;

    if (hp_ <= 0)
    {
        hp_ = 0;

        // 消滅演出開始
        ghostState_ = GHOST_STATE::DISAPPEAR;

        disappearTimer_ = 0.0f;

        CreateDisappearParticles();
        // 行動停止
        isPlaying_ = false;
        isAttack_ = false;
    }
}

void GhostPlayer::Destroy(void)
{
    isActive_ = false;
    isPlaying_ = false;

    ghostState_ =
        GHOST_STATE::INACTIVE;

    records_.clear();

    isAttack_ = false;
    attackTimer_ = 0.0f;
    attackCoolTimer_ = 0.0f;
    hasAttackHit_ = false;

    disappearTimer_ = 0.0f;
}

bool GhostPlayer::IsTargetable(void) const
{
    return
        isActive_ &&
        ghostState_ == GHOST_STATE::ACTIVE;
}

void GhostPlayer::CreateDisappearParticles(void)
{
    particles_.clear();

    for (int i = 0; i < 20; i++)
    {
        GhostParticle particle;

        particle.pos =
            transform_.pos;

        particle.pos.x +=
            GetRand(80) - 40;

        particle.pos.y +=
            GetRand(120);

        particle.pos.z +=
            GetRand(80) - 40;

        particle.velocity =
        {
            (GetRand(100) - 50) * 0.02f,
            1.0f + GetRand(100) * 0.02f,
            (GetRand(100) - 50) * 0.02f
        };

        particle.life = 0.5f;

        particles_.push_back(
            particle
        );
    }
}
