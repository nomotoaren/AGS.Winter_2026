// VS/PS共通
#include "../Common/VertexToPixelHeader.hlsli"

// IN
#define PS_INPUT VertexToPixelLit

// PS
#include "../Common/Pixel/PixelShader3DHeader.hlsli"

cbuffer cbParam : register(b4)
{
    float3 g_light_dir;
    float dummy;
    float3 g_camera_pos;
    float dummy2;
    float4 g_diff_color;
    float4 g_ambient_color;
}

float4 main(PS_INPUT PSInput) : SV_TARGET0
{
    float4 color;

	// テクスチャーの色を取得
    color = diffuseMapTexture.Sample(diffuseMapSampler, PSInput.uv);
    if (color.a < 0.01f)
    {
        discard;
    }
    
    // 法線
    float3 norm = PSInput.normal;

    // 拡散光の強さ
    float difDot = dot(norm, -g_light_dir);
    //return float4(difDot, difDot, difDot, 1.0f);

    // 頂点(頂点からのカメラ方向)
    float3 toEye = normalize(g_camera_pos - PSInput.worldPos);

    // リムの強さ
    float rimDot = dot(norm, toEye);

    // 0.0f～1.0f(視線方向と一致が逆が1.0)
    rimDot = abs(rimDot);
    // 0.0f～1.0f(視線方向と垂直が1.0)
    rimDot = 1.0f - rimDot;
    // リムの強さを調整
    rimDot = pow(rimDot, 2.0f);

    // リムカラー
    float4 rimColor = float4(1.0f, 0.0f, 0.0f, 1.0f);

    // 拡散光
    float3 diffuse = color.rgb * (difDot * g_diff_color).rgb;

    // 色の合成
    float3 rgb = diffuse + (rimDot * rimColor.rgb) + g_ambient_color.rgb;

    return float4(rgb, color.a);
}