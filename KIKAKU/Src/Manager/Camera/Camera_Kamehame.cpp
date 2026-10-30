#include <DxLib.h>
#include "../../Utility/AsoUtility.h"
#include "../../Object/Common/Transform.h"
#include "Camera.h"

void Camera::SetBeforeDrawKamehame(void)
{
    if (followTransform_ == nullptr)
    {
        return;
    }

    VECTOR playerPos =
        followTransform_->pos;

    // プレイヤーの向き
    VECTOR forward =
        followTransform_->quaRot.GetForward();

    forward.y = 0.0f;

    if (VSize(forward) <= 0.001f)
    {
        forward = { 0.0f, 0.0f, 1.0f };
    }
    else
    {
        forward = VNorm(forward);
    }

    // 右方向
    VECTOR right =
    {
        forward.z,
        0.0f,
        -forward.x
    };

    // カメラ位置
    pos_ = playerPos;

    // 少し前
    pos_ =
        VAdd(
            pos_,
            VScale(forward, 120.0f)
        );

    // プレイヤーの右側
    pos_ =
        VAdd(
            pos_,
            VScale(right, 150.0f)
        );

    // 高さ
    pos_.y += 100.0f;

    // 注視点
    targetPos_ = playerPos;

    // 胸のあたりを見る
    targetPos_.y += 80.0f;

    // 少し前を見る
    targetPos_ =
        VAdd(
            targetPos_,
            VScale(forward, 40.0f)
        );

    cameraUp_ =
        AsoUtility::DIR_U;
}

void Camera::SetBeforeDrawKamehameShot(void)
{
    if (followTransform_ == nullptr)
    {
        return;
    }

    VECTOR playerPos =
        followTransform_->pos;

    VECTOR forward =
        followTransform_->quaRot.GetForward();

    forward.y = 0.0f;

    if (VSize(forward) <= 0.001f)
    {
        forward =
        {
            0.0f,
            0.0f,
            1.0f
        };
    }
    else
    {
        forward =
            VNorm(forward);
    }

    // カメラ位置
    // プレイヤーの後ろ
    pos_ =
        VSub(
            playerPos,
            VScale(
                forward,
                350.0f
            )
        );

    // 少し上に
    pos_.y += 120.0f;

    // 少し横にずらして
    // プレイヤーとビームが両方見えるようにする
    VECTOR right =
    {
        forward.z,
        0.0f,
        -forward.x
    };

    pos_ =
        VAdd(
            pos_,
            VScale(
                right,
                80.0f
            )
        );

    // 注視点
    // プレイヤーの前方遠く
    targetPos_ =
        VAdd(
            playerPos,
            VScale(
                forward,
                300.0f
            )
        );

    targetPos_.y += 70.0f;

    cameraUp_ =
        AsoUtility::DIR_U;
}
