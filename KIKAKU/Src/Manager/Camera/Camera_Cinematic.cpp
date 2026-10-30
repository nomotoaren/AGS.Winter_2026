#include <DxLib.h>
#include "../../Utility/AsoUtility.h"
#include "Camera.h"
#include "Camera_Internal.h"

using namespace CameraInternal;

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
