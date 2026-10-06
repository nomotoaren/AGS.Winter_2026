#pragma once
#include <string>

// 敵のHPバー(画面右上、ドラゴンボール カカロット風)
// 使い方:
//   Init() を1回、毎フレーム Update(hp)、描画の最後に Draw()
class EnemyHud
{
public:

	EnemyHud(void);
	~EnemyHud(void);

	void Init(void);
	void Update(int hp);
	void Draw(void);

	// 名前プレートの文字
	void SetName(const std::string& name);

	// アイコンに出す文字
	void SetIconText(const std::string& text);

	// 最大HP(指定しなければ、最初に渡されたHPを最大値とする)
	void SetMaxHp(int maxHp);

private:

	int fontName_;
	int fontIcon_;
	int fontNum_;

	std::string name_;
	std::string iconText_;

	int hp_;
	int maxHp_;

	float delayedHp_;	// 遅れて減る白いバーのHP
	float delayWait_;	// 減り始めるまでの待ち時間
	float flashTimer_;	// 被弾フラッシュ
	float time_;		// ピンチ点滅用
	bool initialized_;
};