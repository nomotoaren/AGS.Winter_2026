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
    hasAttackHit_(false)
{
    hp_ = 100;
    hitRadius_ = 50.0f;

    isDamage_ = false;
    damageTimer_ = 0.0f;

    isSlamDown_ = false;
    isDown_ = false;
    downTimer_ = 0.0f;
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

        // 高さ制限
        const float MIN_HEIGHT = 100.0f;
        const float MAX_HEIGHT = 2000.0f;

        if (transform_.pos.y <= MIN_HEIGHT)
        {
            transform_.pos.y = MIN_HEIGHT;

            // 8段目で叩き落とされていたらダウン
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
            damageTimer_ = 0.5f;

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
        damageTimer_ -= deltaTime;

        if (damageTimer_ <= 0.0f)
        {
            damageTimer_ = 0.0f;
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

    // 高速接近中
    if (isBoostChase_)
    {
        boostChaseTimer_ += deltaTime;

        VECTOR dir =
            VSub(
                player_.GetTransform().pos,
                transform_.pos
            );

        float chaseDistance =
            VSize(dir);

        if (chaseDistance > 0.001f)
        {
            dir = VNorm(dir);

            transform_.quaRot =
                Quaternion::LookRotation(dir);
        }

        // プレイヤーの近くまで来たら終了
        if (chaseDistance <= stopRange_ ||
            boostChaseTimer_ >= BOOST_CHASE_TIME)
        {
            isBoostChase_ = false;
            boostChaseTimer_ = 0.0f;

            animationController_->Play(
                (int)ANIM_TYPE::IDLE
            );
        }
        else
        {
            transform_.pos =
                VAdd(
                    transform_.pos,
                    VScale(
                        dir,
                        BOOST_CHASE_SPEED
                    )
                );

            animationController_->Play(
                (int)ANIM_TYPE::FAST_RUN
            );

            transform_.Update();

            return;
        }
    }

    // 攻撃中
    if (isAttack_)
    {
        attackTimer_ += deltaTime;

        // 1～3段目は攻撃中もプレイヤーについていく
        if (attackCombo_ >= 1 &&
            attackCombo_ <= 3)
        {
            VECTOR dir =
                VSub(
                    player_.GetTransform().pos,
                    transform_.pos
                );

            float distance =
                VSize(dir);

            if (distance > 0.001f)
            {
                dir =
                    VNorm(dir);

                float stopDistance = 65.0f;
                float chaseSpeed = 10.0f;

                if (distance > stopDistance)
                {
                    float moveDistance =
                        distance - stopDistance;

                    if (moveDistance > chaseSpeed)
                    {
                        moveDistance =
                            chaseSpeed;
                    }

                    transform_.pos =
                        VAdd(
                            transform_.pos,
                            VScale(
                                dir,
                                moveDistance
                            )
                        );
                }

                transform_.quaRot =
                    Quaternion::LookRotation(dir);
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

                animationController_->Play(
                    (int)ANIM_TYPE::IDLE
                );
                break;
            }
        }

        transform_.Update();

        return;
    }

    VECTOR dir =
        AsoUtility::VECTOR_ZERO;

    if (distance > 0.001f)
    {
        dir =
            VNorm(toTarget);
    }

    // Playerの方向を向く
    if (!AsoUtility::EqualsVZero(dir))
    {
        transform_.quaRot =
            Quaternion::LookRotation(dir);
    }

    // Playerへ近づく
    if (distance > stopRange_)
    {
        // 遠ければ高速接近
        if (distance >= BOOST_CHASE_DISTANCE)
        {
            isBoostChase_ = true;
            boostChaseTimer_ = 0.0f;

            animationController_->Play(
                (int)ANIM_TYPE::FAST_RUN
            );
        }
        else
        {
            transform_.pos =
                VAdd(
                    transform_.pos,
                    VScale(
                        dir,
                        moveSpeed_
                    )
                );

            animationController_->Play(
                (int)ANIM_TYPE::RUN
            );
        }
    }
    else
    {
        if (attackCoolTimer_ <= 0.0f)
        {
            isAttack_ = true;
            attackCombo_ = 1;
            attackTimer_ = 0.0f;
            hasAttackHit_ = false;

            animationController_->Play(
                (int)ANIM_TYPE::ATTACK01,
                false
            );
        }
        else
        {
            animationController_->Play(
                (int)ANIM_TYPE::IDLE
            );
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

bool MeleeEnemy::IsDown(void) const
{
    return isDown_;
}