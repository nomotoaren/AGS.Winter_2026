#pragma once
#include "ResultSceneBase.h"

// ゲームオーバー画面(プレイヤーのHPが0になったとき)
//   メニュー: リトライ / タイトルへ戻る / ゲーム終了
class GameOverScene : public ResultSceneBase
{

public:

	GameOverScene(void);
	~GameOverScene(void) override;

protected:

	void DrawBackground(void) const override;
	std::string GetHeadingText(void) const override;
	unsigned int GetHeadingColor(void) const override;
	std::vector<std::string> GetMenuItems(void) const override;
	std::string GetCaption(void) const override;
	bool OnSelect(int index) override;

};