#include <DxLib.h>
#include <cmath>
#include "MeleeEnemy.h"
#include "../../Application.h"
#include "../Player.h"
#include "../../Manager/ResourceManager.h"
#include "../../Manager/SceneManager.h"
#include "../../Utility/AsoUtility.h"

MeleeEnemy::MeleeEnemy(Player& player)
    :
    player_(player),
    moveSpeed_(2.0f),
    stopRange_(80.0f),
    isBoostChase_(false),
    boostChaseTimer_(0.0f),
    isAttack_(false),
    attackCombo_(0),
    attackTimer_(0.0f),
    attackCoolTimer_(0.0f),
    hasAttackHit_(false),
    isAttackWait_(false),
    attackWaitTimer_(0.0f)
{
    hp_ = 100;
    hitRadius_ = 50.0f;

    isDamage_ = false;
    damageTimer_ = 0.0f;

    isSlamDown_ = false;
    isDown_ = false;
    downTimer_ = 0.0f;

    isGuard_ = false;
    guardTimer_ = 0.0f;
    guardHp_ = 100.0f;

    isGuardBreak_ = false;
    guardBreakTimer_ = 0.0f;
    guardCoolTimer_ = 0.0f;
}

MeleeEnemy::~MeleeEnemy(void)
{
}

void MeleeEnemy::Init(void)
{
    // 仮モデル
    transform_.SetModel(
        resMng_.LoadModelDuplicate(
            ResourceManager::SRC::PLAYER
        )
    );

    InitAnimation();

    // 初期位置
    transform_.pos = { 300.0f, 1000.0f, 0.0f };

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
    animationController_->Add((int)ANIM_TYPE::KAMEHAME, path + "かめはめ波.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::DAMAGE, path + "Damege.mv1", 60.0f);
    animationController_->Add((int)ANIM_TYPE::GUARD, path + "Block.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::GUARD_BREAK, path + "GuardBreak.mv1", 20.0f);
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

    if (isKamehameHit_)
    {
        kamehamePushSpeed_ += 0.35f;

        if (kamehamePushSpeed_ > 12.0f)
        {
            kamehamePushSpeed_ = 12.0f;
        }

        transform_.pos =
            VAdd(
                transform_.pos,
                VScale(
                    kamehamePushDir_,
                    kamehamePushSpeed_
                )
            );

        transform_.Update();

        return;
    }

    if (guardCoolTimer_ > 0.0f)
    {
        guardCoolTimer_ -= deltaTime;

        if (guardCoolTimer_ < 0.0f)
        {
            guardCoolTimer_ = 0.0f;
        }
    }

    // 攻撃クールタイム
    if (attackCoolTimer_ > 0.0f)
    {
        attackCoolTimer_ -= deltaTime;

        if (attackCoolTimer_ < 0.0f)
        {
            attackCoolTimer_ = 0.0f;
        }
    }

    VECTOR targetPos =
        player_.GetTransform().pos;

    VECTOR toTarget =
        VSub(
            targetPos,
            transform_.pos
        );

    float distance =
        VSize(toTarget);

    // ノックバック
    if (VSize(knockBackPow_) > 0.1f)
    {
        transform_.pos =
            VAdd(
                transform_.pos,
                knockBackPow_
            );

        const float MIN_HEIGHT = 100.0f;
        const float MAX_HEIGHT = 2000.0f;

        if (transform_.pos.y <= MIN_HEIGHT)
        {
            transform_.pos.y = MIN_HEIGHT;

            if (isSlamDown_)
            {
                isSlamDown_ = false;
                isDown_ = true;
                downTimer_ = 1.0f;

                knockBackPow_ =
                    AsoUtility::VECTOR_ZERO;

                animationController_->Play(
                    (int)ANIM_TYPE::DAMAGE,
                    false
                );

                transform_.Update();

                return;
            }

            knockBackPow_.y = 0.0f;
        }

        if (transform_.pos.y > MAX_HEIGHT)
        {
            transform_.pos.y = MAX_HEIGHT;
            knockBackPow_.y = 0.0f;
        }

        knockBackPow_ =
            VScale(
                knockBackPow_,
                0.85f
            );

        if (!isDamage_)
        {
            isDamage_ = true;
            damageTimer_ = 0.18f;

            animationController_->Play(
                (int)ANIM_TYPE::DAMAGE,
                false
            );
        }

        transform_.Update();

        return;
    }
    else
    {
        knockBackPow_ =
            AsoUtility::VECTOR_ZERO;
    }

    // ダウン中
    if (isDown_)
    {
        downTimer_ -= deltaTime;

        if (downTimer_ <= 0.0f)
        {
            downTimer_ = 0.0f;
            isDown_ = false;
            isDamage_ = false;
            damageTimer_ = 0.0f;

            aiState_ = AI_STATE::WAIT;
            aiTimer_ = 0.0f;

            animationController_->Play(
                (int)ANIM_TYPE::IDLE
            );
        }

        transform_.Update();

        return;
    }

    // ダメージモーション
    if (isDamage_)
    {
        transform_.quaRot =
            damageRot_;

        damageTimer_ -= deltaTime;

        if (damageTimer_ <= 0.0f)
        {
            damageTimer_ = 0.0f;
            isDamage_ = false;

            aiState_ = AI_STATE::WAIT;
            aiTimer_ = 0.0f;

            animationController_->Play(
                (int)ANIM_TYPE::IDLE
            );
        }
        else
        {
            transform_.Update();
            animationController_->Update();

            return;
        }
    }

    // ガードブレイク中
    if (isGuardBreak_)
    {
        guardBreakTimer_ -= deltaTime;

        if (guardBreakTimer_ <= 0.0f)
        {
            guardBreakTimer_ = 0.0f;
            isGuardBreak_ = false;

            guardHp_ = 100.0f;
            guardCoolTimer_ = 2.0f;

            aiState_ = AI_STATE::WAIT;
            aiTimer_ = 0.0f;

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

    // ガード中
    if (isGuard_)
    {
        guardTimer_ -= deltaTime;

        if (guardTimer_ <= 0.0f)
        {
            guardTimer_ = 0.0f;
            isGuard_ = false;

            aiState_ = AI_STATE::WAIT;
            aiTimer_ = 0.0f;

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

    // プレイヤーが近くで攻撃していたらガード
    if (distance <= 150.0f &&
        player_.GetCombo() > 0 &&
        guardCoolTimer_ <= 0.0f)
    {
        isGuard_ = true;
        guardTimer_ = 0.8f;

        isAttack_ = false;
        attackCombo_ = 0;
        attackTimer_ = 0.0f;
        hasAttackHit_ = false;

        isBoostChase_ = false;
        boostChaseTimer_ = 0.0f;

        aiState_ = AI_STATE::WAIT;
        aiTimer_ = 0.0f;

        animationController_->Play(
            (int)ANIM_TYPE::GUARD,
            true,
            0.0f,
            -1.0f,
            true,
            true
        );

        transform_.Update();

        return;
    }

    // 攻撃中
    if (isAttack_)
    {
        attackTimer_ += deltaTime;

        // 1～3段目は攻撃中もプレイヤーについていく
        if (attackCombo_ >= 1 &&
            attackCombo_ <= 3)
        {
            VECTOR playerPos =
                player_.GetTransform().pos;

            VECTOR toPlayer =
                VSub(
                    playerPos,
                    transform_.pos
                );

            VECTOR horizontalDir =
                toPlayer;

            horizontalDir.y = 0.0f;

            float horizontalDistance =
                VSize(horizontalDir);

            if (horizontalDistance > 0.001f)
            {
                horizontalDir =
                    VNorm(horizontalDir);
            }
            else
            {
                horizontalDir =
                    transform_.quaRot.GetForward();

                horizontalDir.y = 0.0f;

                if (VSize(horizontalDir) <= 0.001f)
                {
                    horizontalDir =
                        VGet(
                            0.0f,
                            0.0f,
                            1.0f
                        );
                }

                horizontalDir =
                    VNorm(horizontalDir);
            }

            const float attackDistance = 90.0f;

            VECTOR attackPos =
                VSub(
                    playerPos,
                    VScale(
                        horizontalDir,
                        attackDistance
                    )
                );

            VECTOR follow =
                VSub(
                    attackPos,
                    transform_.pos
                );

            const float followRate = 0.25f;

            transform_.pos =
                VAdd(
                    transform_.pos,
                    VScale(
                        follow,
                        followRate
                    )
                );

            VECTOR lookDir =
                VSub(
                    playerPos,
                    transform_.pos
                );

            VECTOR lookHorizontal =
                lookDir;

            lookHorizontal.y = 0.0f;

            float lookDistance =
                VSize(lookHorizontal);

            if (lookDistance > 0.001f)
            {
                float maxY =
                    lookDistance * 0.35f;

                if (lookDir.y > maxY)
                {
                    lookDir.y = maxY;
                }

                if (lookDir.y < -maxY)
                {
                    lookDir.y = -maxY;
                }
            }

            if (VSize(lookDir) > 0.001f)
            {
                lookDir =
                    VNorm(lookDir);

                transform_.quaRot =
                    Quaternion::LookRotation(
                        lookDir
                    );
            }
        }

        // 次の攻撃へ
        if (attackTimer_ >= 0.55f)
        {
            attackTimer_ = 0.0f;
            hasAttackHit_ = false;

            attackCombo_++;

            switch (attackCombo_)
            {
            case 2:
                animationController_->Play(
                    (int)ANIM_TYPE::ATTACK02,
                    false
                );
                break;

            case 3:
                animationController_->Play(
                    (int)ANIM_TYPE::ATTACK03,
                    false
                );
                break;

            case 4:
                animationController_->Play(
                    (int)ANIM_TYPE::ATTACK04,
                    false
                );
                break;

            default:
                isAttack_ = false;
                attackCombo_ = 0;
                attackTimer_ = 0.0f;
                hasAttackHit_ = false;

                attackCoolTimer_ =
                    ATTACK_COOL_TIME;

                aiState_ = AI_STATE::WAIT;
                aiTimer_ = 0.0f;

                animationController_->Play(
                    (int)ANIM_TYPE::IDLE
                );
                break;
            }
        }

        transform_.Update();

        return;
    }

    VECTOR playerPos =
        player_.GetTransform().pos;

    VECTOR toPlayer =
        VSub(
            playerPos,
            transform_.pos
        );

    VECTOR horizontalDir =
        toPlayer;

    horizontalDir.y = 0.0f;

    float horizontalDistance =
        VSize(horizontalDir);

    if (horizontalDistance > 0.001f)
    {
        horizontalDir =
            VNorm(horizontalDir);
    }
    else
    {
        horizontalDir =
            transform_.quaRot.GetForward();

        horizontalDir.y = 0.0f;

        if (VSize(horizontalDir) <= 0.001f)
        {
            horizontalDir =
                VGet(
                    0.0f,
                    0.0f,
                    1.0f
                );
        }

        horizontalDir =
            VNorm(horizontalDir);
    }

    // プレイヤーを見る
    VECTOR lookDir =
        VSub(
            playerPos,
            transform_.pos
        );

    VECTOR lookHorizontal =
        lookDir;

    lookHorizontal.y = 0.0f;

    float lookDistance =
        VSize(lookHorizontal);

    if (lookDistance > 0.001f)
    {
        float maxY =
            lookDistance * 0.35f;

        if (lookDir.y > maxY)
        {
            lookDir.y = maxY;
        }

        if (lookDir.y < -maxY)
        {
            lookDir.y = -maxY;
        }
    }

    if (VSize(lookDir) > 0.001f)
    {
        lookDir =
            VNorm(lookDir);

        transform_.quaRot =
            Quaternion::LookRotation(
                lookDir
            );
    }

    // 普段はこのくらい離れて戦う
    const float normalBattleDistance = 280.0f;

    VECTOR normalBattlePos =
        VSub(
            playerPos,
            VScale(
                horizontalDir,
                normalBattleDistance
            )
        );

    switch (aiState_)
    {
    case AI_STATE::WAIT:
    {
        aiTimer_ += deltaTime;

        animationController_->Play(
            (int)ANIM_TYPE::IDLE
        );

        // 離れていてもすぐには飛んでこない
        if (aiTimer_ >= 0.8f)
        {
            aiTimer_ = 0.0f;

            if (distance >= 650.0f)
            {
                aiState_ =
                    AI_STATE::BOOST_ATTACK;
            }
            else
            {
                aiState_ =
                    AI_STATE::SIDE_MOVE;

                sideMoveDir_ *= -1.0f;
            }
        }

        break;
    }

    case AI_STATE::SIDE_MOVE:
    {
        aiTimer_ += deltaTime;

        VECTOR sideDir =
        {
            horizontalDir.z,
            0.0f,
            -horizontalDir.x
        };

        sideDir =
            VScale(
                sideDir,
                sideMoveDir_
            );

        // 280からズレた分を少しずつ直す
        VECTOR distanceCorrection =
            VSub(
                normalBattlePos,
                transform_.pos
            );

        distanceCorrection.y = 0.0f;

        VECTOR sideMove =
            VScale(
                sideDir,
                2.0f
            );

        VECTOR correctionMove =
            VScale(
                distanceCorrection,
                0.08f
            );

        VECTOR move =
            VAdd(
                sideMove,
                correctionMove
            );

        if (VSize(move) > 3.0f)
        {
            move =
                VScale(
                    VNorm(move),
                    3.0f
                );
        }

        transform_.pos =
            VAdd(
                transform_.pos,
                move
            );

        // 高さはゆっくり合わせる
        float heightDiff =
            playerPos.y -
            transform_.pos.y;

        const float verticalSpeed = 2.0f;

        if (heightDiff > verticalSpeed)
        {
            transform_.pos.y +=
                verticalSpeed;
        }
        else if (heightDiff < -verticalSpeed)
        {
            transform_.pos.y -=
                verticalSpeed;
        }
        else
        {
            transform_.pos.y =
                playerPos.y;
        }

        animationController_->Play(
            (int)ANIM_TYPE::RUN
        );

        if (aiTimer_ >= 1.0f)
        {
            aiTimer_ = 0.0f;
            aiState_ = AI_STATE::MOVE;
        }

        break;
    }

    case AI_STATE::MOVE:
    {
        // 攻撃前に一度280付近へ間合いを整える
        VECTOR moveDir =
            VSub(
                normalBattlePos,
                transform_.pos
            );

        float moveDistance =
            VSize(moveDir);

        if (moveDistance > 0.001f)
        {
            moveDir =
                VNorm(moveDir);
        }

        if (moveDistance > 10.0f)
        {
            float moveAmount =
                moveDistance;

            if (moveAmount > moveSpeed_)
            {
                moveAmount =
                    moveSpeed_;
            }

            transform_.pos =
                VAdd(
                    transform_.pos,
                    VScale(
                        moveDir,
                        moveAmount
                    )
                );

            animationController_->Play(
                (int)ANIM_TYPE::RUN
            );
        }
        else
        {
            aiTimer_ += deltaTime;

            animationController_->Play(
                (int)ANIM_TYPE::IDLE
            );

            if (aiTimer_ >= 0.5f)
            {
                aiTimer_ = 0.0f;

                if (attackCoolTimer_ <= 0.0f)
                {
                    aiState_ =
                        AI_STATE::ATTACK;
                }
                else
                {
                    aiState_ =
                        AI_STATE::WAIT;
                }
            }
        }

        break;
    }

    case AI_STATE::BOOST_ATTACK:
    {
        // 高速接近はこの行動を選んだ時だけ
        if (!isBoostChase_)
        {
            isBoostChase_ = true;
            boostChaseTimer_ = 0.0f;

            animationController_->Play(
                (int)ANIM_TYPE::FAST_RUN
            );
        }

        boostChaseTimer_ += deltaTime;

        const float boostStopDistance = 110.0f;

        VECTOR boostTarget =
            VSub(
                playerPos,
                VScale(
                    horizontalDir,
                    boostStopDistance
                )
            );

        VECTOR boostDir =
            VSub(
                boostTarget,
                transform_.pos
            );

        float boostDistance =
            VSize(boostDir);

        if (boostDistance > 0.001f)
        {
            boostDir =
                VNorm(boostDir);
        }

        if (boostDistance <= 10.0f ||
            boostChaseTimer_ >= BOOST_CHASE_TIME)
        {
            isBoostChase_ = false;
            boostChaseTimer_ = 0.0f;

            aiState_ =
                AI_STATE::ATTACK;

            aiTimer_ = 0.0f;

            animationController_->Play(
                (int)ANIM_TYPE::IDLE
            );
        }
        else
        {
            float boostMove =
                boostDistance;

            if (boostMove >
                BOOST_CHASE_SPEED)
            {
                boostMove =
                    BOOST_CHASE_SPEED;
            }

            transform_.pos =
                VAdd(
                    transform_.pos,
                    VScale(
                        boostDir,
                        boostMove
                    )
                );

            animationController_->Play(
                (int)ANIM_TYPE::FAST_RUN
            );
        }

        break;
    }

    case AI_STATE::ATTACK:
    {
        // 攻撃すると決めてから90まで近づく
        const float attackDistance = 90.0f;

        VECTOR attackPos =
            VSub(
                playerPos,
                VScale(
                    horizontalDir,
                    attackDistance
                )
            );

        VECTOR attackMoveDir =
            VSub(
                attackPos,
                transform_.pos
            );

        float attackMoveDistance =
            VSize(attackMoveDir);

        if (attackMoveDistance > 0.001f)
        {
            attackMoveDir =
                VNorm(attackMoveDir);
        }

        if (attackMoveDistance > 10.0f)
        {
            const float attackApproachSpeed = 5.0f;

            float moveAmount =
                attackMoveDistance;

            if (moveAmount >
                attackApproachSpeed)
            {
                moveAmount =
                    attackApproachSpeed;
            }

            transform_.pos =
                VAdd(
                    transform_.pos,
                    VScale(
                        attackMoveDir,
                        moveAmount
                    )
                );

            animationController_->Play(
                (int)ANIM_TYPE::RUN
            );
        }
        else
        {
            // 90まで来てから今までの4段攻撃開始
            if (attackCoolTimer_ <= 0.0f)
            {
                isAttack_ = true;
                attackCombo_ = 1;
                attackTimer_ = 0.0f;
                hasAttackHit_ = false;

                isAttackWait_ = false;
                attackWaitTimer_ = 0.0f;

                animationController_->Play(
                    (int)ANIM_TYPE::ATTACK01,
                    false
                );
            }
            else
            {
                aiState_ =
                    AI_STATE::WAIT;

                aiTimer_ = 0.0f;

                animationController_->Play(
                    (int)ANIM_TYPE::IDLE
                );
            }
        }

        break;
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

    // プレイヤー本体
    SetUseLighting(FALSE);

    MV1DrawModel(
        transform_.modelId
    );

    SetUseLighting(TRUE);

    DrawFormatString(
        20,
        180,
        GetColor(255, 255, 255),
        "Enemy HP : %d",
        hp_
    );

    DrawFormatString(
        20,
        200,
        GetColor(255, 255, 255),
        "Attack:%d Combo:%d Timer:%.2f Cool:%.2f",
        isAttack_,
        attackCombo_,
        attackTimer_,
        attackCoolTimer_
    );
}

void MeleeEnemy::SetPosition(VECTOR pos)
{
    transform_.pos = pos;

    transform_.Update();
}

void MeleeEnemy::LookAtPlayer(void)
{
    VECTOR dir =
        VSub(
            player_.GetTransform().pos,
            transform_.pos
        );

    dir.y = 0.0f;

    if (VSize(dir) <= 0.001f)
    {
        return;
    }

    dir = VNorm(dir);

    damageRot_ =
        Quaternion::LookRotation(dir);

    transform_.quaRot =
        damageRot_;
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

int MeleeEnemy::GetAttackCombo(void) const
{
    return attackCombo_;
}

void MeleeEnemy::DrawSyncRing(void)
{
    if (!IsSyncReady())
    {
        return;
    }

    VECTOR center =
        transform_.pos;

    // 足元より少し上
    center.y += 3.0f;

    // 1.0 → 0.0
    float rate =
        GetSyncRate();

    // 残り時間で円が縮む
    float radius =
        80.0f * rate;

    static constexpr int SEGMENT = 48;

    unsigned int color =
        GetColor(
            80,
            220,
            255
        );

    // 半透明
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


        // 中心から三角形を並べて円を作る
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

void MeleeEnemy::StartSlamDown(void)
{
    isSlamDown_ = true;
    isDown_ = false;
    downTimer_ = 0.0f;

    isAttack_ = false;
    attackTimer_ = 0.0f;

    isDamage_ = false;
    damageTimer_ = 0.0f;
}

void MeleeEnemy::GuardBurst(void)
{
    // 攻撃を中断
    isAttack_ = false;
    attackCombo_ = 0;
    attackTimer_ = 0.0f;
    hasAttackHit_ = false;

    // 高速接近も中断
    isBoostChase_ = false;
    boostChaseTimer_ = 0.0f;

    // すぐに再攻撃しないようにする
    attackCoolTimer_ =
        ATTACK_COOL_TIME;
}

void MeleeEnemy::GuardDamage(float damage)
{
    if (!isGuard_ ||
        isGuardBreak_)
    {
        return;
    }

    guardHp_ -= damage;

    if (guardHp_ <= 0.0f)
    {
        guardHp_ = 0.0f;

        isGuard_ = false;
        guardTimer_ = 0.0f;

        isGuardBreak_ = true;
        guardBreakTimer_ = 2.0f;

        isAttack_ = false;
        attackCombo_ = 0;
        attackTimer_ = 0.0f;
        hasAttackHit_ = false;

        isBoostChase_ = false;
        boostChaseTimer_ = 0.0f;

        animationController_->Play(
            (int)ANIM_TYPE::GUARD_BREAK,
            false
        );
    }
}

bool MeleeEnemy::IsDown(void) const
{
    return isDown_;
}

bool MeleeEnemy::IsGuard(void) const
{
    return isGuard_;
}

void MeleeEnemy::SetKamehameHit(
    bool hit,
    VECTOR dir
)
{
    if (!hit)
    {
        isKamehameHit_ = false;

        kamehamePushDir_ =
            AsoUtility::VECTOR_ZERO;

        kamehamePushSpeed_ = 0.0f;

        return;
    }

    if (!isKamehameHit_)
    {
        kamehamePushSpeed_ = 3.0f;

        animationController_->Play(
            (int)ANIM_TYPE::DAMAGE,
            true
        );
    }

    isKamehameHit_ = true;

    if (VSize(dir) > 0.001f)
    {
        kamehamePushDir_ =
            VNorm(dir);
    }

    isAttack_ = false;
    attackCombo_ = 0;
    attackTimer_ = 0.0f;
    hasAttackHit_ = false;

    isBoostChase_ = false;
    boostChaseTimer_ = 0.0f;
}