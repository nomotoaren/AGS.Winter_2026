#include <DxLib.h>
#include <cmath>
#include "MeleeEnemy.h"
#include "../../Application.h"
#include "../Player.h"
#include "../GhostPlayer.h"
#include "../../Manager/ResourceManager.h"
#include "../../Manager/SceneManager.h"
#include "../../Utility/AsoUtility.h"

MeleeEnemy::MeleeEnemy(
    Player& player,
    GhostPlayer& ghostPlayer
)
    :
    player_(player),
    ghostPlayer_(ghostPlayer),
    showSurprise_(false),
    surpriseTimer_(0.0f),
    surpriseHandle_(-1),
    moveSpeed_(2.0f),
    searchRange_(500.0f),
    stopRange_(80.0f),
    isAttack_(false),
    attackTimer_(0.0f),
    attackCoolTimer_(0.0f),
    hasAttackHit_(false),
    isTargetGhost_(false),
    targetLockTimer_(0.0f)
{
    hp_ = 100;
    hitRadius_ = 50.0f;
    isDamage_ = false;
    damageTimer_ = 0.0f;
}

MeleeEnemy::~MeleeEnemy(void)
{
}

void MeleeEnemy::Init(void)
{
    // âºÉÇÉfÉã
    transform_.SetModel(
        resMng_.LoadModelDuplicate(
            ResourceManager::SRC::PLAYER
        )
    );

    InitAnimation();

    surpriseHandle_ =
        LoadGraph(
            "Data/Image/Surprise.png"
        );

    // èâä˙à íu
    transform_.pos =
    {
        300.0f,
        -30.0f,
        0.0f
    };

    transform_.scl =
    {
        1.0f,
        1.0f,
        1.0f
    };

    transform_.quaRotLocal =
        Quaternion::Euler(
            0.0f,
            AsoUtility::Deg2RadF(180.0f),
            0.0f
        );

    transform_.Update();
}

void MeleeEnemy::InitAnimation(void)
{
    std::string path = Application::PATH_MODEL + "Player/";
    animationController_ = std::make_unique<AnimationController>(transform_.modelId);
    animationController_->Add((int)ANIM_TYPE::IDLE, path + "Idle.mv1", 20.0f);
    animationController_->Add((int)ANIM_TYPE::RUN, path + "Running.mv1", 20.0f);
    animationController_->Add((int)ANIM_TYPE::FAST_RUN, path + "Fast Run.mv1", 20.0f);
    animationController_->Add((int)ANIM_TYPE::JUMP, path + "Jumping.mv1", 60.0f);
    animationController_->Add((int)ANIM_TYPE::WARP_PAUSE, path + "WarpPose.mv1", 60.0f);
    animationController_->Add((int)ANIM_TYPE::FLY, path + "Flying.mv1", 60.0f);
    animationController_->Add((int)ANIM_TYPE::FALLING, path + "Falling.mv1", 80.0f);
    animationController_->Add((int)ANIM_TYPE::VICTORY, path + "Victory.mv1", 60.0f);
    animationController_->Add((int)ANIM_TYPE::ATTACK01, path + "Attack01.mv1", 45.0f);
    animationController_->Add((int)ANIM_TYPE::ATTACK02, path + "Attack02.mv1", 45.0f);
    animationController_->Add((int)ANIM_TYPE::ATTACK03, path + "Attack03.mv1", 45.0f);
    animationController_->Add((int)ANIM_TYPE::ATTACK04, path + "Attack04.mv1", 45.0f);
    animationController_->Add((int)ANIM_TYPE::KAMEHAME, path + "Ç©ÇﬂÇÕÇﬂîg.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::DAMAGE, path + "Damege.mv1", 60.0f);
    animationController_->Play((int)ANIM_TYPE::IDLE);
}

void MeleeEnemy::Update(void)
{
    if (isDead_)
    {
        return;
    }

    float deltaTime =
        SceneManager::GetInstance().GetDeltaTime();

    animationController_->Update();

    UpdateSyncWindow(deltaTime);

    // É^Å[ÉQÉbÉgÇåàÇﬂÇÈ
    // É^Å[ÉQÉbÉgå≈íËéûä‘Çå∏ÇÁÇ∑
    if (targetLockTimer_ > 0.0f)
    {
        targetLockTimer_ -= deltaTime;

        if (targetLockTimer_ < 0.0f)
        {
            targetLockTimer_ = 0.0f;
        }
    }

    if (targetLockTimer_ <= 0.0f)
    {
        // ëOâÒGhostÇë_Ç¡ÇƒÇ¢ÇΩÇ©
        bool wasTargetGhost =
            isTargetGhost_;

        // äÓñ{ÇÕPlayer
        bool nextTargetGhost =
            false;

        // GhostÇç≈óDêÊ
        if (ghostPlayer_.IsTargetable())
        {
            VECTOR ghostPos =
                ghostPlayer_.GetTransform().pos;

            VECTOR toGhost =
                VSub(
                    ghostPos,
                    transform_.pos
                );

            toGhost.y = 0.0f;

            float ghostDistance =
                VSize(toGhost);

            // GhostÇ™çıìGîÕàÕì‡Ç»ÇÁ
            // PlayerÇ∆ÇÃãóó£Ç…ä÷åWÇ»Ç≠GhostÇë_Ç§
            if (ghostDistance <= searchRange_)
            {
                nextTargetGhost = true;
            }
        }

        // Player Å® GhostÇ÷ïœÇÌÇ¡ÇΩèuä‘
        if (!wasTargetGhost &&
            nextTargetGhost)
        {
            showSurprise_ = true;
            surpriseTimer_ = 0.0f;
        }

        isTargetGhost_ =
            nextTargetGhost;

        // àÍíËéûä‘É^Å[ÉQÉbÉgå≈íË
        targetLockTimer_ =
            TARGET_LOCK_TIME;
    }

    VECTOR targetPos;

    // GhostÇë_Ç¡ÇƒÇ¢ÇƒÅAGhostÇ™Ç‹Çæë∂ç›Ç∑ÇÈ
    if (isTargetGhost_ &&
        ghostPlayer_.IsTargetable())
    {
        targetPos =
            ghostPlayer_.GetTransform().pos;
    }
    else
    {
        // GhostÇ™è¡Ç¶ÇΩèÍçáÇ»Ç«ÇÕPlayerÇ÷ñﬂÇ∑
        isTargetGhost_ = false;

        targetPos =
            player_.GetTransform().pos;
    }

    // É^Å[ÉQÉbÉgÇ‹Ç≈ÇÃï˚å¸ÅEãóó£
    VECTOR toTarget =
        VSub(
            targetPos,
            transform_.pos
        );

    toTarget.y = 0.0f;

    float distance =
        VSize(toTarget);

    // ÉmÉbÉNÉoÉbÉN
    if (VSize(knockBackPow_) > 0.1f)
    {
        transform_.pos =
            VAdd(
                transform_.pos,
                knockBackPow_
            );

        knockBackPow_ =
            VScale(
                knockBackPow_,
                0.85f
            );

        isDamage_ = true;
        damageTimer_ = 0.5f;

        animationController_->Play(
            (int)ANIM_TYPE::DAMAGE,
            false
        );

        transform_.Update();

        return;

        return;
    }
    else
    {
        knockBackPow_ =
            AsoUtility::VECTOR_ZERO;
    }

    if (isDamage_)
    {
        if (animationController_->IsEnd())
        {
            isDamage_ = false;

            animationController_->Play(
                (int)ANIM_TYPE::IDLE
            );
        }
        else
        {
            transform_.Update();
            return;
        }
    }

    if (showSurprise_)
    {
        surpriseTimer_ += deltaTime;

        if (surpriseTimer_ >= SURPRISE_TIME)
        {
            surpriseTimer_ = 0.0f;
            showSurprise_ = false;
        }
    }

    // çUåÇíÜ
    if (isAttack_)
    {
        attackTimer_ += deltaTime;

        if (attackTimer_ >= ATTACK_TIME)
        {
            isAttack_ = false;
            attackTimer_ = 0.0f;

            // çUåÇå„ÇÃë“Çøéûä‘
            attackCoolTimer_ =
                ATTACK_COOL_TIME;
        }

        transform_.Update();
        return;
    }

    // çUåÇÉNÅ[ÉãÉ^ÉCÉÄ
    if (attackCoolTimer_ > 0.0f)
    {
        attackCoolTimer_ -= deltaTime;

        if (attackCoolTimer_ < 0.0f)
        {
            attackCoolTimer_ = 0.0f;
        }
    }

    // çıìGîÕàÕ
    if (distance <= searchRange_)
    {
        VECTOR dir =
            AsoUtility::VECTOR_ZERO;

        if (distance > 0.001f)
        {
            dir = VNorm(toTarget);
        }

        // PlayerÇÃï˚å¸Çå¸Ç≠
        if (!AsoUtility::EqualsVZero(dir))
        {
            transform_.quaRot =
                Quaternion::LookRotation(dir);
        }

        if (distance > stopRange_)
        {
            transform_.pos =
                VAdd(
                    transform_.pos,
                    VScale(
                        dir,
                        moveSpeed_
                    )
                );
        }
        else
        {
            if (attackCoolTimer_ <= 0.0f)
            {
                isAttack_ = true;
                attackTimer_ = 0.0f;

                hasAttackHit_ = false;
            }
        }
    }

    transform_.Update();
}

void MeleeEnemy::Draw(void)
{
    if (isDead_)
    {
        return;
    }

    MV1DrawModel(
        transform_.modelId
    );

    // SYNCíÜÇæÇØë´å≥ÉäÉìÉO
    DrawSyncRing();

    if (showSurprise_ &&
        surpriseHandle_ != -1)
    {
        VECTOR markPos =
            transform_.pos;

        markPos.y += 150.0f;

        VECTOR screenPos =
            ConvWorldPosToScreenPos(
                markPos
            );

        if (screenPos.z >= 0.0f &&
            screenPos.z <= 1.0f)
        {
            float rate =
                surpriseTimer_ /
                SURPRISE_TIME;

            if (rate > 1.0f)
            {
                rate = 1.0f;
            }

            float scale;

            if (rate < 0.2f)
            {
                // 0.0 Å® 0.2 ÇÃä‘Ç≈ã}ägëÂ
                float appearRate =
                    rate / 0.2f;

                scale =
                    0.05f +
                    0.10f * appearRate;
            }
            else
            {
                // ÇªÇÃå„è≠ÇµèkÇﬁ
                float disappearRate =
                    (rate - 0.2f) / 0.8f;

                scale =
                    0.15f -
                    0.04f * disappearRate;
            }

            // ìßñæìx
            int alpha =
                static_cast<int>(
                    255.0f * (1.0f - rate)
                    );

            SetDrawBlendMode(
                DX_BLENDMODE_ALPHA,
                alpha
            );

            DrawRotaGraph(
                static_cast<int>(screenPos.x),
                static_cast<int>(screenPos.y),
                scale,
                0.0,
                surpriseHandle_,
                true
            );

            SetDrawBlendMode(
                DX_BLENDMODE_NOBLEND,
                255
            );
        }
    }

    DrawFormatString(
        20,
        180,
        GetColor(255, 255, 255),
        "Enemy HP : %d",
        hp_
    );
}

void MeleeEnemy::SetPosition(VECTOR pos)
{
    transform_.pos = pos;

    transform_.Update();
}

bool MeleeEnemy::IsAttackHitTiming(void) const
{
    return
        isAttack_ &&
        attackTimer_ >= ATTACK_HIT_START &&
        attackTimer_ <= ATTACK_HIT_END;
}

bool MeleeEnemy::HasAttackHit(void) const
{
    return hasAttackHit_;
}

void MeleeEnemy::SetAttackHit(void)
{
    hasAttackHit_ = true;
}

bool MeleeEnemy::IsTargetGhost(void) const
{
    return isTargetGhost_;
}

void MeleeEnemy::DrawSyncRing(void)
{
    if (!IsSyncReady())
    {
        return;
    }

    VECTOR center =
        transform_.pos;

    // ë´å≥ÇÊÇËè≠Çµè„
    center.y += 3.0f;

    // 1.0 Å® 0.0
    float rate =
        GetSyncRate();

    // écÇËéûä‘Ç≈â~Ç™èkÇﬁ
    float radius =
        80.0f * rate;

    static constexpr int SEGMENT = 48;

    unsigned int color =
        GetColor(
            80,
            220,
            255
        );

    // îºìßñæ
    SetDrawBlendMode(
        DX_BLENDMODE_ALPHA,
        90
    );

    for (int i = 0; i < SEGMENT; i++)
    {
        float angle1 =
            DX_TWO_PI_F *
            static_cast<float>(i) /
            static_cast<float>(SEGMENT);

        float angle2 =
            DX_TWO_PI_F *
            static_cast<float>(i + 1) /
            static_cast<float>(SEGMENT);


        VECTOR p1 =
        {
            center.x + cosf(angle1) * radius,
            center.y,
            center.z + sinf(angle1) * radius
        };

        VECTOR p2 =
        {
            center.x + cosf(angle2) * radius,
            center.y,
            center.z + sinf(angle2) * radius
        };


        // íÜêSÇ©ÇÁéOäpå`Çï¿Ç◊Çƒâ~ÇçÏÇÈ
        DrawTriangle3D(
            center,
            p2,
            p1,
            color,
            true
        );
    }

    SetDrawBlendMode(
        DX_BLENDMODE_NOBLEND,
        255
    );

    float outlineRadius =
        radius + 3.0f;

    for (int i = 0; i < SEGMENT; i++)
    {
        float angle1 =
            DX_TWO_PI_F *
            static_cast<float>(i) /
            static_cast<float>(SEGMENT);

        float angle2 =
            DX_TWO_PI_F *
            static_cast<float>(i + 1) /
            static_cast<float>(SEGMENT);

        VECTOR p1 =
        {
            center.x + cosf(angle1) * outlineRadius,
            center.y + 0.5f,
            center.z + sinf(angle1) * outlineRadius
        };

        VECTOR p2 =
        {
            center.x + cosf(angle2) * outlineRadius,
            center.y + 0.5f,
            center.z + sinf(angle2) * outlineRadius
        };

        DrawLine3D(
            p1,
            p2,
            GetColor(
                180,
                245,
                255
            )
        );
    }
}