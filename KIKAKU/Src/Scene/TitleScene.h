#pragma once
#include <memory>
#include "SceneBase.h"
#include "../Object/Common/Transform.h"
class SceneManager;
class SkyDome;
class AnimationController;
class MenuUI;

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

	// タイトルメニュー(ゲームスタート / ゲーム終了)
	std::unique_ptr<MenuUI> menu_;

	// 決定済みか(フェード中に何度も決定しないため)
	bool isDecided_;

	// 遊んだ形跡があるか(あれば「つづきから / はじめから」を出す)
	bool hasProgress_;

	// 「はじめから」の確認中か
	bool isConfirming_;

	// メインのメニューを開く(cursor: 最初にカーソルを置く項目)
	void OpenMainMenu(int cursor);

};