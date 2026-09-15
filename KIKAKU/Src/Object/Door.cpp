#include <cmath>
#include "Door.h"
#include "FloorSwitch.h"
#include "../Utility/AsoUtility.h"
#include "../Manager/ResourceManager.h"

Door::Door(
    FloorSwitch& floorSwitch,
    OPEN_TYPE openType)
    :
    floorSwitch1_(floorSwitch),
    floorSwitch2_(nullptr),
    openType_(openType),
    isUnlocked_(false),
    moveSpeed_(3.0f)
{
    closePos_ = AsoUtility::VECTOR_ZERO;
    openPos_ = AsoUtility::VECTOR_ZERO;
}

Door::Door(
    FloorSwitch& floorSwitch1,
    FloorSwitch& floorSwitch2,
    OPEN_TYPE openType)
    :
    floorSwitch1_(floorSwitch1),
    floorSwitch2_(&floorSwitch2),
    openType_(openType),
    isUnlocked_(false),
    moveSpeed_(3.0f)
{
    closePos_ = AsoUtility::VECTOR_ZERO;
    openPos_ = AsoUtility::VECTOR_ZERO;
}

Door::~Door(void)
{
}

void Door::Init(void)
{
    transform_.SetModel(
        resMng_.LoadModelDuplicate(
            ResourceManager::SRC::DOOR
        )
    );

    // 仮の扉位置
    closePos_ = { 300.0f, -30.0f, 0.0f };

    // 開いた時は上に移動
    openPos_ = closePos_;
    openPos_.y += 200.0f;

    transform_.pos = closePos_;
    transform_.scl = AsoUtility::VECTOR_ONE;
    transform_.quaRot = Quaternion();

    transform_.MakeCollider(
        Collider::TYPE::STAGE
    );

    transform_.Update();
}

void Door::Update(void)
{
    // スイッチ条件
    bool switchCondition = false;

    // スイッチ1個
    if (floorSwitch2_ == nullptr)
    {
        switchCondition =
            floorSwitch1_.IsPressed();
    }

    // スイッチ2個
    else
    {
        switchCondition =
            floorSwitch1_.IsPressed() &&
            floorSwitch2_->IsPressed();
    }

    // Doorを開けるか
    bool isOpen = false;

    if (openType_ == OPEN_TYPE::HOLD)
    {
        // 条件成立中だけ開く
        isOpen = switchCondition;
    }
    else if (openType_ == OPEN_TYPE::UNLOCK)
    {
        // 一度でも条件成立したら解除
        if (switchCondition)
        {
            isUnlocked_ = true;
        }

        isOpen = isUnlocked_;
    }

    // Door移動
    float targetY;

    if (isOpen)
    {
        targetY = openPos_.y;
    }
    else
    {
        targetY = closePos_.y;
    }

    float diff =
        targetY - transform_.pos.y;

    transform_.pos.y +=
        diff * 0.08f;

    if (fabsf(diff) < 0.5f)
    {
        transform_.pos.y = targetY;
    }

    transform_.Update();
}

void Door::Draw(void)
{
    // ドアモデル描画
    MV1DrawModel(transform_.modelId);

    VECTOR minPos =
    {
        transform_.pos.x - 50.0f,
        transform_.pos.y,
        transform_.pos.z - 15.0f
    };

    VECTOR maxPos =
    {
        transform_.pos.x + 50.0f,
        transform_.pos.y + 150.0f,
        transform_.pos.z + 15.0f
    };

    DrawCube3D(
        minPos,
        maxPos,
        GetColor(100, 150, 255),
        GetColor(255, 255, 255),
        true
    );

    DrawFormatString(
        20,
        160,
        GetColor(255, 255, 255),
        "DOOR MODEL : %d",
        transform_.modelId
    );
}

std::weak_ptr<Collider> Door::GetCollider(void) const
{
    return transform_.collider;
}

void Door::SetPosition(VECTOR pos)
{
    closePos_ = pos;

    openPos_ = closePos_;
    openPos_.y += 200.0f;

    transform_.pos = closePos_;

    transform_.Update();
}