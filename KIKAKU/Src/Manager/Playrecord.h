#pragma once

// 今回のプレイの記録
namespace PlayRecord
{
	// クリアまでにかかった時間(秒)
	inline float& ClearTime(void)
	{
		static float time = 0.0f;
		return time;
	}
}