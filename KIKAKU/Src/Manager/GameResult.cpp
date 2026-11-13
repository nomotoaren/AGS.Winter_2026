#pragma once

// ƒQ[ƒ€‚ÌŒ‹‰Ê
namespace GameResult
{
	// ƒNƒŠƒA‚Ü‚Å‚É‚©‚©‚Á‚½ŠÔ
	inline static float& PlayTime(void)
	{
		static float time = 0.0f;
		return time;
	}
}