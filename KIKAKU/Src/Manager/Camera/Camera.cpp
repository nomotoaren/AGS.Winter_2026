#include <math.h>
#include <DxLib.h>
#include <EffekseerForDXLib.h>
#include "../../Utility/AsoUtility.h"
#include "../../Manager/InputManager.h"
#include "../../Manager/AppFlow.h"
#include "../../Manager/SceneManager.h"
#include "../../Object/Common/Transform.h"
#include "Camera.h"

#include "Camera_Internal.h"

using namespace CameraInternal;

Camera::Camera(void)
{
    angles_ = VECTOR();
    cameraUp_ = VECTOR();
    mode_ = MODE::NONE;
    pos_ = AsoUtility::VECTOR_ZERO;
    targetPos_ = AsoUtility::VECTOR_ZERO;
    followTransform_ = nullptr;
    lockOnTransform_ = nullptr;

    followTargetPos_ =
    {
        0.0f,
        0.0f,
        0.0f
    };

    followTargetLook_ =
    {
        0.0f,
        0.0f,
        0.0f
    };

    isShake_ = false;
    shakeTimer_ = 0.0f;
    shakePower_ = 0.0f;

    lockOnCameraDir_ =
    {
        0.0f,
        0.0f,
        -1.0f
    };
}

Camera::~Camera(void)
{
}

void Camera::Init(void)
{
    ChangeMode(MODE::FIXED_POINT);
}

void Camera::Update(void)
{
}

void Camera::SetBeforeDraw(void)
{
    // クリップ距離を設定する
    SetCameraNearFar(
        CAMERA_NEAR,
        CAMERA_FAR
    );

    switch (mode_)
    {
    case Camera::MODE::FIXED_POINT:
        SetBeforeDrawFixedPoint();
        break;

    case Camera::MODE::FOLLOW:
        SetBeforeDrawFollow();
        break;

    case Camera::MODE::LOCK_ON:
        SetBeforeDrawLockOn();
        break;

    case Camera::MODE::KAMEHAME:
        SetBeforeDrawKamehame();
        break;

    case Camera::MODE::KAMEHAME_SHOT:
        SetBeforeDrawKamehameShot();
        break;

    case Camera::MODE::CINEMATIC:
        SetBeforeDrawCinematic();
        break;

    }

    // 画角: 演出が終わったら通常へ滑らかに戻す
    if (!cineActive_)
    {
        const float rate =
            1.0f - expf(-6.0f * CameraDeltaTime());

        fovDeg_ += (DEFAULT_FOV_DEG - fovDeg_) * rate;
    }

    SetupCamera_Perspective(AsoUtility::Deg2RadF(fovDeg_));

    VECTOR drawPos =
        pos_;

    // カメラ揺れ
    if (isShake_ && !AppFlow::IsEffectFrozen())
    {
        shakeTimer_ -= CameraDeltaTime();

        if (shakeTimer_ <= 0.0f)
        {
            shakeTimer_ = 0.0f;
            shakePower_ = 0.0f;
            shakeDuration_ = 0.0f;
            isShake_ = false;
        }
        else
        {
            // 終わりに向かって弱くする
            const float fade = shakeTimer_ / shakeDuration_;
            const float p = shakePower_ * fade;

            const float shakeX = static_cast<float>(GetRand(200) - 100) / 100.0f;
            const float shakeY = static_cast<float>(GetRand(200) - 100) / 100.0f;

            // 画面から見て、横と縦に揺らす
            const VECTOR forward = VNorm(VSub(targetPos_, pos_));
            const VECTOR right = VNorm(VCross(cameraUp_, forward));
            const VECTOR up = VCross(forward, right);

            drawPos = VAdd(drawPos, VScale(right, shakeX * p));
            drawPos = VAdd(drawPos, VScale(up, shakeY * p));
        }
    }

    // カメラの設定
    SetCameraPositionAndTargetAndUpVec(
        drawPos,
        targetPos_,
        cameraUp_
    );

    Effekseer_Sync3DSetting();
}

void Camera::Draw(void)
{
}

void Camera::SetFollow(
    const Transform* follow
)
{
    followTransform_ = follow;
}

void Camera::SetLockOnTarget(
    const Transform* target
)
{
    lockOnTransform_ = target;
}

void Camera::StartShake(
    float time,
    float power
)
{
    // 今の揺れの残りの強さ
    float current = 0.0f;
    if (isShake_ && shakeDuration_ > 0.0f)
    {
        current = shakePower_ * (shakeTimer_ / shakeDuration_);
    }

    // 弱い揺れは、強い揺れを打ち消さない
    if (power < current)
    {
        return;
    }

    isShake_ = true;
    shakeTimer_ = time;
    shakeDuration_ = time;
    shakePower_ = power;
}

VECTOR Camera::GetPos(void) const
{
    return pos_;
}

VECTOR Camera::GetAngles(void) const
{
    return angles_;
}

VECTOR Camera::GetTargetPos(void) const
{
    return targetPos_;
}

Quaternion Camera::GetQuaRot(void) const
{
    return rot_;
}

Quaternion Camera::GetQuaRotOutX(void) const
{
    return rotOutX_;
}

VECTOR Camera::GetForward(void) const
{
    return VNorm(
        VSub(
            targetPos_,
            pos_
        )
    );
}

void Camera::ChangeMode(MODE mode)
{

    // 演出中は切り替えず、演出が終わったときの戻り先だけ覚えておく
    if (cineActive_ && mode != MODE::CINEMATIC)
    {
        cinePrevMode_ = mode;
        return;
    }

    // カメラの初期設定
    if (mode_ == MODE::NONE)
    {
        SetDefault();
    }

    // カメラモードの変更
    mode_ = mode;

    // 変更時の初期化処理
    switch (mode_)
    {
    case Camera::MODE::FIXED_POINT:
        break;
    case Camera::MODE::FOLLOW:
        break;
    case Camera::MODE::LOCK_ON:
        break;
    case Camera::MODE::KAMEHAME:
        break;
    case Camera::MODE::KAMEHAME_SHOT:
        break;
    }

}

void Camera::SetDefault(void)
{

    // カメラの初期位置
    pos_ = DEFAULT_CAMERA_POS;

    // 注視点
    targetPos_ = AsoUtility::VECTOR_ZERO;

    // カメラの上方向
    cameraUp_ = AsoUtility::DIR_U;

    angles_.x = AsoUtility::Deg2RadF(30.0f);
    angles_.y = 0.0f;
    angles_.z = 0.0f;

    rot_ = Quaternion();

}

void Camera::SyncFollow(void)
{

    // 追従対象の位置
    VECTOR pos = followTransform_->pos;

    // 重力の方向に従い
    // 正面から設定されたY軸を回転させる
    rotOutX_ = Quaternion::AngleAxis(angles_.y, AsoUtility::AXIS_Y);

    // 正面から設定されたX軸を回転させる
    rot_ = rotOutX_.Mult(Quaternion::AngleAxis(angles_.x, AsoUtility::AXIS_X));

    VECTOR localPos;

    // 注視点(通常重力であれば追従対象と同じY値にする)
    localPos = rotOutX_.PosAxis(LOCAL_F2T_POS);
    followTargetLook_ = VAdd(pos, localPos);

    // カメラ位置
    localPos = rot_.PosAxis(LOCAL_F2C_POS);

    followTargetPos_ = VAdd(pos, localPos);

    // カメラの上方向
    cameraUp_ = AsoUtility::DIR_U;

}

void Camera::ProcessRot(void)
{

    auto& ins = InputManager::GetInstance();

    float movePow = 5.0f;

    // 回す角度(度): 1フレームあたり1度 × フレームの長さ
    const float step = 1.0f * CameraFrameScale();

    // カメラ回転
    if (ins.IsNew(KEY_INPUT_RIGHT))
    {
        // 右回転
        angles_.y += AsoUtility::Deg2RadF(step);
    }
    if (ins.IsNew(KEY_INPUT_LEFT))
    {
        // 左回転
        angles_.y += AsoUtility::Deg2RadF(-step);
    }

    // 上回転
    if (ins.IsNew(KEY_INPUT_UP))
    {
        angles_.x += AsoUtility::Deg2RadF(step);
        if (angles_.x > LIMIT_X_UP_RAD)
        {
            angles_.x = LIMIT_X_UP_RAD;
        }
    }

    // 下回転
    if (ins.IsNew(KEY_INPUT_DOWN))
    {
        angles_.x += AsoUtility::Deg2RadF(-step);
        if (angles_.x < -LIMIT_X_DW_RAD)
        {
            angles_.x = -LIMIT_X_DW_RAD;
        }
    }

}

void Camera::SetBeforeDrawFixedPoint(void)
{
    // 何もしない
}

void Camera::SetBeforeDrawFollow(void)
{
    if (followTransform_ == nullptr)
    {
        return;
    }

    ProcessRot();

    // 通常FOLLOWの目標位置を計算
    SyncFollow();

    // 追いつく割合(フレームの長さに合わせて、fps が変わっても同じ速さにする)
    const float smooth = 1.0f - powf(1.0f - FOLLOW_SMOOTH, CameraFrameScale());

    // カメラ位置を滑らかに追従
    pos_ =
        VAdd(
            pos_,
            VScale(
                VSub(
                    followTargetPos_,
                    pos_
                ),
                smooth
            )
        );

    // 注視点を滑らかにする
    targetPos_ =
        VAdd(
            targetPos_,
            VScale(
                VSub(
                    followTargetLook_,
                    targetPos_
                ),
                smooth
            )
        );

    cameraUp_ =
        AsoUtility::DIR_U;
}

void Camera::SetBeforeDrawLockOn(void)
{
    if (followTransform_ == nullptr ||
        lockOnTransform_ == nullptr)
    {
        return;
    }

    const float deltaTime = CameraDeltaTime();

    // 調整用パラメータ
    const float BASE_DISTANCE = 260.0f;   // 基本の後ろ距離
    const float DISTANCE_RATE = 0.25f;    // 敵が遠いほど引く割合
    const float MAX_DISTANCE = 650.0f;
    const float BASE_HEIGHT = 110.0f;     // カメラの基本の高さ
    const float HEIGHT_RATE = 0.25f;      // 敵との高低差でカメラを上下させる割合
    const float SHOULDER_OFFSET = 60.0f;  // 肩越しのずらし(右が+)
    const float LOOK_RATE = 0.55f;        // 注視点を敵へ寄せる割合
    const float LOOK_HEIGHT = 70.0f;
    const float DIR_FOLLOW_SPEED = 6.0f;  // 向きの追従速度
    const float POS_FOLLOW_SPEED = 9.0f;  // 位置の追従速度
    const float LOOK_FOLLOW_SPEED = 14.0f;// 注視点の追従速度
    const float AIM_DISTANCE = 220.0f;    // 狙い中の後ろ距離(近づく)
    const float AIM_BLEND_SPEED = 8.0f;   // 狙いカメラへの切り替えの速さ

    VECTOR playerPos = followTransform_->pos;
    VECTOR enemyPos = lockOnTransform_->pos;

    VECTOR toEnemy = VSub(enemyPos, playerPos);

    VECTOR horizontal = toEnemy;
    horizontal.y = 0.0f;

    const float horizontalDistance = VSize(horizontal);
    const float heightDiff = toEnemy.y;

    // 敵への水平方向(真上・真下では前回の向きを維持)
    VECTOR horizontalDir;

    if (horizontalDistance > 0.001f)
    {
        horizontalDir = VNorm(horizontal);
    }
    else
    {
        horizontalDir = VScale(lockOnCameraDir_, -1.0f);
        horizontalDir.y = 0.0f;

        if (VSize(horizontalDir) <= 0.001f)
        {
            horizontalDir = VGet(0.0f, 0.0f, 1.0f);
        }

        horizontalDir = VNorm(horizontalDir);
    }

    VECTOR right = { horizontalDir.z, 0.0f, -horizontalDir.x };

    // 目標のカメラ方向(敵をカメラから見て真後ろ)
    VECTOR goalCameraDir = VScale(horizontalDir, -1.0f);

    // 向きの追従(フレームレート非依存)
    const float dirRate = 1.0f - expf(-DIR_FOLLOW_SPEED * deltaTime);

    lockOnCameraDir_ =
        VAdd(lockOnCameraDir_,
            VScale(VSub(goalCameraDir, lockOnCameraDir_), dirRate));

    lockOnCameraDir_.y = 0.0f;

    if (VSize(lockOnCameraDir_) > 0.001f)
    {
        lockOnCameraDir_ = VNorm(lockOnCameraDir_);
    }

    VECTOR camRight = { -lockOnCameraDir_.z, 0.0f, lockOnCameraDir_.x };

    // カメラ距離(敵が遠い・高低差が大きいほど引く)
    float cameraDistance =
        BASE_DISTANCE +
        horizontalDistance * DISTANCE_RATE +
        fabsf(heightDiff) * 0.15f;

    if (cameraDistance > MAX_DISTANCE)
    {
        cameraDistance = MAX_DISTANCE;
    }

    // 狙い中は、通常のカメラから少しずつ寄せる
    const float aimRate = 1.0f - expf(-AIM_BLEND_SPEED * deltaTime);
    aimBlend_ += ((isAimMode_ ? 1.0f : 0.0f) - aimBlend_) * aimRate;

    if (aimBlend_ > 0.001f)
    {
        cameraDistance += (AIM_DISTANCE - cameraDistance) * aimBlend_;
    }

    // カメラ位置: プレイヤーの背後 + 肩越し
    VECTOR cameraPos =
        VAdd(playerPos, VScale(lockOnCameraDir_, cameraDistance));

    cameraPos = VAdd(cameraPos, VScale(right, SHOULDER_OFFSET));

    // 敵が上ならカメラを下げて見上げ、下なら上げて見下ろす
    float clampedDiff = heightDiff;

    if (clampedDiff > 400.0f) { clampedDiff = 400.0f; }
    if (clampedDiff < -400.0f) { clampedDiff = -400.0f; }

    cameraPos.y = playerPos.y + BASE_HEIGHT - clampedDiff * HEIGHT_RATE;

    // 注視点: 敵寄り(プレイヤーは下側、敵は上側に映る)
    VECTOR lookPos =
        VAdd(playerPos, VScale(toEnemy, LOOK_RATE));

    lookPos.y += LOOK_HEIGHT;

    // 狙い中は、枠の中心を見る(枠が画面の真ん中に来る)
    if (aimBlend_ > 0.001f)
    {
        lookPos = VAdd(lookPos, VScale(VSub(aimPoint_, lookPos), aimBlend_));
    }

    // 連続エネルギー弾の狙い: カメラの位置はそのままで、視線だけを振る
    if (aimYaw_ != 0.0f || aimPitch_ != 0.0f)
    {
        const VECTOR v = VSub(lookPos, cameraPos);
        const float len = VSize(v);

        if (len > 0.001f)
        {
            const float h = sqrtf(v.x * v.x + v.z * v.z);
            const float pitch = atan2f(v.y, h) + aimPitch_;
            const float yaw = atan2f(v.x, v.z) + aimYaw_;

            lookPos = VAdd(cameraPos, VGet(
                len * cosf(pitch) * sinf(yaw),
                len * sinf(pitch),
                len * cosf(pitch) * cosf(yaw)));
        }
    }

    // 位置・注視点の追従
    const float posRate = 1.0f - expf(-POS_FOLLOW_SPEED * deltaTime);
    const float lookRate = 1.0f - expf(-LOOK_FOLLOW_SPEED * deltaTime);

    pos_ = VAdd(pos_, VScale(VSub(cameraPos, pos_), posRate));
    targetPos_ = VAdd(targetPos_, VScale(VSub(lookPos, targetPos_), lookRate));

    cameraUp_ = AsoUtility::DIR_U;
}

void Camera::SetBeforeDrawSelfShot(void)
{
}
