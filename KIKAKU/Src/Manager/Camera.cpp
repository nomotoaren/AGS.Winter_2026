#include <math.h>
#include <DxLib.h>
#include <EffekseerForDXLib.h>
#include "../Utility/AsoUtility.h"
#include "../Manager/InputManager.h"
#include "../Manager/SceneManager.h"
#include "../Object/Common/Transform.h"
#include "Camera.h"

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

    }

    VECTOR drawPos =
        pos_;

    // カメラ揺れ
    if (isShake_)
    {
        shakeTimer_ -=
            SceneManager::GetInstance().GetDeltaTime();

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

	// カメラの初期設定
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

	// 同期先の位置
	VECTOR pos = followTransform_->pos;

	// 重力の方向制御に従う
	// 正面から設定されたY軸分、回転させる
	rotOutX_ = Quaternion::AngleAxis(angles_.y, AsoUtility::AXIS_Y);

	// 正面から設定されたX軸分、回転させる
	rot_ = rotOutX_.Mult(Quaternion::AngleAxis(angles_.x, AsoUtility::AXIS_X));

	VECTOR localPos;

	// 注視点(通常重力でいうところのY値を追従対象と同じにする)
    localPos = rotOutX_.PosAxis(LOCAL_F2T_POS );
    followTargetLook_ = VAdd(pos,localPos);

	// カメラ位置
    localPos = rot_.PosAxis(LOCAL_F2C_POS);

    followTargetPos_ = VAdd(pos,localPos);

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

    // 注視点も滑らかにする
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

    VECTOR playerPos =
        followTransform_->pos;

    VECTOR enemyPos =
        lockOnTransform_->pos;


    // プレイヤーから敵への方向
    VECTOR dir =
        VSub(
            enemyPos,
            playerPos
        );

    dir.y = 0.0f;

    float enemyDistance =
        VSize(dir);

    if (enemyDistance <= 0.001f)
    {
        return;
    }

    dir =
        VNorm(dir);


    // プレイヤーの後ろ
    VECTOR goalCameraDir =
        VScale(
            dir,
            -1.0f
        );


    // 現在のカメラ方向
    VECTOR cameraDir =
        VSub(
            pos_,
            playerPos
        );

    cameraDir.y = 0.0f;

    if (VSize(cameraDir) <= 0.001f)
    {
        cameraDir =
            goalCameraDir;
    }
    else
    {
        cameraDir =
            VNorm(cameraDir);

        cameraDir =
            VAdd(
                cameraDir,
                VScale(
                    VSub(
                        goalCameraDir,
                        cameraDir
                    ),
                    0.03f
                )
            );

        if (VSize(cameraDir) > 0.001f)
        {
            cameraDir =
                VNorm(cameraDir);
        }
    }


    // 敵との距離に合わせてカメラを引く
    float cameraDistance =
        400.0f +
        enemyDistance * 0.45f;

    // 引きすぎ防止
    if (cameraDistance > 850.0f)
    {
        cameraDistance = 850.0f;
    }


    // カメラ位置
    VECTOR cameraPos =
        VAdd(
            playerPos,
            VScale(
                cameraDir,
                cameraDistance
            )
        );


    // 遠くなるほど少し高くする
    float cameraHeight =
        150.0f +
        enemyDistance * 0.08f;

    if (cameraHeight > 260.0f)
    {
        cameraHeight = 260.0f;
    }

    cameraPos.y +=
        cameraHeight;


    // プレイヤーと敵の中間を見る
    VECTOR lookPos =
        VAdd(
            playerPos,
            VScale(
                VSub(
                    enemyPos,
                    playerPos
                ),
                0.5f
            )
        );

    lookPos.y += 70.0f;


    // カメラ位置を滑らかに移動
    pos_ =
        VAdd(
            pos_,
            VScale(
                VSub(
                    cameraPos,
                    pos_
                ),
                FOLLOW_SMOOTH
            )
        );


    // 注視点も滑らかに移動
    targetPos_ =
        VAdd(
            targetPos_,
            VScale(
                VSub(
                    lookPos,
                    targetPos_
                ),
                FOLLOW_SMOOTH
            )
        );


    cameraUp_ =
        AsoUtility::DIR_U;
}

void Camera::SetBeforeDrawSelfShot(void)
{
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

    // 上
    pos_.y += 100.0f;

    // 注視点
    targetPos_ = playerPos;

    // 胸～手のあたりを見る
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

    // 少し上
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
    // プレイヤーより前を見る
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