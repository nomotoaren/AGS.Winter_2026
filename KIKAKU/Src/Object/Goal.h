#pragma once

#include "ActorBase.h"

class Player;

class Goal : public ActorBase
{
public:

    Goal(Player& player);
    ~Goal(void);

    void Init(void) override;
    void Update(void) override;
    void Draw(void) override;

    bool IsClear(void) const;

    void Reset(void);

    void SetPosition(VECTOR pos);

private:

    Player& player_;

    bool isClear_;

    float radius_;
};