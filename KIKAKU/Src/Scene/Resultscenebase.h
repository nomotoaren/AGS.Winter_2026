#pragma once
#include <memory>
#include <string>
#include <vector>
#include "SceneBase.h"
class MenuUI;

// ゲームオーバー / ゲームクリアなど、「結果を見せて、メニューで次を選ぶ」シーンの共通部分
//   ・見出し(GAME OVER など)をゆっくり出す
//   ・少し待ってから、メニュー(リトライ / タイトルへ…)を出す
//   ・決定された項目の処理は、継承先(OnSelect)で書く
class ResultSceneBase : public SceneBase
{

public:

	ResultSceneBase(void);
	~ResultSceneBase(void) override;

	void Init(void) override;
	void Update(void) override;
	void Draw(void) override;

protected:

	// --- 継承先で決めるもの ---

	// 背景を描く(2D)
	virtual void DrawBackground(void) const = 0;

	// 見出しの文字と色
	virtual std::string GetHeadingText(void) const = 0;
	virtual unsigned int GetHeadingColor(void) const = 0;

	// メニューの項目
	virtual std::vector<std::string> GetMenuItems(void) const = 0;

	// メニュー上の説明文(なければ空文字)
	virtual std::string GetCaption(void) const;

	// 項目が決定されたときの処理
	//   戻り値: シーンの切り替えを始めたら true(以後の入力を止める)
	virtual bool OnSelect(int index) = 0;

	// シーンが始まってからの経過時間(秒)
	float GetTime(void) const { return timer_; }

private:

	// メニューを出すまでの時間(秒)
	static constexpr float MENU_OPEN_TIME = 1.2f;

	// 見出しが出きるまでの時間(秒)
	static constexpr float HEADING_FADE_TIME = 0.8f;

	// 見出し・メニューの位置(画面の高さに対する割合)
	static constexpr float HEADING_Y_RATE = 0.28f;
	static constexpr float MENU_Y_RATE = 0.55f;

	std::unique_ptr<MenuUI> menu_;

	float timer_;

	// 決定済みか(フェード中に何度も決定しないため)
	bool isDecided_;

};