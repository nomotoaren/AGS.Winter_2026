#pragma once

// 画面の暗転(フェードアウト)・明転(フェードイン)
// 使い方: SetFade で開始 → 毎フレーム Update / Draw → IsEnd で終わりを調べる
class Fader
{

public:

	// フェードが進む速さ(1フレームあたりの透明度の変化量)
	static constexpr float SPEED_ALPHA = 5.0f;

	// 状態
	enum class STATE
	{
		NONE,		// フェードしていない
		FADE_OUT,	// だんだん暗くなる
		FADE_IN		// だんだん明るくなる
	};

	// 状態の取得
	STATE GetState(void) const;

	// フェードが完全に終了しているか
	bool IsEnd(void) const;

	// 指定のフェードを開始する
	void SetFade(STATE state);

	void Init(void);
	void Update(void);
	void Draw(void) const;

private:

	// 透明度の最大値(真っ暗)
	static constexpr float ALPHA_MAX = 255.0f;

	// 状態
	STATE state_ = STATE::NONE;

	// 透明度(0:透明 〜 255:真っ黒)
	float alpha_ = 0.0f;

	// 状態(STATE)を保ったまま終了判定を行うため、
	// Update->Draw->Update の1フレーム分を作る用
	bool isPreEnd_ = true;

	// フェード処理の終了判定
	bool isEnd_ = true;

	// 透明度が端まで進んだときの処理
	void OnReachEnd(void);

};