#include <cmath>
#include <cstdio>
#include <DxLib.h>
#include "PadInput.h"

bool PadInput::cur_[(int)PadInput::BTN::MAX] = {};
bool PadInput::old_[(int)PadInput::BTN::MAX] = {};
float PadInput::stickX_ = 0.0f;
float PadInput::stickY_ = 0.0f;
bool PadInput::connected_ = false;
int PadInput::l2HoldFrame_ = 0;
int PadInput::guardBlockFrame_ = 0;

namespace
{
	constexpr int PAD = DX_INPUT_PAD1;

	// スティックの遊び(これ未満は 0 扱い)
	constexpr float STICK_DEAD_ZONE = 0.25f;

	// L2 を押してからガードになるまでの待ち(フレーム)
	constexpr int GUARD_START_FRAME = 6;

	// R2 / L1 を離したあと、ガードを禁止する時間(フレーム)
	constexpr int GUARD_BLOCK_FRAME = 12;

	// L2/R2 を押したとみなすトリガーの深さ(0~255、Xbox のみ)
	constexpr int TRIGGER_ON = 100;

	// DirectInput(DualSense / DualShock4)のボタン番号
	// 反応するボタンが違うときは、ここの番号を入れ替える
	constexpr int DI_SQUARE = 0;
	constexpr int DI_CROSS = 1;
	constexpr int DI_CIRCLE = 2;
	constexpr int DI_TRIANGLE = 3;
	constexpr int DI_L1 = 4;
	constexpr int DI_R1 = 5;
	constexpr int DI_L2 = 6;
	constexpr int DI_R2 = 7;
	constexpr int DI_OPTIONS = 9;
	constexpr int DI_L3 = 10;
	constexpr int DI_R3 = 11;

	bool IsXbox(int type)
	{
		return type == DX_PADTYPE_XBOX_360 || type == DX_PADTYPE_XBOX_ONE;
	}
}

void PadInput::Update(void)
{
	for (int i = 0; i < (int)BTN::MAX; i++)
	{
		old_[i] = cur_[i];
		cur_[i] = false;
	}

	stickX_ = 0.0f;
	stickY_ = 0.0f;

	connected_ = GetJoypadNum() > 0;

	if (!connected_)
	{
		l2HoldFrame_ = 0;
		guardBlockFrame_ = 0;
		return;
	}

	auto set = [](BTN b, bool v) { cur_[(int)b] = v; };

	const int type = GetJoypadType(PAD);

	if (IsXbox(type))
	{
		XINPUT_STATE xs = {};
		GetJoypadXInputState(PAD, &xs);

		set(BTN::CROSS, xs.Buttons[XINPUT_BUTTON_A] != 0);
		set(BTN::CIRCLE, xs.Buttons[XINPUT_BUTTON_B] != 0);
		set(BTN::SQUARE, xs.Buttons[XINPUT_BUTTON_X] != 0);
		set(BTN::TRIANGLE, xs.Buttons[XINPUT_BUTTON_Y] != 0);
		set(BTN::L1, xs.Buttons[XINPUT_BUTTON_LEFT_SHOULDER] != 0);
		set(BTN::R1, xs.Buttons[XINPUT_BUTTON_RIGHT_SHOULDER] != 0);
		set(BTN::L2, xs.LeftTrigger >= TRIGGER_ON);
		set(BTN::R2, xs.RightTrigger >= TRIGGER_ON);
		set(BTN::L3, xs.Buttons[XINPUT_BUTTON_LEFT_THUMB] != 0);
		set(BTN::R3, xs.Buttons[XINPUT_BUTTON_RIGHT_THUMB] != 0);
		set(BTN::OPTIONS, xs.Buttons[XINPUT_BUTTON_START] != 0);
	}
	else
	{
		DINPUT_JOYSTATE ds = {};
		GetJoypadDirectInputState(PAD, &ds);

		set(BTN::CROSS, ds.Buttons[DI_CROSS] != 0);
		set(BTN::CIRCLE, ds.Buttons[DI_CIRCLE] != 0);
		set(BTN::SQUARE, ds.Buttons[DI_SQUARE] != 0);
		set(BTN::TRIANGLE, ds.Buttons[DI_TRIANGLE] != 0);
		set(BTN::L1, ds.Buttons[DI_L1] != 0);
		set(BTN::R1, ds.Buttons[DI_R1] != 0);
		set(BTN::L2, ds.Buttons[DI_L2] != 0);
		set(BTN::R2, ds.Buttons[DI_R2] != 0);
		set(BTN::L3, ds.Buttons[DI_L3] != 0);
		set(BTN::R3, ds.Buttons[DI_R3] != 0);
		set(BTN::OPTIONS, ds.Buttons[DI_OPTIONS] != 0);
	}

	// 左スティック(どのパッドでも -1000 ~ 1000)
	int ax = 0;
	int ay = 0;
	GetJoypadAnalogInput(&ax, &ay, PAD);

	float x = (float)ax / 1000.0f;
	float y = -(float)ay / 1000.0f;	// 上をプラスにする

	if (std::sqrt(x * x + y * y) < STICK_DEAD_ZONE)
	{
		x = 0.0f;
		y = 0.0f;
	}

	stickX_ = x;
	stickY_ = y;

	// ガード誤爆防止のカウント
	l2HoldFrame_ = cur_[(int)BTN::L2] ? l2HoldFrame_ + 1 : 0;

	if (cur_[(int)BTN::R2] || cur_[(int)BTN::L1])
	{
		guardBlockFrame_ = GUARD_BLOCK_FRAME;
	}
	else if (guardBlockFrame_ > 0)
	{
		guardBlockFrame_--;
	}
}

bool PadInput::IsConnected(void) { return connected_; }

bool PadInput::IsNew(BTN btn) { return cur_[(int)btn]; }

bool PadInput::IsTrg(BTN btn) { return cur_[(int)btn] && !old_[(int)btn]; }

float PadInput::StickX(void) { return stickX_; }
float PadInput::StickY(void) { return stickY_; }
bool PadInput::IsStickTilted(void) { return stickX_ != 0.0f || stickY_ != 0.0f; }

//------------------------------------------------------------
// 操作
//------------------------------------------------------------
bool PadInput::IsFormPalette(void)
{
	return IsNew(BTN::L2) && IsNew(BTN::R2);
}

bool PadInput::IsSpecialPalette(void)
{
	return IsNew(BTN::L1);
}

bool PadInput::IsGuard(void)
{
	// L2 + R2 はフォームチェンジパレットなのでガードにしない
	// (パレットを開閉する一瞬だけガードが入って動けなくなるのを防ぐため、少し待つ)
	return IsNew(BTN::L2) && !IsNew(BTN::R2) &&
		l2HoldFrame_ >= GUARD_START_FRAME &&
		guardBlockFrame_ <= 0;
}

bool PadInput::IsBurstTrg(void)
{
	return IsGuard() && IsTrg(BTN::CIRCLE);
}

bool PadInput::IsFightTrg(void)
{
	return IsTrg(BTN::SQUARE) && !IsSpecialPalette() && !IsFormPalette();
}

bool PadInput::IsKiBlastTrg(void)
{
	return IsTrg(BTN::CIRCLE) && !IsNew(BTN::L2) && !IsSpecialPalette();
}

bool PadInput::IsStepTrg(void)
{
	return IsTrg(BTN::CROSS);
}

bool PadInput::IsChargeTrg(void)
{
	return IsTrg(BTN::TRIANGLE);
}

bool PadInput::IsChargeHold(void)
{
	return IsNew(BTN::TRIANGLE);
}

bool PadInput::IsHighBoostTrg(void)
{
	return IsTrg(BTN::L3);
}

bool PadInput::IsVerticalMode(void)
{
	return IsNew(BTN::R2) && !IsNew(BTN::L2);
}

bool PadInput::IsKamehameTrg(void)
{
	return IsSpecialPalette() && IsTrg(BTN::SQUARE);
}

bool PadInput::IsTransformTrg(void)
{
	return IsFormPalette() && IsTrg(BTN::SQUARE);
}