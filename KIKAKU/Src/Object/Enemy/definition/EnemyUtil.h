#pragma once
#include <cmath>
#include <DxLib.h>
#include "../../../Manager/SceneManager.h"
#include "../EnemyBase.h"	

namespace EnemyUtil
{
	// 「ゼロ」とみなす長さ
	constexpr float kEpsilon = 0.001f;

	// 1フレームあたりの移動量に掛ける倍率
	//   (吹っ飛びなどの速さは 144fps の1フレーム分で決めてあるので、60fps なら約2.4倍にして同じ速さにする)
	inline float FrameScale(void)
	{
		float s = SceneManager::GetInstance().GetFrameScale();

		// 処理落ちした1フレームで、飛びすぎないようにする
		if (s > 4.0f)
		{
			s = 4.0f;
		}

		return s;
	}

	// Y成分を捨てる
	inline VECTOR ToHorizontal(VECTOR v)
	{
		v.y = 0.0f;
		return v;
	}

	// タイマーを減らして、0未満にならないようにする
	inline void CountDown(float& timer, float deltaTime)
	{
		if (timer > 0.0f)
		{
			timer -= deltaTime;

			if (timer < 0.0f)
			{
				timer = 0.0f;
			}
		}
	}

	// 水平方向の単位ベクトル。ほぼ真上・真下なら今向いている方向を使う
	inline VECTOR HorizontalDirOrForward(VECTOR toTarget, const Quaternion& rot)
	{
		toTarget = ToHorizontal(toTarget);

		if (VSize(toTarget) > kEpsilon)
		{
			return VNorm(toTarget);
		}

		VECTOR forward = ToHorizontal(rot.GetForward());

		if (VSize(forward) <= kEpsilon)
		{
			forward = VGet(0.0f, 0.0f, 1.0f);
		}

		return VNorm(forward);
	}

	// 0.0~1.0 に収める
	inline float Saturate(float v)
	{
		if (v < 0.0f) { return 0.0f; }
		if (v > 1.0f) { return 1.0f; }
		return v;
	}

	// 点 p から線分 a-b までの最短距離
	inline float DistanceToSegment(const VECTOR& p, const VECTOR& a, const VECTOR& b)
	{
		const VECTOR ab = VSub(b, a);
		const float lenSq = VDot(ab, ab);

		float t = 0.0f;

		if (lenSq > kEpsilon)
		{
			t = Saturate(VDot(VSub(p, a), ab) / lenSq);
		}

		return VSize(VSub(p, VAdd(a, VScale(ab, t))));
	}

	// 水平な輪を線で描く
	inline void DrawRing3D(const VECTOR& center, float radius, unsigned int color)
	{
		constexpr int kSeg = 48;

		// 線は細いので、半径と高さをずらして重ねて太く見せる
		for (int k = 0; k < 5; k++)
		{
			const float r = radius + (float)(k - 2) * 2.5f;
			const float y = center.y + (float)(k % 2) * 1.5f;

			for (int i = 0; i < kSeg; i++)
			{
				const float a1 = DX_TWO_PI_F * (float)i / (float)kSeg;
				const float a2 = DX_TWO_PI_F * (float)(i + 1) / (float)kSeg;

				DrawLine3D(
					VGet(center.x + cosf(a1) * r, y, center.z + sinf(a1) * r),
					VGet(center.x + cosf(a2) * r, y, center.z + sinf(a2) * r),
					color);
			}
		}
	}

	// 重みの大きいものほど選ばれやすいランダム。選べるものが無ければ -1
	inline int PickWeighted(const int weights[], int count)
	{
		int total = 0;

		for (int i = 0; i < count; i++)
		{
			total += weights[i];
		}

		if (total <= 0)
		{
			return -1;
		}

		int r = GetRand(total - 1);

		for (int i = 0; i < count; i++)
		{
			if (r < weights[i])
			{
				return i;
			}

			r -= weights[i];
		}

		return count - 1;
	}
}
