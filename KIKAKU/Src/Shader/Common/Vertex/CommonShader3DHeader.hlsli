#define DX_D3D11_COMMON_CONST_LIGHT_NUM (6)         // 共通パラメータのライトの最大数

#define DX_VERTEXLIGHTING_LIGHT_NUM		(3)		    // 頂点単位ライティングで同時に使用できるライトの最大数
#define DX_PIXELLIGHTING_LIGHT_NUM		(6)		    // ピクセル単位ライティングで同時に使用できるライトの最大数

#define DX_LIGHTTYPE_POINT				(1)			// ポイントライト
#define DX_LIGHTTYPE_SPOT				(2)			// スポットライト
#define DX_LIGHTTYPE_DIRECTIONAL		(3)			// ディレクショナルライト

// マテリアルパラメータ
struct Material
{
    float4 diffuse;             // ディフューズカラー
    float4 specular;            // スペキュラカラー
    float4 ambientEmissive;     // マテリアルエミッシブカラー + マテリアルアンビエントカラー * グローバルアンビエントカラー
	
    float power;                // スペキュラの強さ
    float typeParam0;           // マテリアルタイプパラメータ0
    float typeParam1;           // マテリアルタイプパラメータ1
    float typeParam2;           // マテリアルタイプパラメータ2
};

// フォグパラメータ
struct VsFog
{
    float linearAdd;            // フォグ用パラメータ end / ( end - start )
    float linearDiv;            // フォグ用パラメータ -1  / ( end - start )
    float density;              // フォグ用パラメータ density
    float e;                    // フォグ用パラメータ 自然対数の底

    float4 color;               // カラー
};

// ライトパラメータ
struct Light
{
    int type;               // ライトタイプ( DX_LIGHTTYPE_POINT など )
    int3 padding1;          // パディング１
	
    float3 position;        // 座標( ビュー空間 )
    float rangePow2;        // 有効距離の２乗
	
    float3 direction;       // 方向( ビュー空間 )
    float fallOff;          // スポットライト用FallOff
	
    float3 diffuse;         // ディフューズカラー
    float spotParam0;       // スポットライト用パラメータ０( cos( Phi / 2.0f ) )
	
    float3 specular;        // スペキュラカラー
    float spotParam1;       // スポットライト用パラメータ１( 1.0f / ( cos( Theta / 2.0f ) - cos( Phi / 2.0f ) ) )
	
    float4 ambient;         // アンビエントカラーとマテリアルのアンビエントカラーを乗算したもの
	
    float attenuation0;     // 距離による減衰処理用パラメータ０
    float attenuation1;     // 距離による減衰処理用パラメータ１
    float attenuation2;     // 距離による減衰処理用パラメータ２
    float padding2;         // パディング２
};

// ピクセルシェーダー・頂点シェーダー共通パラメータ
struct Common
{
    Light light[DX_D3D11_COMMON_CONST_LIGHT_NUM];  // ライトパラメータ
    Material material;                             // マテリアルパラメータ
    VsFog fog;                                     // フォグパラメータ
};
