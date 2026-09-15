#include <cmath>
#include "Goal.h"
#include "Player.h"
#include "../Utility/AsoUtility.h"

Goal::Goal(Player& player)
    :
    player_(player),
    isClear_(false),
    radius_(70.0f)
{
}

Goal::~Goal(void)
{
}

void Goal::Init(void)
{
    // Door‚ÌŒü‚±‚¤‘¤‚É‰¼”z’u
    transform_.pos = { 500.0f, -28.0f, 0.0f };

    transform_.scl = AsoUtility::VECTOR_ONE;

    transform_.Update();
}

void Goal::Update(void)
{
    if (isClear_)
    {
        return;
    }

    VECTOR playerPos =
        player_.GetTransform().pos;

    float diffX =
        playerPos.x - transform_.pos.x;

    float diffZ =
        playerPos.z - transform_.pos.z;

    float distance =
        sqrtf(
            diffX * diffX +
            diffZ * diffZ
        );

    if (distance < radius_)
    {
        isClear_ = true;
    }
}

void Goal::Draw(void)
{
    VECTOR drawPos = transform_.pos;

    drawPos.y += 30.0f;

    DrawSphere3D(
        drawPos,
        35.0f,
        16,
        GetColor(255, 220, 50),
        GetColor(255, 255, 255),
        TRUE
    );
}

bool Goal::IsClear(void) const
{
    return isClear_;
}

void Goal::Reset(void)
{
    isClear_ = false;
}

void Goal::SetPosition(VECTOR pos)
{
    transform_.pos = pos;
    transform_.Update();
}