#include "../Common/Pixel/PixelShader2DHeader.hlsli"

cbuffer cbParam : register(b4)
{
    // x : ŠÔ’â~ƒJƒ‰[‹­“x
    // y : ŠÔ’â~‚ÌL‚ª‚è 0`1
    float4 g_timeStop;
}

float4 main(PS_INPUT PSInput) : SV_TARGET
{
    float4 srcCol =
        tex.Sample(
            texSampler,
            PSInput.uv
        );

    if (srcCol.a < 0.01f)
    {
        discard;
    }


    // =====================================
    // ŠÔ’â~ƒJƒ‰[
    // =====================================

    float gray =
        dot(
            srcCol.rgb,
            float3(
                0.299f,
                0.587f,
                0.114f
            )
        );

    float3 lowColor =
        lerp(
            srcCol.rgb,
            float3(gray, gray, gray),
            0.75f
        );

    float3 stopColor =
        lowColor *
        float3(
            0.65f,
            0.80f,
            1.25f
        );


    // =====================================
    // ‰æ–Ê’†‰›‚©‚ç‚Ì‹——£
    // =====================================

    float2 center =
        float2(0.5f, 0.5f);

    float dist =
        distance(
            PSInput.uv,
            center
        );


    // =====================================
    // ‰~‚ğL‚°‚é
    // =====================================

    // ‰æ–Ê‚ÌŠp‚Ü‚Å“Í‚­‚æ‚¤‚É­‚µ‘å‚«‚ß
    float radius =
        g_timeStop.y * 0.8f;

    // ‹«ŠE‚ğ­‚µŠŠ‚ç‚©‚É‚·‚é
    float mask =
        1.0f -
        smoothstep(
            radius - 0.03f,
            radius,
            dist
        );


    // =====================================
    // ’Êí ¨ ŠÔ’â~
    // =====================================

    float power =
        mask *
        saturate(g_timeStop.x);

    float3 resultColor =
        lerp(
            srcCol.rgb,
            stopColor,
            power
        );

    return float4(
        resultColor,
        srcCol.a
    );
}