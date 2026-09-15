#include "../Common/Pixel/PixelShader2DHeader.hlsli"

// 定数バッファ：スロット4番目(b4と書く)
cbuffer cbParam : register(b4)
{
	float4 g_color;
	float  g_time;
}

float4 main(PS_INPUT PSInput) : SV_TARGET
{

	float2 uv = PSInput.uv;

	// UV座標とテクスチャを参照して、最適な色を取得する
	float4 srcCol = tex.Sample(texSampler, uv);
	
	// 縦の大きさと時間で-1.0～1.0の値を作る
	float area = sin(uv.y * 2.0f - g_time * 0.5f);

	// 縦に特定範囲を作る
	// 1 or 0 …… 1 <= 2 = 1、2 <= 1 = 0 
	float isArea = step(0.996f, area * area);

	srcCol.rgb -= abs(sin(uv.y * 60.0f + g_time * 1.0f)) * 0.05f;
	srcCol.rgb -= abs(sin(uv.y * 100.0f - g_time * 2.0f)) * 0.15f;

	// 一定エリア処理
	// ------------------------------------------------------------------------------
	// 一定エリア以外、間隔を空けて、縞々模様(下地)を作る(色の減算で色を暗くする)
	//srcCol.rgb -= (1.0f - isArea) * abs(sin(uv.y *  60.0f + g_time * 1.0f)) * 0.05f;
	//srcCol.rgb -= (1.0f - isArea) * abs(sin(uv.y * 100.0f - g_time * 2.0f)) * 0.15f;

	// 特定範囲だけ明るくする(色の加算で明るくする)
	//srcCol.rgb += isArea * 0.5f;
	// ------------------------------------------------------------------------------

	// 下地だけ
	//srcCol.rgb -= abs(sin(uv.y *  60.0f + g_time * 1.0f)) * 0.10f;
	//srcCol.rgb -= abs(sin(uv.y * 100.0f - g_time * 2.0f)) * 0.15f;

	return srcCol;

}
