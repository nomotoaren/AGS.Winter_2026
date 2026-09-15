#pragma once

#include <memory>

class PixelMaterial;
class PixelRenderer;

class TimeStopController
{
public:
    // ŠÔ’â~‚Å‚«‚éÅ‘åŠÔ
    static constexpr float STOP_TIME = 6.0f;

    enum class STATE
    {
        NORMAL,
        STOP
    };

    // ŠÔ’â~‰‰o‚ªL‚ª‚éŠÔ
    static constexpr float EFFECT_TIME = 0.35f;

    TimeStopController(void);
    ~TimeStopController(void);

    void Init(void);
    void Update(void);

    // ŠÔ’â~ƒVƒF[ƒ_‚ğ•`‰æ
    void DrawEffect(int postEffectScreen);

    // ŠÔ’â~’†‚©
    bool IsStopping(void) const;

    bool IsJustFinished(void) const;
private:

    // ó‘Ô
    STATE state_;

    // ŠÔ’â~ŠJn‰‰o
    float effectTimer_;

    // ŠÔ’â~‚µ‚Ä‚©‚ç‚ÌŒo‰ßŠÔ
    float stopTimer_;

    bool justFinished_;
    // Shader
    std::unique_ptr<PixelMaterial> material_;
    std::unique_ptr<PixelRenderer> renderer_;
};