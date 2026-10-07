#include "EffekseerEffect.h"
#include <cmath>
#include "../Application.h"

EffekseerEffect* EffekseerEffect::instance_ = nullptr;

namespace
{
    // 変身のエフェクト(気溜め + 爆発)に共通でかける色(0~255)
    // 色を変えたいときは、ここの3つの数字を変える
    constexpr int TRANSFORM_COLOR_R = 255;
    constexpr int TRANSFORM_COLOR_G = 215;
    constexpr int TRANSFORM_COLOR_B = 90;
    constexpr int TRANSFORM_COLOR_A = 255;

    // 変身のエフェクトの大きさ・速さ
    // (大きすぎるとカメラが爆発の内側に入って何も見えなくなるので、まず小さめ)
    constexpr float TRANSFORM_BURST_SCALE = 12.0f;
    constexpr float TRANSFORM_BURST_SPEED = 1.0f;

    // 爆発に色をかけるか(出ないときは false にして試す)
    constexpr bool TRANSFORM_BURST_TINT = true;
    constexpr float TRANSFORM_CHARGE_SCALE = 10.0f;		// 変身中の気溜めの大きさ(MagicTornade2 用。大きい/小さいときはここを変える)
    constexpr bool TRANSFORM_CHARGE_TINT = false;		// true にすると、変身の色(水色)をかける

    // 再生中のエフェクトに変身の色をかける
    void ApplyTransformColor(int playHandle)
    {
        SetColorPlayingEffekseer3DEffect(
            playHandle,
            TRANSFORM_COLOR_R,
            TRANSFORM_COLOR_G,
            TRANSFORM_COLOR_B,
            TRANSFORM_COLOR_A
        );
    }
}

EffekseerEffect::EffekseerEffect(void)
    :
    hitEffectId_(-1),
    playHitEffectHandle_(-1),
    chargeEffectId_(-1),
    playChargeEffectHandle_(-1),
    transformChargeEffectId_(-1),
    transformEffectId_(-1),
    playTransformEffectHandle_(-1),
    shalshutEffectId_(-1),
    PlayshalshuEffectHandle(-1),
    slashHandle_(-1),
    isSlashing_(false),
    tutorialEffectId_(-1),
    playTutorialHandle(-1),
    finisyuId(-1),
    finisyu2Id(-1),
    finisyu3Id(-1),
    finisyu4Id(-1),
    finisyu5Id(-1),
    finisyu6Id(-1),
    playFinisyuHandle(-1),
    playFinisyu2Handle(-1),
    playFinisyu3Handle(-1),
    playFinisyu4Handle(-1),
    playFinisyu5Handle(-1),
    playFinisyu6Handle(-1)
{
}

EffekseerEffect::~EffekseerEffect(void)
{

}

void EffekseerEffect::Init(void)
{
    shalshutEffectId_ = LoadEffekseerEffect(
        (Application::PATH_EFFECT + "slashu.efkefc").c_str());

    if (shalshutEffectId_ == -1) {
        MessageBoxA(NULL, "エフェクトの読み込みに失敗しました。パスやファイルを確認してください。", "エラー", MB_OK);
    }

    finisyuId = LoadEffekseerEffect(
        (Application::PATH_EFFECT + "Fiyaer.efkefc").c_str());

    if (finisyuId == -1) {
        MessageBoxA(NULL, "エフェクトの読み込みに失敗しました。パスやファイルを確認してください。", "エラー", MB_OK);
    }

    finisyu2Id = LoadEffekseerEffect(
        (Application::PATH_EFFECT + "mizu.efkefc").c_str());

    if (finisyu2Id == -1) {
        MessageBoxA(NULL, "エフェクトの読み込みに失敗しました。パスやファイルを確認してください。", "エラー", MB_OK);
    }

    finisyu3Id = LoadEffekseerEffect(
        (Application::PATH_EFFECT + "kori.efkefc").c_str());

    if (finisyu3Id == -1) {
        MessageBoxA(NULL, "エフェクトの読み込みに失敗しました。パスやファイルを確認してください。", "エラー", MB_OK);
    }

    finisyu4Id = LoadEffekseerEffect(
        (Application::PATH_EFFECT + "bakuhatu.efkefc").c_str());

    if (finisyu4Id == -1) {
        MessageBoxA(NULL, "エフェクトの読み込みに失敗しました。パスやファイルを確認してください。", "エラー", MB_OK);
    }

    finisyu5Id = LoadEffekseerEffect(
        (Application::PATH_EFFECT + "comboLast.efkefc").c_str());

    if (finisyu5Id == -1) {
        MessageBoxA(NULL, "エフェクトの読み込みに失敗しました。パスやファイルを確認してください。", "エラー", MB_OK);
    }

    finisyu6Id = LoadEffekseerEffect(
        (Application::PATH_EFFECT + "meteo.efkefc").c_str());

    if (finisyu6Id == -1) {
        MessageBoxA(NULL, "エフェクトの読み込みに失敗しました。パスやファイルを確認してください。", "エラー", MB_OK);
    }

    tutorialEffectId_ = LoadEffekseerEffect(
        (Application::PATH_EFFECT + "nomotoaren.efkefc").c_str());

    if (tutorialEffectId_ == -1) {
        MessageBoxA(NULL, "エフェクトの読み込みに失敗しました。パスやファイルを確認してください。", "エラー", MB_OK);
    }

    hitEffectId_ = LoadEffekseerEffect(
        (Application::PATH_EFFECT + "Blast.efkefc").c_str()
    );

    if (hitEffectId_ == -1)
    {
        MessageBoxA(
            NULL,
            "Hit.efkefcの読み込みに失敗しました。",
            "エラー",
            MB_OK
        );
    }

    chargeEffectId_ = LoadEffekseerEffect(
        (Application::PATH_EFFECT + "MagicTornade.efkefc").c_str()
    );

    if (chargeEffectId_ == -1)
    {
        MessageBoxA(
            NULL,
            "MagicTornade.efkefcの読み込みに失敗しました。",
            "エラー",
            MB_OK
        );
    }

    // 変身中の気溜めエフェクト
    transformChargeEffectId_ = LoadEffekseerEffect(
        (Application::PATH_EFFECT + "MagicTornade2.efkefc").c_str()
    );

    if (chargeEffectId_ == -1)
    {
        MessageBoxA(
            NULL,
            "MagicTornade.efkefcの読み込みに失敗しました。",
            "エラー",
            MB_OK
        );
    }

    // 変身の爆発エフェクト
    transformEffectId_ = LoadEffekseerEffect(
        (Application::PATH_EFFECT + "bakuhatu.efkefc").c_str()
    );

    if (transformEffectId_ == -1)
    {
        MessageBoxA(
            NULL,
            "bakuhatu.efkefcの読み込みに失敗しました。",
            "エラー",
            MB_OK
        );
    }

    // かめはめ波
    kamehameEffectId_ = LoadEffekseerEffect(
        (Application::PATH_EFFECT + "blue_laser.efkefc").c_str()
    );

    if (kamehameEffectId_ == -1)
    {
        MessageBoxA(
            NULL,
            "blue_laser.efkefcの読み込みに失敗しました。",
            "エラー",
            MB_OK
        );
    }
}

void EffekseerEffect::Update(void)
{
    UpdateEffekseer3D();
}

void EffekseerEffect::Draw(void)
{
    DrawEffekseer3D();
}

void EffekseerEffect::Release(void)
{
    Effkseer_End();
}

void EffekseerEffect::Delete(void)
{
    DeleteEffekseerEffect(playTutorialHandle);
}

void EffekseerEffect::PlayTutorialEffect(const VECTOR& pos, float rotY)
{
    playTutorialHandle = PlayEffekseer3DEffect(tutorialEffectId_);
    SetPosPlayingEffekseer3DEffect(
        playTutorialHandle,
        pos.x,
        pos.y,
        pos.z
    );
    SetRotationPlayingEffekseer3DEffect(
        playTutorialHandle,
        0.0f,
        rotY,
        0.0f
    );
    SetScalePlayingEffekseer3DEffect(
        playTutorialHandle,
        50.0f, 50.0f, 50.0f
    );
    SetSpeedPlayingEffekseer3DEffect(
        playTutorialHandle,
        0.2f
    );
}

void EffekseerEffect::PlayComboEffect(const VECTOR& pos, float rotY)
{
    playFinisyuHandle = PlayEffekseer3DEffect(finisyuId);

    SetPosPlayingEffekseer3DEffect(
        playFinisyuHandle,
        pos.x,
        pos.y,
        pos.z
    );

    SetRotationPlayingEffekseer3DEffect(
        playFinisyuHandle,
        0.0f,
        rotY,
        0.0f
    );

    SetScalePlayingEffekseer3DEffect(
        playFinisyuHandle,
        5.0f, 5.0f, 5.0f
    );

    SetSpeedPlayingEffekseer3DEffect(
        playFinisyuHandle,
        0.2f
    );
}

void EffekseerEffect::PlayComboEffect2(const VECTOR& pos, float rotY)
{
    playFinisyu2Handle = PlayEffekseer3DEffect(finisyu2Id);

    SetPosPlayingEffekseer3DEffect(
        playFinisyu2Handle,
        pos.x,
        pos.y,
        pos.z
    );

    SetRotationPlayingEffekseer3DEffect(
        playFinisyu2Handle,
        0.0f,
        rotY,
        0.0f
    );

    SetScalePlayingEffekseer3DEffect(
        playFinisyu2Handle,
        50.0f, 50.0f, 50.0f
    );

    SetSpeedPlayingEffekseer3DEffect(
        playFinisyu2Handle,
        0.2f
    );
}

void EffekseerEffect::PlayComboEffect3(const VECTOR& pos, float rotY)
{
    playFinisyu3Handle = PlayEffekseer3DEffect(finisyu3Id);

    SetPosPlayingEffekseer3DEffect(
        playFinisyu3Handle,
        pos.x,
        pos.y,
        pos.z
    );

    SetRotationPlayingEffekseer3DEffect(
        playFinisyu3Handle,
        0.0f,
        rotY,
        0.0f
    );

    SetScalePlayingEffekseer3DEffect(
        playFinisyu3Handle,
        50.0f, 50.0f, 50.0f
    );

    SetSpeedPlayingEffekseer3DEffect(
        playFinisyu3Handle,
        0.2f
    );
}

void EffekseerEffect::PlayComboEffect4(const VECTOR& pos, float rotY)
{
    playFinisyu4Handle = PlayEffekseer3DEffect(finisyu4Id);

    SetPosPlayingEffekseer3DEffect(
        playFinisyu4Handle,
        pos.x,
        pos.y,
        pos.z
    );

    SetRotationPlayingEffekseer3DEffect(
        playFinisyu4Handle,
        0.0f,
        rotY,
        0.0f
    );

    SetScalePlayingEffekseer3DEffect(
        playFinisyu4Handle,
        50.0f, 50.0f, 50.0f
    );

    SetSpeedPlayingEffekseer3DEffect(
        playFinisyu4Handle,
        0.2f
    );
}

void EffekseerEffect::PlayComboEffect5(const VECTOR& pos, float rotY)
{
    playFinisyu5Handle = PlayEffekseer3DEffect(finisyu5Id);

    SetPosPlayingEffekseer3DEffect(
        playFinisyu5Handle,
        pos.x,
        pos.y,
        pos.z
    );

    SetRotationPlayingEffekseer3DEffect(
        playFinisyu5Handle,
        0.0f,
        rotY,
        0.0f
    );

    SetScalePlayingEffekseer3DEffect(
        playFinisyu5Handle,
        50.0f, 50.0f, 50.0f
    );

    SetSpeedPlayingEffekseer3DEffect(
        playFinisyu5Handle,
        0.2f
    );
}

void EffekseerEffect::PlayComboEffect6(const VECTOR& pos, float rotY)
{
    playFinisyu6Handle = PlayEffekseer3DEffect(finisyu6Id);

    SetPosPlayingEffekseer3DEffect(
        playFinisyu6Handle,
        pos.x,
        pos.y,
        pos.z
    );

    SetRotationPlayingEffekseer3DEffect(
        playFinisyu6Handle,
        0.0f,
        rotY,
        0.0f
    );

    SetScalePlayingEffekseer3DEffect(
        playFinisyu6Handle,
        50.0f, 50.0f, 50.0f
    );

    SetSpeedPlayingEffekseer3DEffect(
        playFinisyu6Handle,
        0.2f
    );
}

void EffekseerEffect::PlayHitEffect(
    const VECTOR& pos,
    float rotY
)
{
    if (hitEffectId_ == -1)
    {
        return;
    }

    playHitEffectHandle_ =
        PlayEffekseer3DEffect(
            hitEffectId_
        );

    if (playHitEffectHandle_ == -1)
    {
        return;
    }

    SetPosPlayingEffekseer3DEffect(
        playHitEffectHandle_,
        pos.x,
        pos.y,
        pos.z
    );

    SetRotationPlayingEffekseer3DEffect(
        playHitEffectHandle_,
        0.0f,
        rotY,
        0.0f
    );

    SetScalePlayingEffekseer3DEffect(
        playHitEffectHandle_,
        10.0f,
        10.0f,
        10.0f
    );

    SetSpeedPlayingEffekseer3DEffect(
        playHitEffectHandle_,
        1.0f
    );
}

void EffekseerEffect::PlayChargeEffect(
    const VECTOR& pos,
    bool useTransformColor
)
{
    // 変身中は専用のエフェクトを使う(読み込めていなければ普通の気溜めで代用)
    const bool isTransform = useTransformColor && transformChargeEffectId_ != -1;
    const int effectId = isTransform ? transformChargeEffectId_ : chargeEffectId_;

    if (effectId == -1)
    {
        return;
    }

    if (playChargeEffectHandle_ != -1)
    {
        return;
    }

    playChargeEffectHandle_ =
        PlayEffekseer3DEffect(
            effectId
        );

    if (playChargeEffectHandle_ == -1)
    {
        return;
    }

    SetPosPlayingEffekseer3DEffect(
        playChargeEffectHandle_,
        pos.x,
        pos.y,
        pos.z
    );

    const float scale = isTransform ? TRANSFORM_CHARGE_SCALE : 10.0f;

    SetScalePlayingEffekseer3DEffect(
        playChargeEffectHandle_,
        scale,
        scale,
        scale
    );

    // 変身用のエフェクトは、そのままの色で出す(色をかけたいときは TRANSFORM_CHARGE_TINT)
    // 代用の気溜めを使うときだけ、爆発と同じ色にする
    if (useTransformColor && (!isTransform || TRANSFORM_CHARGE_TINT))
    {
        ApplyTransformColor(playChargeEffectHandle_);
    }
}

void EffekseerEffect::UpdateChargeEffect(
    const VECTOR& pos
)
{
    if (playChargeEffectHandle_ == -1)
    {
        return;
    }

    SetPosPlayingEffekseer3DEffect(
        playChargeEffectHandle_,
        pos.x,
        pos.y,
        pos.z
    );
}

void EffekseerEffect::StopChargeEffect(void)
{
    if (playChargeEffectHandle_ == -1)
    {
        return;
    }

    StopEffekseer3DEffect(
        playChargeEffectHandle_
    );

    playChargeEffectHandle_ = -1;
}

// 変身の瞬間の爆発エフェクト(気溜めと同じ色)
void EffekseerEffect::PlayTransformEffect(const VECTOR& pos)
{
    if (transformEffectId_ == -1)
    {
        return;
    }

    playTransformEffectHandle_ =
        PlayEffekseer3DEffect(
            transformEffectId_
        );

    if (playTransformEffectHandle_ == -1)
    {
        return;
    }

    SetPosPlayingEffekseer3DEffect(
        playTransformEffectHandle_,
        pos.x,
        pos.y,
        pos.z
    );

    SetScalePlayingEffekseer3DEffect(
        playTransformEffectHandle_,
        TRANSFORM_BURST_SCALE,
        TRANSFORM_BURST_SCALE,
        TRANSFORM_BURST_SCALE
    );

    SetSpeedPlayingEffekseer3DEffect(
        playTransformEffectHandle_,
        TRANSFORM_BURST_SPEED
    );

    if (TRANSFORM_BURST_TINT)
    {
        ApplyTransformColor(playTransformEffectHandle_);
    }
}

//------------------------------------------------------------
// かめはめ波
//------------------------------------------------------------
namespace
{
    // 進行方向(dir)を向くための回転(ラジアン)。ビームはエフェクトの +Z 方向へ伸びる
    // 上下が逆に出るときは、PITCH_SIGN を -1.0f に変える
    constexpr float KAMEHAME_PITCH_SIGN = 1.0f;

    // エフェクト全体にかける色(0~255)。紫が気になるときはここを変える
    constexpr int KAMEHAME_TINT_R = 90;
    constexpr int KAMEHAME_TINT_G = 225;
    constexpr int KAMEHAME_TINT_B = 255;

    // ビームが後ろへ出てしまうので、エフェクトの向きを反転させる(前へ出るなら 1.0f に戻す)
    constexpr float KAMEHAME_AXIS_SIGN = -1.0f;

    void CalcDirRotation(const VECTOR& dir, float& rotX, float& rotY)
    {
        const float dx = dir.x * KAMEHAME_AXIS_SIGN;
        const float dy = dir.y * KAMEHAME_AXIS_SIGN;
        const float dz = dir.z * KAMEHAME_AXIS_SIGN;
        const float horizontal = sqrtf(dx * dx + dz * dz);

        rotY = atan2f(dx, dz);
        rotX = -atan2f(dy, horizontal) * KAMEHAME_PITCH_SIGN;
    }
}

void EffekseerEffect::PlayKamehameEffect(
    const VECTOR& pos, const VECTOR& dir, float scale, float speed)
{
    if (kamehameEffectId_ == -1)
    {
        return;
    }

    // 前のが残っていたら止める
    StopKamehameEffect();

    playKamehameEffectHandle_ = PlayEffekseer3DEffect(kamehameEffectId_);

    if (playKamehameEffectHandle_ == -1)
    {
        return;
    }

    SetScalePlayingEffekseer3DEffect(playKamehameEffectHandle_, scale, scale, scale);
    SetSpeedPlayingEffekseer3DEffect(playKamehameEffectHandle_, speed);

    // 手元の気弾(水色)に合わせて、エフェクトの紫っぽい部分を水色に寄せる
    SetColorPlayingEffekseer3DEffect(
        playKamehameEffectHandle_,
        KAMEHAME_TINT_R, KAMEHAME_TINT_G, KAMEHAME_TINT_B, 255);

    UpdateKamehameEffect(pos, dir);
}

void EffekseerEffect::UpdateKamehameEffect(const VECTOR& pos, const VECTOR& dir)
{
    if (playKamehameEffectHandle_ == -1)
    {
        return;
    }

    float rotX = 0.0f;
    float rotY = 0.0f;
    CalcDirRotation(dir, rotX, rotY);

    SetPosPlayingEffekseer3DEffect(playKamehameEffectHandle_, pos.x, pos.y, pos.z);
    SetRotationPlayingEffekseer3DEffect(playKamehameEffectHandle_, rotX, rotY, 0.0f);
}

void EffekseerEffect::SetKamehameEffectScale(float x, float y, float z)
{
    if (playKamehameEffectHandle_ == -1)
    {
        return;
    }

    SetScalePlayingEffekseer3DEffect(playKamehameEffectHandle_, x, y, z);
}

void EffekseerEffect::SetKamehameEffectSpeed(float speed)
{
    if (playKamehameEffectHandle_ == -1)
    {
        return;
    }

    SetSpeedPlayingEffekseer3DEffect(playKamehameEffectHandle_, speed);
}

void EffekseerEffect::StopKamehameEffect(void)
{
    if (playKamehameEffectHandle_ == -1)
    {
        return;
    }

    StopEffekseer3DEffect(playKamehameEffectHandle_);
    playKamehameEffectHandle_ = -1;
}