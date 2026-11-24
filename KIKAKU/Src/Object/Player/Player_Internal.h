#pragma once
#include <string>
#include <DxLib.h>
#include "../../Manager/InputManager.h"
#include "../../Manager/PadInput.h"
#include "../../Manager/SceneManager.h"
#include "../../Manager/ResourceManager.h"
#include "Player.h"

namespace PlayerInternal
{
	// BASE: 通常 / SUPER: 変身後
	// モデル・アニメーションのフォルダ・スケール・移動速度の倍率・攻撃力の倍率
	inline const Player::FormData kFormData[(int)Player::FORM::MAX] =
	{
		{ ResourceManager::SRC::PLAYER,   "Player/Animation/", 1.5f, 1.0f, 1.0f },
		{ ResourceManager::SRC::EVPLAY_,  "Player/AnimationEV/", 0.8f, 0.8f, 2.0f },
	};

	// 1フレームあたりの移動量に掛ける倍率
	//   (移動の速さは 144fps の1フレーム分で決めてあるので、60fps なら約2.4倍にして同じ速さにする)
	inline float FrameScale(void)
	{
		float s = SceneManager::GetInstance().GetFrameScale();

		// 処理落ちした1フレームで、壁をすり抜けるほど飛ばないようにする
		if (s > 4.0f)
		{
			s = 4.0f;
		}

		return s;
	}

	// 移動キー(またはスティック)が押されているか
	inline bool IsMoveKeyDown(InputManager& ins)
	{
		return ins.IsNew(KEY_INPUT_W) ||
			ins.IsNew(KEY_INPUT_A) ||
			ins.IsNew(KEY_INPUT_S) ||
			ins.IsNew(KEY_INPUT_D) ||
			PadInput::IsStickTilted();
	}
}