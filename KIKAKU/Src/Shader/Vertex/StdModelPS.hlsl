// VS/PS共通
#include "../Common/VertexToPixelHeader.hlsli"

// IN
#define PS_INPUT VertexToPixelLit

// PS
#include "../Common/Pixel/PixelShader3DHeader.hlsli"

float4 main(PS_INPUT PSInput) : SV_TARGET0
{

    float4 color;

	// テクスチャーの色を取得
    color = diffuseMapTexture.Sample(diffuseMapSampler, PSInput.uv);
    if (color.a < 0.01f)
    {
        discard;
    }
    
    // テクスチャーの色を黄色に変更
    color.rgb = float3(1.0f, 1.0f, 0.0f);
    
	// 関数の戻り値がラスタライザに渡される
    return color;

}
