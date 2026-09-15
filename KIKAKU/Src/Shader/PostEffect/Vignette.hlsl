#include "../Common/Pixel/PixelShader2DHeader.hlsli"

// 定数バッファ：スロット4番目(b4と書く)
cbuffer cbParam : register(b4)
{
	float g_vine_pow;// ビネットの強度
}

float4 main(PS_INPUT PSInput) : SV_TARGET
{

	float2 uv = PSInput.uv;
	float x = abs(uv.x - 0.5f);	// 0.0～1.0f → -0.5～0.5 → 0.0～0.5
	float y = abs(uv.y - 0.5f);	// 0.0～1.0f → -0.5～0.5 → 0.0～0.5
	float dis = (x * x + y * y);

	// ビネットの強度を計算
	float vignette = 1.0f - saturate(dis * g_vine_pow);

	// UV座標とテクスチャを参照して、最適な色を取得する
	float4 ret = tex.Sample(texSampler, PSInput.uv);
	ret.rgb *= vignette;

	return ret;

}
