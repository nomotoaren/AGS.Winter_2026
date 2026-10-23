#pragma once
#include "ResultSceneBase.h"

// ゲームクリア画面(敵を全部倒したとき)
//   クリアタイムを表示する
//   メニュー: もう一度遊ぶ / タイトルへ戻る / ゲーム終了
class GameClearScene : public ResultSceneBase
{

public:

	GameClearScene(void);
	~GameClearScene(void) override;

protected:

	void DrawBackground(void) const override;
	std::string GetHeadingText(void) const override;
	unsigned int GetHeadingColor(void) const override;
	std::vector<std::string> GetMenuItems(void) const override;
	std::string GetCaption(void) const override;
	bool OnSelect(int index) override;

};