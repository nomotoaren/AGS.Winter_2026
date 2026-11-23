#pragma once
#include <memory>
#include "Common/Transform.h"
#include "ActorBase.h"

class PixelMaterial;
class PixelRenderer;

// シェーダーで描く空(モデル・画像なし)
// 画面いっぱいの四角に、カメラの向きから空と雲を計算して描く
class SkyDome : public ActorBase
{

public:

	// 定数バッファの個数(FLOAT4)
	static constexpr int CONST_BUF_NUM = 5;

	// 雲の量(0.0～1.0。大きいほど雲が増える)
	static constexpr float CLOUD_COVER = 0.55f;

	// 雲の流れる速さ(シェーダー側の風と掛け合わせる倍率)
	static constexpr float CLOUD_SPEED = 1.0f;

	// 太陽の方向(太陽に向かう向き。ステージのライトと合わせると自然)
	static constexpr VECTOR SUN_DIR = { 0.45f, 0.65f, 0.35f };

	// コンストラクタ(従来と同じ引数。空は位置に依存しないので使いません)
	SkyDome(const Transform& syncTransform);

	// デストラクタ
	~SkyDome(void);

	void Init(void) override;
	void Update(void) override;
	void Draw(void) override;

private:

	// 自機の情報(従来との互換のため保持)
	const Transform& syncTransform_;

	// 経過時間(雲を流すための時計)
	float time_;

	// 空のシェーダー(rendererより先に宣言すること)
	std::unique_ptr<PixelMaterial> material_;
	std::unique_ptr<PixelRenderer> renderer_;

};