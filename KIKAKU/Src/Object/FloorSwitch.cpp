#include "FloorSwitch.h"
#include "Player.h"
#include "GhostPlayer.h"
#include "../Manager/ResourceManager.h"
#include "../Utility/AsoUtility.h"
#include <cmath>

FloorSwitch::FloorSwitch(
    Player& player,
    GhostPlayer& ghostPlayer)
    :
    player_(player),
    ghostPlayer_(ghostPlayer),
    isPressed_(false),
    radius_(60.0f)
{
}

FloorSwitch::~FloorSwitch(void)
{
}

void FloorSwitch::Init(void)
{
    transform_.pos = { 200.0f, -28.0f, 200.0f };
    transform_.scl = AsoUtility::VECTOR_ONE;

    normalPos_ = { 100.0f, -28.0f, 0.0f };

    pressedPos_ = normalPos_;
    // スイッチの沈む量
    pressedPos_.y -= 20.0f;

    transform_.pos = normalPos_;

    transform_.Update();
}

void FloorSwitch::Update(void)
{
    // -------------------------
    // Player判定
    // -------------------------
    VECTOR playerPos =
        player_.GetTransform().pos;

    VECTOR switchPos =
        transform_.pos;

    float playerDiffX =
        playerPos.x - switchPos.x;

    float playerDiffZ =
        playerPos.z - switchPos.z;

    float playerDistance =
        sqrtf(
            playerDiffX * playerDiffX +
            playerDiffZ * playerDiffZ
        );

    bool isPlayerPressed =
        playerDistance < radius_;

    // -------------------------
    // Ghost判定
    // -------------------------
    bool isGhostPressed = false;

    // Ghost再生中だけ判定
    if (ghostPlayer_.IsPlaying())
    {
        VECTOR ghostPos =
            ghostPlayer_.GetTransform().pos;

        float ghostDiffX =
            ghostPos.x - switchPos.x;

        float ghostDiffZ =
            ghostPos.z - switchPos.z;

        float ghostDistance =
            sqrtf(
                ghostDiffX * ghostDiffX +
                ghostDiffZ * ghostDiffZ
            );

        isGhostPressed =
            ghostDistance < radius_;
    }

    // -------------------------
    // どちらかが踏めばON
    // -------------------------
    isPressed_ =
        isPlayerPressed ||
        isGhostPressed;

    float targetY;

    if (isPressed_)
    {
        targetY = pressedPos_.y;
    }
    else
    {
        targetY = normalPos_.y;
    }

    float diffY = targetY - transform_.pos.y;

    // 沈む速度
    transform_.pos.y += diffY * 0.15f;

    if (fabsf(diffY) < 0.1f)
    {
        transform_.pos.y = targetY;
    }

    transform_.Update();
}
void FloorSwitch::Draw(void)
{
    unsigned int color;

    if (isPressed_)
    {
        // 押されている 緑
        color = GetColor(0, 255, 0);
    }
    else
    {
        // 押されていない 赤
        color = GetColor(255, 0, 0);
    }

    // 床に埋まらないように少し上へ
    VECTOR drawPos = transform_.pos;
    drawPos.y += 20.0f;

    // デバッグ用の球
    DrawSphere3D(
        drawPos,
        30.0f,
        16,
        color,
        GetColor(255, 255, 255),
        TRUE
    );

    // 状態確認
    if (isPressed_)
    {
        DrawString(
            20,
            120,
            "SWITCH : ON",
            GetColor(0, 255, 0)
        );
    }
    else
    {
        DrawString(
            20,
            120,
            "SWITCH : OFF",
            GetColor(255, 255, 255)
        );
    }
}

bool FloorSwitch::IsPressed(void) const
{
    return isPressed_;
}

void FloorSwitch::SetPosition(VECTOR pos)
{
    normalPos_ = pos;

    pressedPos_ = normalPos_;

    // 前に調整した沈む量
    pressedPos_.y -= 20.0f;

    transform_.pos = normalPos_;

    transform_.Update();
}