#pragma once

#include <memory>
#include "ActorBase.h"

class FloorSwitch;
class Collider;

class Door : public ActorBase
{
public:

    enum class OPEN_TYPE
    {
        HOLD,       // 踏んでいる間だけ
        UNLOCK      // 条件達成後ずっと開く
    };

    // スイッチ1個用
    Door(
        FloorSwitch& floorSwitch,
        OPEN_TYPE openType
    );

    // スイッチ2個用
    Door(
        FloorSwitch& floorSwitch1,
        FloorSwitch& floorSwitch2,
        OPEN_TYPE openType
    );

    ~Door(void);

    void Init(void) override;
    void Update(void) override;
    void Draw(void) override;

    void SetPosition(VECTOR pos);

    std::weak_ptr<Collider> GetCollider(void) const;
private:

    // 連動する床スイッチ
    FloorSwitch& floorSwitch1_;

    // 2個目
    FloorSwitch* floorSwitch2_;

    // 閉じている時の位置
    VECTOR closePos_;

    // 開いている時の位置
    VECTOR openPos_;

    // 扉の移動速度
    float moveSpeed_;

    OPEN_TYPE openType_;

    // 一度開いたか
    bool isUnlocked_;
};