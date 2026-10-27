#include <math.h>
#include <DxLib.h>
#include <EffekseerForDXLib.h>
#include "../Utility/AsoUtility.h"
#include "../Manager/InputManager.h"
#include "../Manager/AppFlow.h"
#include "../Manager/SceneManager.h"
#include "../Object/Common/Transform.h"
#include "Camera.h"

namespace
{
    // 止まっている間は 0 秒として扱う(カメラの時間が進まなくなる)
    float CameraDeltaTime(void)
    {
        return AppFlow::IsEffectFrozen()
            ? 0.0f
            : SceneManager::GetInstance().GetDeltaTime();
    }
}

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
        shakeTimer_ -=
            CameraDeltaTime();

        if (shakeTimer_ <= 0.0f)
        {
            shakeTimer_ = 0.0f;
            shakePower_ = 0.0f;
            isShake_ = false;
        }
        else
        {
            float shakeX =
                static_cast<float>(
                    GetRand(200) - 100
                    ) / 100.0f;

            float shakeY =
                static_cast<float>(
                    GetRand(200) - 100
                    ) / 100.0f;

            drawPos.x +=
                shakeX * shakePower_;

            drawPos.y +=
                shakeY * shakePower_;
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
    isShake_ = true;
    shakeTimer_ = time;
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

    // カメラ回転
    if (ins.IsNew(KEY_INPUT_RIGHT))
    {
        // 右回転
        angles_.y += AsoUtility::Deg2RadF(1.0f);
    }
    if (ins.IsNew(KEY_INPUT_LEFT))
    {
        // 左回転
        angles_.y += AsoUtility::Deg2RadF(-1.0f);
    }

    // 上回転
    if (ins.IsNew(KEY_INPUT_UP))
    {
        angles_.x += AsoUtility::Deg2RadF(1.0f);
        if (angles_.x > LIMIT_X_UP_RAD)
        {
            angles_.x = LIMIT_X_UP_RAD;
        }
    }

    // 下回転
    if (ins.IsNew(KEY_INPUT_DOWN))
    {
        angles_.x += AsoUtility::Deg2RadF(-1.0f);
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

    // カメラ位置を滑らかに追従
    pos_ =
        VAdd(
            pos_,
            VScale(
                VSub(
                    followTargetPos_,
                    pos_
                ),
                FOLLOW_SMOOTH
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
                FOLLOW_SMOOTH
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

// 演出カメラの開始
void Camera::StartCinematic(const std::vector<CineKey>& keys)
{
    if (keys.empty())
    {
        return;
    }

    if (!cineActive_)
    {
        cinePrevMode_ = mode_;
    }

    cineActive_ = true;
    cineKeys_ = keys;
    cineTime_ = 0.0f;

    // 今のカメラから滑らかにつなぐ
    cineStartPos_ = pos_;
    cineStartTarget_ = targetPos_;

    mode_ = MODE::CINEMATIC;
}

void Camera::UpdateCinematicPivot(const VECTOR& pos, const VECTOR& forward)
{
    cinePivotPos_ = pos;

    VECTOR f = forward;
    f.y = 0.0f;

    if (VSize(f) > 0.001f)
    {
        cinePivotFwd_ = VNorm(f);
    }
}

// 演出カメラの終了(通常のカメラの追従が、今の位置から滑らかに戻していく)
void Camera::EndCinematic(void)
{
    if (!cineActive_)
    {
        return;
    }

    cineActive_ = false;
    mode_ = cinePrevMode_;

    if (mode_ == MODE::NONE || mode_ == MODE::CINEMATIC)
    {
        mode_ = MODE::FOLLOW;
    }
}

void Camera::SetBeforeDrawCinematic(void)
{
    if (!cineActive_ || cineKeys_.empty())
    {
        return;
    }

    cineTime_ += CameraDeltaTime();

    // 時刻が属する区間を探す
    const int last = static_cast<int>(cineKeys_.size()) - 1;
    int i = 0;

    while (i < last && cineKeys_[i + 1].time <= cineTime_)
    {
        i++;
    }

    VECTOR localPos = cineKeys_[i].pos;
    VECTOR localLook = cineKeys_[i].look;
    float fov = cineKeys_[i].fovDeg;

    // 次のキーへ補間する(次がカットなら、その時刻まで今のまま)
    if (i < last && !cineKeys_[i + 1].cut)
    {
        const CineKey& a = cineKeys_[i];
        const CineKey& b = cineKeys_[i + 1];

        float u = (cineTime_ - a.time) / (b.time - a.time);
        u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
        u = u * u * (3.0f - 2.0f * u);	// ゆっくり始まり、ゆっくり止まる

        localPos = VAdd(a.pos, VScale(VSub(b.pos, a.pos), u));
        localLook = VAdd(a.look, VScale(VSub(b.look, a.look), u));
        fov = a.fovDeg + (b.fovDeg - a.fovDeg) * u;
    }

    // 基準点からの相対座標 → ワールド座標(右 = {z, 0, -x})
    const VECTOR fwd = cinePivotFwd_;
    const VECTOR right = { fwd.z, 0.0f, -fwd.x };

    auto toWorld = [&](const VECTOR& l) -> VECTOR
        {
            VECTOR w = cinePivotPos_;
            w = VAdd(w, VScale(right, l.x));
            w = VAdd(w, VGet(0.0f, l.y, 0.0f));
            w = VAdd(w, VScale(fwd, l.z));
            return w;
        };

    const VECTOR goalPos = toWorld(localPos);
    const VECTOR goalLook = toWorld(localLook);

    // 始まりは、今のカメラから滑らかにつなぐ(0.25秒)
    float w = cineTime_ / 0.25f;
    w = w > 1.0f ? 1.0f : w;
    w = w * w * (3.0f - 2.0f * w);

    pos_ = VAdd(cineStartPos_, VScale(VSub(goalPos, cineStartPos_), w));
    targetPos_ = VAdd(cineStartTarget_, VScale(VSub(goalLook, cineStartTarget_), w));

    fovDeg_ = DEFAULT_FOV_DEG + (fov - DEFAULT_FOV_DEG) * w;

    cameraUp_ = AsoUtility::DIR_U;
}

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