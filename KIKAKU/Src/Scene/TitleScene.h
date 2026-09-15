#pragma once
#include <memory>
#include "SceneBase.h"
#include "../Object/Common/Transform.h"
class SceneManager;
class SkyDome;
class AnimationController;

class TitleScene : public SceneBase
{

public:

	// コンストラクタ
	TitleScene(void);

	// デストラクタ
	~TitleScene(void);

	void Init(void) override;
	void Update(void) override;
	void Draw(void) override;

private:

	// 画像
	int imgTitle_;
	int imgPush_;

	// スカイドーム用
	Transform spaceDomeTran_;

	// スカイドーム(背景)
	std::unique_ptr<SkyDome> skyDome_;

	// 惑星
	Transform planet_;
	Transform movePlanet_;

	// キャラクター
	Transform charactor_;

	// アニメーション
	std::unique_ptr<AnimationController> animationController_;

};
