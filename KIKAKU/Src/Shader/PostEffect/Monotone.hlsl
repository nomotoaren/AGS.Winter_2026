#include "../Common/Pixel/PixelShader2DHeader.hlsli"

// 定数バッファ：スロット4番目(b4と書く)
cbuffer cbParam : register(b4)
{
	float4 g_color;
}

float4 main(PS_INPUT PSInput) : SV_TARGET
{

	// UV座標とテクスチャを参照して、最適な色を取得する
	float4 srcCol = tex.Sample(texSampler, PSInput.uv);
	if (srcCol.a < 0.01f)
	{
		// 描画しない(アルファテスト)
		discard;
	}

	float4 dstCol = srcCol;

	// ３色の平均モノトーン
	float gray = (dstCol.r + dstCol.g + dstCol.b) / 3.0f;
	dstCol.rgb = float3(gray, gray, gray);
	dstCol.rgb *= g_color.rgb;

	/*
	// セピア
	float3 sepia = dstCol.rgb;
	sepia.r = dot(dstCol.rgb, float3(0.393f, 0.769f, 0.189f));
	sepia.g = dot(dstCol.rgb, float3(0.349f, 0.686f, 0.168f));
	sepia.b = dot(dstCol.rgb, float3(0.272f, 0.534f, 0.131f));
	dstCol.rgb = lerp(dstCol.rbg, sepia, 1.0f);
	*/

	return dstCol;

}
