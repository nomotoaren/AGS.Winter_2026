// VS/PS共通
#include "../Common/VertexToPixelHeader.hlsli"

// IN
#include "../Common/Vertex/VertexInputType.hlsli"
#define VERTEX_INPUT DX_MV1_VERTEX_TYPE_NMAP_1FRAME

// OUT
#define VS_OUTPUT VertexToPixelLit
#include "../Common/Vertex/VertexShader3DHeader.hlsli"

// 波のパラメータ用定数バッファ
cbuffer WaveParams : register(b4)
{
    float4 g_waveParams; // x: 総時間, y: 波の振幅, z: 波の周波数, w: 波の速度
};

VS_OUTPUT main(VS_INPUT VSInput)
{
	VS_OUTPUT ret;

	// 頂点座標変換 +++++++++++++++++++++++++++++++++++++( 開始 )
    float4 lLocalPosition;
    float4 lWorldPosition;
    float4 lViewPosition;
    float4 g_uv_scale = float4(5.0f, 5.0f, 5.0f, 5.0f);

	// float3 → float4
    lLocalPosition.xyz = VSInput.pos;
    lLocalPosition.w = 1.0f;

	// ローカル座標をワールド座標に変換(剛体)
    lWorldPosition.w = 1.0f;
    lWorldPosition.xyz = mul(lLocalPosition, g_base.localWorldMatrix);
    
    // 正弦波による頂点移動 ++++++++++++++++++++++++++++( 開始 )
    float totalTime = g_waveParams.x; // 総時間
    float amplitude = g_waveParams.y; // 波の振幅
    float frequency = g_waveParams.z; // 波の周波数
    float speed = g_waveParams.w; // 波の速度
    
    // ワールド座標のX位置に応じて、正弦波でY座標を変動させる
    float wave = sin(lWorldPosition.x * frequency + totalTime * speed);
    lWorldPosition.y += wave * amplitude;
    // 正弦波による頂点移動 ++++++++++++++++++++++++++++( 終了 )
    
    ret.worldPos.xyz = lWorldPosition.xyz;

	// ワールド座標をビュー座標に変換
    lViewPosition.w = 1.0f;
    lViewPosition.xyz = mul(lWorldPosition, g_base.viewMatrix);
    ret.vwPos.xyz = lViewPosition.xyz;
    
    // ビュー座標を射影座標に変換
    ret.svPos = mul(lViewPosition, g_base.projectionMatrix);
	
	// 頂点座標変換 +++++++++++++++++++++++++++++++++++++( 終了 )
	// その他、ピクセルシェーダへ引継&初期化 ++++++++++++( 開始 )
	// UV座標（ここをまとめて代入）
    ret.uv.x = VSInput.uv0.x * g_uv_scale.x;
    ret.uv.y = VSInput.uv0.y * g_uv_scale.y;
	// 法線をローカル空間からワールド空間へ変換
    ret.normal = normalize(
		mul(VSInput.norm, (float3x3) g_base.localWorldMatrix));
	// ディフューズカラー
    ret.diffuse = VSInput.diffuse;
	// ライト方向(ローカル)
    ret.lightDir = float3(0.0f, 0.0f, 0.0f);
	// ライトから見た座標
    ret.lightAtPos = float3(0.0f, 0.0f, 0.0f);
	// その他、ピクセルシェーダへ引継&初期化 ++++++++++++( 終了 )
    
	// 出力パラメータを返す
    return ret;
}