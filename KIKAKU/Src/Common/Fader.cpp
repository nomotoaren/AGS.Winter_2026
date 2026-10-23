#include <DxLib.h>
#include "../Application.h"
#include "Fader.h"

Fader::STATE Fader::GetState(void) const
{
	return state_;
}

bool Fader::IsEnd(void) const
{
	return isEnd_;
}

void Fader::SetFade(STATE state)
{
	state_ = state;

	// フェードを始めるときは、終了判定をリセットする
	if (state_ != STATE::NONE)
	{
		isPreEnd_ = false;
		isEnd_ = false;
	}
}

void Fader::Init(void)
{
	state_ = STATE::NONE;
	alpha_ = 0.0f;
	isPreEnd_ = true;
	isEnd_ = true;
}

void Fader::Update(void)
{

	if (isEnd_)
	{
		return;
	}

	switch (state_)
	{
	case STATE::FADE_OUT:
		alpha_ += SPEED_ALPHA;
		if (alpha_ > ALPHA_MAX)
		{
			alpha_ = ALPHA_MAX;
			OnReachEnd();
		}
		break;

	case STATE::FADE_IN:
		alpha_ -= SPEED_ALPHA;
		if (alpha_ < 0.0f)
		{
			alpha_ = 0.0f;
			OnReachEnd();
		}
		break;

	default:
		break;
	}

}

void Fader::Draw(void) const
{

	if (state_ == STATE::NONE)
	{
		return;
	}

	// 黒い板を、透明度をつけて画面全体に重ねる
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(alpha_));
	DrawBox(
		0, 0,
		Application::SCREEN_SIZE_X,
		Application::SCREEN_SIZE_Y,
		0x000000, true);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

}

void Fader::OnReachEnd(void)
{
	// 端まで進んだ最初のフレームでは終了にせず、
	// 1フレーム後(Draw のあと)に終了にする。
	// こうすると、真っ暗(または透明)な画面が1回は描かれてからシーンが切り替わる
	if (isPreEnd_)
	{
		isEnd_ = true;
	}
	isPreEnd_ = true;
}