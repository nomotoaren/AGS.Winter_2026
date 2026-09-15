// VS/PS共通
#include "../Common/VertexToPixelHeader.hlsli"

// IN
#include "../Common/Vertex/VertexInputType.hlsli"
#define VERTEX_INPUT DX_MV1_VERTEX_TYPE_NMAP_1FRAME

// OUT
#define VS_OUTPUT VertexToPixelLit
#include "../Common/Vertex/VertexShader3DHeader.hlsli"


VS_OUTPUT main(VS_INPUT VSInput)
{
    VS_OUTPUT ret;

    // 頂点座標変換
    float4 localPosition;
    float4 worldPosition;
    float4 viewPosition;

    // ローカル座標
    localPosition.xyz = VSInput.pos;
    localPosition.w = 1.0f;

    // ローカル → ワールド
    worldPosition.w = 1.0f;

    worldPosition.xyz =
        mul(
            localPosition,
            g_base.localWorldMatrix
        );

    // PS側で使うワールド座標
    ret.worldPos =
        worldPosition.xyz;

    // ワールド → ビュー
    viewPosition.w = 1.0f;

    viewPosition.xyz =
        mul(
            worldPosition,
            g_base.viewMatrix
        );

    ret.vwPos.xyz =
        viewPosition.xyz;

    // ビュー → 射影
    ret.svPos =
        mul(
            viewPosition,
            g_base.projectionMatrix
        );

    // UV
    ret.uv =
        VSInput.uv0;

    // 法線
    ret.normal =
        normalize(
            mul(
                VSInput.norm,
                (float3x3)g_base.localWorldMatrix
            )
        );

    // 頂点カラー
    ret.diffuse =
        VSInput.diffuse;

    // 初期値
    ret.lightDir =
        float3(
            0.0f,
            0.0f,
            0.0f
        );

    ret.lightAtPos =
        float3(
            0.0f,
            0.0f,
            0.0f
        );


    return ret;
}