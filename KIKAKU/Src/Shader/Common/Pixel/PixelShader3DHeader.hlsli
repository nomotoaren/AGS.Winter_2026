#include "../Vertex/CommonShader3DHeader.hlsli"

#if !defined(PS_INPUT)
#define PS_INPUT VertexToPixel
#endif

#if !defined(PS_OUTPUT)
#define PS_OUTPUT float4
#endif

#define DX_D3D11_PS_CONST_FILTER_SIZE	(1280)	    // フィルター用定数バッファのサイズ

// シャドウマップパラメータ
struct ShadowMap
{
    float adjustDepth;      // 閾値深度補正値
    float gradationParam;   // グラデーション範囲
    float enable_Light0;    // ライト０への適用情報
    float enable_Light1;    // ライト１への適用情報

    float enable_Light2;    // ライト２への適用情報
    float3 padding;         // パディング
};

// 定数バッファピクセルシェーダー基本パラメータ
struct PsBase
{
    float4 factorColor;             // アルファ値等

    float mulAlphaColor;            // カラーにアルファ値を乗算するかどうか( 0.0f:乗算しない  1.0f:乗算する )
    float alphaTestRef;             // アルファテストで使用する比較値
    float2 padding1;

    int alphaTestCmpMode;           // アルファテスト比較モード( DX_CMP_NEVER など )
    int noLightAngleAttenuation;    // ライトの角度減衰を行わないか( 0:減衰を行う   1:減衰を行わない )
    int2 padding2;

    float4 ignoreTextureColor;      // テクスチャカラー無視処理用カラー

    float4 drawAddColor;            // 加算する色
};

// 定数バッファシャドウマップパラメータ
struct PsShadowMap
{
    ShadowMap data[3];
};

// 頂点シェーダー・ピクセルシェーダー共通パラメータ
cbuffer cbD3D11_CONST_BUFFER_COMMON : register(b0)
{
    Common g_common;
};
// 基本パラメータ
cbuffer cbD3D11_CONST_BUFFER_PS_BASE : register(b1)
{
    PsBase g_base;
};
// シャドウマップパラメータ
cbuffer cbD3D11_CONST_BUFFER_PS_SHADOWMAP : register(b2)
{
    PsShadowMap g_shadowMap;
};

SamplerState diffuseMapSampler            : register(s0);           // ディフューズマップテクスチャ
Texture2D diffuseMapTexture               : register(t0);           // ディフューズマップテクスチャ

#if BUMPMAP
SamplerState normalMapSampler             : register(s1);    		// 法線マップテクスチャ
Texture2D    normalMapTexture             : register(t1);	    	// 法線マップテクスチャ
#endif // BUMPMAP

#if USE_SPETEX
SamplerState specularMapSampler           : register(s2);    		// スペキュラマップテクスチャ
Texture2D    specularMapTexture           : register(t2);	    	// スペキュラマップテクスチャ
#endif // USE_SPETEX

#if TOON
SamplerState toonDiffuseGradSampler       : register(s3);   		// トゥーンレンダリング用ディフューズカラーグラデーションテクスチャ
Texture2D    toonDiffuseGradTexture       : register(t3); 	    	// トゥーンレンダリング用ディフューズカラーグラデーションテクスチャ

SamplerState toonSpecularGradSampler      : register(s4);	    	// トゥーンレンダリング用スペキュラカラーグラデーションテクスチャ
Texture2D    toonSpecularGradTexture      : register(t4);   		// トゥーンレンダリング用スペキュラカラーグラデーションテクスチャ

#if TOON_SPHEREOP_MUL || TOON_SPHEREOP_ADD
SamplerState toonSphereMapSampler         : register(s5);     		// トゥーンレンダリング用スフィアマップテクスチャ
Texture2D    toonSphereMapTexture         : register(t5);	    	// トゥーンレンダリング用スフィアマップテクスチャ
#endif // TOON_SPHEREOP_MUL || TOON_SPHEREOP_ADD

SamplerState toonRGBtoVMaxRGBVolumeSampler: register(s6);	    	// トゥーンレンダリング用RGB輝度飽和処理用ボリュームテクスチャ
Texture3D    toonRGBtoVMaxRGBVolumeTexture: register(t6);		    // トゥーンレンダリング用RGB輝度飽和処理用ボリュームテクスチャ
#endif // TOON

#if SUBTEXTUREMODE
SamplerState subSampler                   : register(s7);           // サブテクスチャ
Texture2D subTexture                      : register(t7);           // サブテクスチャ
#endif // SUBTEXTUREMODE != 0

#if SHADOWMAP
SamplerState shadowMap0Sampler            : register(s8);	    	// シャドウマップ０テクスチャ
Texture2D    shadowMap0Texture            : register(t8);		    // シャドウマップ０テクスチャ

SamplerState shadowMap1Sampler            : register(s9);   		// シャドウマップ１テクスチャ
Texture2D    shadowMap1Texture            : register(t9);	    	// シャドウマップ１テクスチャ

SamplerState shadowMap2Sampler            : register(s10); 	    	// シャドウマップ２テクスチャ
Texture2D    shadowMap2Texture            : register(t10);	    	// シャドウマップ２テクスチャ
#endif
