#pragma once
#include <string>
#include "SceneBase.h"
#include "../Manager/GameProgress.h"

// ステージ選択画面(画像で飾る。レベルや名前などの文字は、画像の上にコードで描く)
//   操作: ↑↓ または W/S / パッドのスティック上下 で選択
//         ENTER・SPACE / パッド○ で決定
//         ESC・BACKSPACE / パッド× でタイトルへ戻る
//   画像は ResourceManager から取る(SRC::STAGE_BG など。シーンを抜けるときの解放も ResourceManager が行う)
class StageSelectScene : public SceneBase
{

public:

	StageSelectScene(void);
	~StageSelectScene(void);

	void Init(void) override;
	void Update(void) override;
	void Draw(void) override;

private:

	// 開いた直後は決定を受け付けない時間(クリア画面で押したボタンで、すぐ決定しないため)
	static constexpr float INPUT_GUARD_TIME = 0.3f;

	// スティックを「倒した」と判定する値
	static constexpr float STICK_THRESHOLD = 0.6f;

	// 項目の数(ステージ + 「タイトルへ戻る」)
	static constexpr int ITEM_COUNT = GameProgress::STAGE_MAX + 1;

	// 入力
	int ReadMoveInput(void);
	bool IsDecidePressed(void) const;
	bool IsCancelPressed(void) const;

	// 決定
	void Decide(int index);

	// 画像
	void LoadImages(void);

	// 描画の部品(座標は、画像と同じ 1920x1080 の画面での値で指定する)
	void DrawImageAt(int handle, float x, float y) const;
	void DrawLabel(float x, float y, const std::string& text, unsigned int color, int font, int align) const;
	void DrawStageCard(int index) const;
	void DrawBackCard(void) const;
	void DrawStatusPanel(void) const;
	void DrawDetailPanel(void) const;
	void DrawHint(void) const;

	// 画面の大きさに合わせた倍率(1920x1080 を 1.0 とする)
	float scaleX_;
	float scaleY_;

	int cursor_;
	float guardTimer_;
	float animTimer_;
	int prevStickDir_;
	bool isDecided_;

	// 選んでいるカードが、少し右へずれる量(なめらかに動かす)
	float cardOffset_[ITEM_COUNT];

	// 画像
	int imgBg_;
	int imgCardNormal_;
	int imgCardSelected_;
	int imgCardLocked_;
	int imgButtonNormal_;
	int imgButtonSelected_;
	int imgPanelStatus_;
	int imgPanelDetail_;

	// フォント
	int fontLevel_;
	int fontBig_;
	int fontMid_;
	int fontSmall_;

};