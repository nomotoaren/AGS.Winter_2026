#include "EffekseerEffect.h"
#include "../Application.h"

EffekseerEffect* EffekseerEffect::instance_ = nullptr;

EffekseerEffect::EffekseerEffect(void)
{
    hitEffectId_ = -1;
    playHitEffectHandle_ = -1;
    chargeEffectId_ = -1;
    playChargeEffectHandle_ = -1;
}

EffekseerEffect::~EffekseerEffect(void)
{

}

void EffekseerEffect::Init(void)
{
    //SetUseDirect3DVersion(DX_DIRECT3D_11);

    //if (Effekseer_Init(8000) == -1) {
    //    DxLib_End();
    //    return;
    //}

    //SetChangeScreenModeGraphicsSystemResetFlag(false);

    //Effekseer_SetGraphicsDeviceLostCallbackFunctions();

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
    const VECTOR& pos
)
{
    if (chargeEffectId_ == -1)
    {
        return;
    }

    if (playChargeEffectHandle_ != -1)
    {
        return;
    }

    playChargeEffectHandle_ =
        PlayEffekseer3DEffect(
            chargeEffectId_
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

    SetScalePlayingEffekseer3DEffect(
        playChargeEffectHandle_,
        10.0f,
        10.0f,
        10.0f
    );
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