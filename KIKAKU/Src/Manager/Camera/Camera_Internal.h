#pragma once
#include "../../Manager/AppFlow.h"
#include "../../Manager/SceneManager.h"

// Camera ‚ÌŠeƒtƒ@ƒCƒ‹‚Å‹¤’Ê‚Ég‚¤¬‚³‚ÈŠÖ”
namespace CameraInternal
{
    // ~‚Ü‚Á‚Ä‚¢‚éŠÔ‚Í 0 •b‚Æ‚µ‚Äˆµ‚¤(ƒJƒƒ‰‚ÌŠÔ‚ªi‚Ü‚È‚­‚È‚é)
    inline float CameraDeltaTime(void)
    {
        return AppFlow::IsEffectFrozen()
            ? 0.0f
            : SceneManager::GetInstance().GetDeltaTime();
    }

    // 1ƒtƒŒ[ƒ€‚ ‚½‚è‚Ì—Ê‚ÉŠ|‚¯‚é”{—¦
    //   (’Ç]‚Ì‘¬‚³‚Í 144fps ‚Ì1ƒtƒŒ[ƒ€•ª‚ÅŒˆ‚ß‚Ä‚ ‚é‚Ì‚ÅA60fps ‚È‚ç–ñ2.4”{‚É‚µ‚Ä“¯‚¶‘¬‚³‚É‚·‚é)
    inline float CameraFrameScale(void)
    {
        float s = CameraDeltaTime() * 144.0f;

        // ˆ——‚¿‚µ‚½1ƒtƒŒ[ƒ€‚ÅA‹}‚É“®‚«‚·‚¬‚È‚¢‚æ‚¤‚É‚·‚é
        if (s > 4.0f)
        {
            s = 4.0f;
        }

        return s;
    }
}
