#pragma once
#include <DxLib.h>

// プレイヤーの部品(PlayerAttack / PlayerBoostChase / PlayerKamehameha など)で共通に使う小さな計算
namespace PlayerMath
{
	// これ以下の長さのベクトルは「向きがない」とみなす
	constexpr float EPSILON = 0.001f;

	// Y 成分を捨てて、水平方向だけにする
	inline VECTOR ToHorizontal(const VECTOR& v)
	{
		return VGet(v.x, 0.0f, v.z);
	}

	// 長さがほぼ 0 のときは (0,0,0) を返す正規化
	inline VECTOR NormalizeSafe(const VECTOR& v)
	{
		return (VSize(v) > EPSILON) ? VNorm(v) : VGet(0.0f, 0.0f, 0.0f);
	}
}