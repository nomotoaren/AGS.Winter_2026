#include "../Common/Pixel/PixelShader2DHeader.hlsli"

// 定数バッファ：スロット4番目(b4と書く)
cbuffer cbParam : register(b4)
{
	float g_none;
}

float4 main(PS_INPUT PSInput) : SV_TARGET
{

	float4 _Distortion_Amount = float4(1.0f, 1.2f, 1.0f, 1.0f);
	float4 _Distortion_CenterScale = float4(0.0f, 0.2f, 2.0f, 1.0f);
	
	float2 uv = PSInput.uv;
	uv = ((uv - 0.5f) * _Distortion_Amount.z) + 0.5f;
	float2 ruv = _Distortion_CenterScale.zw * (uv - 0.5f - _Distortion_CenterScale.xy);
	float ru = length(float2(ruv));

	if (_Distortion_Amount.w > 0.0f)
	{
		float wu = ru * _Distortion_Amount.x;
		ru = tan(wu) * (1.0f / (ru * _Distortion_Amount.y));
		uv = uv + ruv * (ru - 1.0f);
	}
	else
	{
		ru = (1.0f / ru) * _Distortion_Amount.x * atan(ru * _Distortion_Amount.y);
		uv = uv + ruv * (ru - 1.0f);
	}

	return tex.Sample(texSampler, uv);

}
