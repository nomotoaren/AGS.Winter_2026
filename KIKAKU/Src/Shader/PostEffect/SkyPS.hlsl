#include "../Common/Pixel/PixelShader2DHeader.hlsli"

// 定数バッファ：スロット4番目(b4)
cbuffer cbParam : register(b4)
{
	float4 g_camRight;	// xyz : カメラの右方向
	float4 g_camUp;		// xyz : カメラの上方向
	float4 g_camFwd;	// xyz : カメラの前方向
	float4 g_param;		// x : 時間, y : tan(縦FOV/2), z : アスペクト比, w : 雲の量(0.0～1.0)
	float4 g_sunDir;	// xyz : 太陽の方向(太陽に向かう向き)
    float4 g_screen; // xy : 画面の幅と高さ(ピクセル)
}

// ノイズ
float Hash21(float2 p)
{
	p = frac(p * float2(123.34f, 456.21f));
	p += dot(p, p + 45.32f);
	return frac(p.x * p.y);
}

float ValueNoise(float2 p)
{
	float2 i = floor(p);
	float2 f = frac(p);
	float2 u = f * f * (3.0f - 2.0f * f);

	float a = Hash21(i);
	float b = Hash21(i + float2(1.0f, 0.0f));
	float c = Hash21(i + float2(0.0f, 1.0f));
	float d = Hash21(i + float2(1.0f, 1.0f));

	return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// 重ねて、もくもくした形にする
float Fbm(float2 p)
{
	float v = 0.0f;
	float a = 0.5f;
	for (int i = 0; i < 5; i++)
	{
		v += a * ValueNoise(p);
		p = p * 2.03f + float2(17.0f, 9.0f);
		a *= 0.5f;
	}
	return v;
}

// 雲の濃さ
float CloudDensity(float2 p, float cover)
{
	float n = Fbm(p);
	// coverが大きいほど、雲が増える
	return saturate((n - (1.0f - cover)) / 0.35f);
}

float4 main(PS_INPUT PSInput) : SV_TARGET
{
	float time = g_param.x;
	float cover = g_param.w;

	// 画面の位置から、カメラが見ている方向を作る
    float2 uv = PSInput.svPos.xy / g_screen.xy;
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
	float3 dir = normalize(
		g_camFwd.xyz
		+ g_camRight.xyz * (ndc.x * g_param.y * g_param.z)
		+ g_camUp.xyz * (ndc.y * g_param.y));

	float3 sunDir = normalize(g_sunDir.xyz);
	float h = dir.y;

	// 空のグラデーション
	float3 horizonCol = float3(0.78f, 0.88f, 0.98f);
	float3 zenithCol = float3(0.16f, 0.42f, 0.86f);
	float t = pow(saturate(h), 0.55f);
	float3 sky = lerp(horizonCol, zenithCol, t);

	// 太陽
	float sunDot = saturate(dot(dir, sunDir));
	sky += float3(1.0f, 0.85f, 0.6f) * pow(sunDot, 8.0f) * 0.25f;		// 周りの光
	sky += float3(1.0f, 0.95f, 0.8f) * pow(sunDot, 400.0f) * 1.5f;	// 太陽本体

	// 雲
	if (h > 0.0f)
	{
		// 地平線に近いほど、遠くの雲が細かく詰まって見える
		float2 p = dir.xz / (h + 0.15f);
		float2 wind = float2(time * 0.020f, time * 0.008f);

		float2 cp = p * 0.55f + wind;
		float d = CloudDensity(cp, cover);

		// 太陽側に少しずらして取り、差分で明暗をつける
		float2 sunOff = normalize(sunDir.xz + 0.0001f) * 0.12f;
		float d2 = CloudDensity(cp + sunOff, cover);
		float light = saturate(1.0f - (d2 - d) * 1.5f);

		float3 cloudLit = float3(1.0f, 1.0f, 1.0f);
		float3 cloudShade = float3(0.62f, 0.68f, 0.78f);
		float3 cloudCol = lerp(cloudShade, cloudLit, light * 0.7f + 0.3f);

		// 地平線付近は、もやに溶けるように薄くする
		float fade = smoothstep(0.0f, 0.18f, h);
		sky = lerp(sky, cloudCol, d * fade * 0.95f);
	}

	// 地平線のもや / 地平線より下
	float haze = 1.0f - smoothstep(0.0f, 0.22f, abs(h));
	sky = lerp(sky, horizonCol, haze * 0.5f);
	if (h < 0.0f)
	{
		sky = lerp(horizonCol, float3(0.55f, 0.62f, 0.70f), saturate(-h * 4.0f));
	}

	return float4(sky, 1.0f);
}