// VS/PS共通
#include "../Common/VertexToPixelHeader.hlsli"

// IN
#define PS_INPUT VertexToPixelLit

// PS
#include "../Common/Pixel/PixelShader3DHeader.hlsli"

cbuffer cbParam : register(b4)
{
    //float3 g_light_dir;
    //float3 g_camera_pos;
    //float a_light;
    //float4 g_diff_color;
    //float4 g_specular_color;
    //float g_specular_pow;
    //float4 g_ambient_color;
    float3 dummy;
    float g_time;
    
}

float4 main(PS_INPUT PSInput) : SV_TARGET0
{
    // UVスクロール
    float2 uv = PSInput.uv;
    uv.x = 1.0f + uv.x - frac(g_time * 0.1f);
    uv.x = frac(uv.x);
    
    float4 color;

	// テクスチャーの色を取得
    color = diffuseMapTexture.Sample(diffuseMapSampler, uv);
    if (color.a < 0.01f)
    {
        discard;
    }
    
    return color;
}