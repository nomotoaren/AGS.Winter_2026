#pragma once

#include <DxLib.h>
#include "ActorBase.h"

class Player;
class GhostPlayer;

class FloorSwitch : public ActorBase
{
public:

    FloorSwitch(Player& player, GhostPlayer& ghostPlayer);
    ~FloorSwitch(void);

    void Init(void) override;
    void Update(void) override;
    void Draw(void) override;

    bool IsPressed(void) const;

    void SetPosition(VECTOR pos);
private:

    // 現在のプレイヤー
    Player& player_;

    // 過去のプレイヤー
    GhostPlayer& ghostPlayer_;

    VECTOR normalPos_;
    VECTOR pressedPos_;

    bool isPressed_;

    float radius_;
};