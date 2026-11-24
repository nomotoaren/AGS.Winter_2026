#pragma once
#include <memory>
#include "Common/Transform.h"
#include "ActorBase.h"

class PixelMaterial;
class PixelRenderer;

// シェーダーで描く空
class SkyDome : public ActorBase
{

public:

	// 定数バッファの個数(FLOAT4)
	static constexpr int CONST_BUF_NUM = 6;

	// 雲の量
	static constexpr float CLOUD_COVER = 0.55f;

	// 雲の流れる速さ
	static constexpr float CLOUD_SPEED = 1.0f;

	// 太陽の方向
	static constexpr VECTOR SUN_DIR = { 0.45f, 0.65f, 0.35f };

	// コンストラクタ
	SkyDome(const Transform& syncTransform);

	// デストラクタ
	~SkyDome(void);

	void Init(void) override;
	void Update(void) override;
	void Draw(void) override;

private:

	// 自機の情報
	const Transform& syncTransform_;

	// 経過時間
	float time_;

	// 空のシェーダー
	std::unique_ptr<PixelMaterial> material_;
	std::unique_ptr<PixelRenderer> renderer_;

};