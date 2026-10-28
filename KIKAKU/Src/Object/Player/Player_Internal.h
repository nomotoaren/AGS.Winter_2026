#pragma once
// Player の .cpp(Player.cpp / Player_Visual.cpp / Player_Move.cpp / Player_Attack.cpp / Player_Status.cpp)で
// 共通に使う小さな部品。Player.cpp が長くなりすぎたので、ファイルを分けるときにここへまとめた
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

	// 2つの行列を t(0~1) で混ぜる
	//   単純に混ぜると縮んだりゆがんだりするので、向きを直交させ直し、大きさは別に混ぜる
	inline MATRIX BlendMatrix(const MATRIX& a, const MATRIX& b, float t)
	{
		MATRIX r{};
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				r.m[i][j] = a.m[i][j] + (b.m[i][j] - a.m[i][j]) * t;
			}
		}

		auto row = [](const MATRIX& m, int i) { return VGet(m.m[i][0], m.m[i][1], m.m[i][2]); };

		// 各軸の大きさ(スケール)は別に混ぜる
		const float sx = VSize(row(a, 0)) + (VSize(row(b, 0)) - VSize(row(a, 0))) * t;
		const float sy = VSize(row(a, 1)) + (VSize(row(b, 1)) - VSize(row(a, 1))) * t;
		const float sz = VSize(row(a, 2)) + (VSize(row(b, 2)) - VSize(row(a, 2))) * t;

		VECTOR x = row(r, 0);
		VECTOR y = row(r, 1);

		if (VSize(x) < 0.0001f || VSize(y) < 0.0001f)
		{
			return (t < 0.5f) ? a : b;
		}

		x = VNorm(x);
		VECTOR z = VCross(x, y);
		if (VSize(z) < 0.0001f)
		{
			return (t < 0.5f) ? a : b;
		}
		z = VNorm(z);
		y = VCross(z, x);

		r.m[0][0] = x.x * sx; r.m[0][1] = x.y * sx; r.m[0][2] = x.z * sx; r.m[0][3] = 0.0f;
		r.m[1][0] = y.x * sy; r.m[1][1] = y.y * sy; r.m[1][2] = y.z * sy; r.m[1][3] = 0.0f;
		r.m[2][0] = z.x * sz; r.m[2][1] = z.y * sz; r.m[2][2] = z.z * sz; r.m[2][3] = 0.0f;
		r.m[3][3] = 1.0f;

		return r;
	}

	// mixamo のボーンを探す(名前の前に付く文字がモデルによって違うので、順に試す)
	inline int FindMixamoFrame(int model, const char* bone)
	{
		const char* prefixes[] = { "mixamorig:", "mixamorig1:", "mixamorig2:", "" };

		for (const char* p : prefixes)
		{
			const std::string name = std::string(p) + bone;
			const int frame = MV1SearchFrame(model, name.c_str());

			if (frame >= 0)
			{
				return frame;
			}
		}

		return -1;
	}

	// 脚のポーズの調整モード(Debug ビルドのみ)の項目名
	inline const char* const kLegTuneName[] =
	{
		"L_THIGH   左太もも(+前 -後)",
		"L_KNEE    左ひざ(曲げ)",
		"L_OPEN    左脚の開き",
		"R_THIGH   右太もも(+前 -後)",
		"R_KNEE    右ひざ(曲げ)",
		"R_OPEN    右脚の開き",
		"FOOT      足首",
		"LEAN_RATE 体の傾きの倍率",
	};
}
