#include "VertexInputType.hlsli"

#if defined(VERTEX_INPUT)

#if (VERTEX_INPUT == DX_MV1_VERTEX_TYPE_1FRAME)
//1フレームの影響を受ける頂点
struct VertexInput
{
    float3 pos : POSITION;          // 座標(ローカル空間)
    float3 norm : NORMAL;           // 法線(ローカル空間)
    float4 diffuse : COLOR0;        // ディフューズカラー
    float4 specular : COLOR1;       // スペキュラカラー
    float4 uv0 : TEXCOORD0;         // テクスチャ座標
    float4 uv1 : TEXCOORD1;         // サブテクスチャ座標
};

#elif (VERTEX_INPUT == DX_MV1_VERTEX_TYPE_4FRAME)
//1～4フレームの影響を受ける頂点
struct VertexInput
{
    float3 pos : POSITION;              // 座標(ローカル空間)
    float3 norm : NORMAL;               // 法線(ローカル空間)
    float4 diffuse : COLOR0;            // ディフューズカラー
    float4 specular : COLOR1;           // スペキュラカラー
    float4 uv0 : TEXCOORD0;             // テクスチャ座標
    float4 uv1 : TEXCOORD1;             // サブテクスチャ座標
    int4 blendIndices0 : BLENDINDICES0; // ボーン処理用 Float型定数配列インデックス0
    float4 blendWeight0 : BLENDWEIGHT0; // ボーン処理用ウエイト値0
};
#elif (VERTEX_INPUT == DX_MV1_VERTEX_TYPE_8FRAME)
//5～8フレームの影響を受ける頂点
struct VertexInput
{
    float3 pos : POSITION;              // 座標(ローカル空間)
    float3 norm : NORMAL;               // 法線(ローカル空間)
    float4 diffuse : COLOR0;            // ディフューズカラー
    float4 specular : COLOR1;           // スペキュラカラー
    float4 uv0 : TEXCOORD0;             // テクスチャ座標
    float4 uv1 : TEXCOORD1;             // サブテクスチャ座標
    int4 blendIndices0 : BLENDINDICES0; // ボーン処理用 Float型定数配列インデックス0
    float4 blendWeight0 : BLENDWEIGHT0; // ボーン処理用ウエイト値0
    int4 blendIndices1 : BLENDINDICES1; // ボーン処理用 Float型定数配列インデックス1
    float4 blendWeight1 : BLENDWEIGHT1; // ボーン処理用ウエイト値1
};
#elif (VERTEX_INPUT == DX_MV1_VERTEX_TYPE_NMAP_1FRAME)
//法線マップの情報が含まれる1フレームの影響を受ける頂点
struct VertexInput
{
    float3 pos : POSITION;              // 座標(ローカル空間)
    float3 norm : NORMAL;               // 法線(ローカル空間)
    float4 diffuse : COLOR0;            // ディフューズカラー
    float4 specular : COLOR1;           // スペキュラカラー
    float4 uv0 : TEXCOORD0;             // テクスチャ座標
    float4 uv1 : TEXCOORD1;             // サブテクスチャ座標
    float3 tan : TANGENT0;              // 接線(ローカル空間)
    float3 bin : BINORMAL0;             // 従法線(ローカル空間)
};
#elif (VERTEX_INPUT == DX_MV1_VERTEX_TYPE_NMAP_4FRAME)
//法線マップの情報が含まれる1～4フレームの影響を受ける頂点
struct VertexInput
{
    float3 pos : POSITION;              // 座標(ローカル空間)
    float3 norm : NORMAL;               // 法線(ローカル空間)
    float4 diffuse : COLOR0;            // ディフューズカラー
    float4 specular : COLOR1;           // スペキュラカラー
    float4 uv0 : TEXCOORD0;             // テクスチャ座標
    float4 uv1 : TEXCOORD1;             // サブテクスチャ座標
    float3 tan : TANGENT0;              // 接線(ローカル空間)
    float3 bin : BINORMAL0;             // 従法線(ローカル空間)
    int4 blendIndices0 : BLENDINDICES0; // ボーン処理用 Float型定数配列インデックス0
    float4 blendWeight0 : BLENDWEIGHT0; // ボーン処理用ウエイト値0
};
#elif (VERTEX_INPUT == DX_MV1_VERTEX_TYPE_NMAP_8FRAME)
//法線マップの情報が含まれる5～8フレームの影響を受ける頂点
struct VertexInput
{
    float3 pos : POSITION;              // 座標(ローカル空間)
    float3 norm : NORMAL;               // 法線(ローカル空間)
    float4 diffuse : COLOR0;            // ディフューズカラー
    float4 specular : COLOR1;           // スペキュラカラー
    float4 uv0 : TEXCOORD0;             // テクスチャ座標
    float4 uv1 : TEXCOORD1;             // サブテクスチャ座標
    float3 tan : TANGENT0;              // 接線(ローカル空間)
    float3 bin : BINORMAL0;             // 従法線(ローカル空間)
    int4 blendIndices0 : BLENDINDICES0; // ボーン処理用 Float型定数配列インデックス0
    float4 blendWeight0 : BLENDWEIGHT0; // ボーン処理用ウエイト値0
    int4 blendIndices1 : BLENDINDICES1; // ボーン処理用 Float型定数配列インデックス1
    float4 blendWeight1 : BLENDWEIGHT1; // ボーン処理用ウエイト値1
};
#elif (VERTEX_INPUT == DX_VERTEX3DSHADER)
//DrawPolygon3DToShader系
struct VertexInput
{
    float3 pos        : POSITION0;		// 座標( ローカル空間 )
    float4 SubPosition     : POSITION1;		// 補助座標
    float3 Normal          : NORMAL;		// 法線
    float3 Tangent         : TANGENT;		// 接線
    float3 Binormal        : BINORMAL;		// 従法線
    float4 diffuse         : COLOR0;		// ディフューズカラー
    float4 Specular        : COLOR1;		// スペキュラカラー
    float2 TexCoords0      : TEXCOORD0;		// テクスチャ座標０
    float2 TexCoords1      : TEXCOORD1;		// テクスチャ座標１
};
#endif

#endif