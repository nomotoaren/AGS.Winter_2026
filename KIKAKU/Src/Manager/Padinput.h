#pragma once

// ゲームパッド入力(DualSense / DualShock4 / Xbox 対応、パッド1のみ)
// 割り当てはドラゴンボール カカロットの「バトル中の基本操作」に合わせる
// 使い方:
//   毎フレーム1回だけ PadInput::Update() を呼ぶ(Player::Update の先頭で呼んでいる)
//   あとは下の「操作」関数で判定する(パッドが無いときはすべて false / 0)
class PadInput
{
public:

	// ボタン(PS表記。Xbox では ×=A ○=B □=X △=Y)
	enum class BTN
	{
		CROSS,		// ×
		CIRCLE,		// ○
		SQUARE,		// □
		TRIANGLE,	// △
		L1,
		R1,
		L2,
		R2,
		L3,
		R3,
		OPTIONS,
		MAX
	};

	static void Update(void);

	static bool IsConnected(void);

	static bool IsNew(BTN btn);		// 押している間
	static bool IsTrg(BTN btn);		// 押した瞬間

	// 左スティック(-1.0 ~ 1.0)。Y は上(前)がプラス
	static float StickX(void);
	static float StickY(void);
	static bool IsStickTilted(void);

	// ---- 操作(カカロットの割り当て) ----
	static bool IsGuard(void);			// L2
	static bool IsBurstTrg(void);		// L2 + ○
	static bool IsFightTrg(void);		// □ 格闘攻撃
	static bool IsKiBlastTrg(void);		// ○ 気弾攻撃
	static bool IsStepTrg(void);		// × ステップ
	static bool IsChargeTrg(void);		// △ 気力溜め(押した瞬間)
	static bool IsChargeHold(void);		// △ 長押し
	static bool IsHighBoostTrg(void);	// L3 ハイブースト
	static bool IsVerticalMode(void);	// 左スティック + R2 で上下移動

	// パレット
	static bool IsSpecialPalette(void);	// L1 長押し(必殺技パレット)
	static bool IsFormPalette(void);	// L2 + R2 長押し(フォームチェンジパレット)
	static bool IsKamehameTrg(void);	// 必殺技パレット中の □
	static bool IsTransformTrg(void);	// フォームチェンジパレット中の □

private:

	static bool cur_[(int)BTN::MAX];
	static bool old_[(int)BTN::MAX];
	static float stickX_;
	static float stickY_;
	static bool connected_;

	// ガードの誤爆防止(L2+R2 のパレットを開閉するときに、L2 だけの瞬間でガードが入らないようにする)
	static int l2HoldFrame_;		// L2 を押し続けているフレーム数
	static int guardBlockFrame_;	// R2 / L1 を離してからガードを禁止する残りフレーム数
};