#pragma once
#include <string>
#include <vector>

// 縦に並んだ選択メニュー(ポーズ / ゲームオーバー / タイトルで共通で使う)
//   操作: ↑↓ または W/S / パッドのスティック上下 で選択
//         ENTER・SPACE / パッド○ で決定
//         ESC・BACKSPACE / パッド× で戻る
//   ※パッドを使うので、呼ぶ側で PadInput::Update() を毎フレーム1回呼んでおくこと
class MenuUI
{

public:

	// Update の戻り値(0以上のときは「決定された項目の番号」)
	static constexpr int RESULT_NONE = -1;		// 何も起きていない
	static constexpr int RESULT_CANCEL = -2;	// 戻る操作が行われた

	MenuUI(void);
	~MenuUI(void);

	// メニューを開く(items: 項目の文字、defaultIndex: 最初にカーソルがある場所)
	void Open(const std::vector<std::string>& items, int defaultIndex = 0);

	// メニューを閉じる
	void Close(void);

	bool IsOpen(void) const { return isOpen_; }

	// 項目の上に出す説明文(「本当に終了しますか?」など)。空文字で消える
	void SetCaption(const std::string& caption) { caption_ = caption; }

	// 更新(決定された項目の番号 / RESULT_NONE / RESULT_CANCEL を返す)
	int Update(float dt);

	// 項目を描画する(topY: 最初の項目のY座標)
	void Draw(int topY) const;

	// 大きな見出し文字を中央に描く(PAUSE / GAME OVER など)
	void DrawHeading(const std::string& text, int centerY, unsigned int color, int alpha) const;

	// 画面全体を暗くする
	static void DrawDim(int alpha, unsigned int color);

private:

	// 開いた直後は決定を受け付けない時間(連打で誤って決定しないため)
	static constexpr float INPUT_GUARD_TIME = 0.3f;

	// スティックを「倒した」と判定する値
	static constexpr float STICK_THRESHOLD = 0.6f;

	// 項目の縦間隔
	static constexpr int ITEM_INTERVAL = 64;

	// フォントを必要になった時に1回だけ作る
	void CreateFonts(void) const;

	bool isOpen_;
	std::vector<std::string> items_;
	int cursor_;
	float guardTimer_;
	float animTimer_;
	std::string caption_;

	// スティックの前フレームの状態(-1:下 0:中立 1:上)
	int prevStickDir_;

	// フォント
	mutable int fontHeading_;
	mutable int fontItem_;
	mutable int fontCaption_;

};