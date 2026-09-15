#include "TimeStopController.h"
#include <DxLib.h>
#include "../Application.h"
#include "../Manager/InputManager.h"
#include "../Manager/SceneManager.h"
#include "../Renderer/PixelMaterial.h"
#include "../Renderer/PixelRenderer.h"

TimeStopController::TimeStopController(void)
    :
    state_(STATE::NORMAL),
    effectTimer_(0.0f),
    stopTimer_(0.0f),
    justFinished_(false)
{
}


TimeStopController::~TimeStopController(void)
{
}


void TimeStopController::Init(void)
{
    state_ = STATE::NORMAL;
    effectTimer_ = 0.0f;
    stopTimer_ = 0.0f;

    // TimeStop Shader
    material_ =
        std::make_unique<PixelMaterial>(
            "TimeStopPS.cso",
            1
        );

    material_->AddConstBuf(
        {
            1.0f,
            0.0f,
            0.0f,
            0.0f
        }
    );

    material_->AddTextureBuf(
        SceneManager::GetInstance().GetMainScreen()
    );


    renderer_ =
        std::make_unique<PixelRenderer>(
            *material_
        );

    renderer_->MakeSquereVertex(
        Vector2(0, 0),
        Vector2(
            Application::SCREEN_SIZE_X,
            Application::SCREEN_SIZE_Y
        )
    );
}

void TimeStopController::Update(void)
{
    justFinished_ = false;

    InputManager& ins =
        InputManager::GetInstance();

    // Q‚ÅŽžŠÔ’âŽ~ŠJŽn
    if (state_ == STATE::NORMAL)
    {
        if (ins.IsTrgDown(KEY_INPUT_Q))
        {
            state_ = STATE::STOP;

            // ŠJŽn‰‰o
            effectTimer_ = 0.0f;

            // ’âŽ~ŽžŠÔ
            stopTimer_ = 0.0f;

            justFinished_ = false;
        }

        return;
    }

    // ŽžŠÔ’âŽ~’†
    float deltaTime =
        SceneManager::GetInstance().GetDeltaTime();

    // ŠJŽn‰‰o
    effectTimer_ += deltaTime;

    if (effectTimer_ > EFFECT_TIME)
    {
        effectTimer_ = EFFECT_TIME;
    }

    // ŽžŠÔ’âŽ~ŽžŠÔ
    stopTimer_ += deltaTime;

    // ŽžŠÔØ‚ê
    if (stopTimer_ >= STOP_TIME)
    {
        state_ = STATE::NORMAL;

        stopTimer_ = 0.0f;
        effectTimer_ = 0.0f;

        justFinished_ = true;
    }
}
bool TimeStopController::IsStopping(void) const
{
    return state_ == STATE::STOP;
}

void TimeStopController::DrawEffect(
    int postEffectScreen)
{
    if (!IsStopping())
    {
        return;
    }

    float effectRate =
        effectTimer_ /
        EFFECT_TIME;

    if (effectRate > 1.0f)
    {
        effectRate = 1.0f;
    }


    int mainScreen =
        SceneManager::GetInstance().GetMainScreen();

    SetDrawScreen(
        postEffectScreen
    );

    ClearDrawScreen();


    material_->SetConstBuf(
        0,
        {
            1.0f,
            effectRate,
            0.0f,
            0.0f
        }
    );
    renderer_->Draw();

    SetDrawScreen(
        mainScreen
    );

    DrawGraph(
        0,
        0,
        postEffectScreen,
        false
    );
}

bool TimeStopController::IsJustFinished(void) const
{
    return justFinished_;
}