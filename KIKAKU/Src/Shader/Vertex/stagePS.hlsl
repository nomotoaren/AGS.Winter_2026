// VS/PS共通
#include "../Common/VertexToPixelHeader.hlsli"

// IN
#define PS_INPUT VertexToPixelLit

// PS
#include "../Common/Pixel/PixelShader3DHeader.hlsli"

cbuffer cbParam : register(b4)
{
    float3 g_light_dir;
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
    
    // ランパート反射
    float lightDot = dot(PSInput.normal, -g_light_dir);
    float3 rgb = color.rgb * lightDot;
    rgb += float3(0.2f, 0.2f, 0.2f);
    return float4(rgb, color.a);
    
    // 関数の戻り値がラスタライザに渡される
    return color;
    
//    return float4(
//PSInput.normal.x, PSInput.normal.x, PSInput.normal.x, 1.0f);
    
}
