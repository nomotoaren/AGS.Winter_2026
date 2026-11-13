#pragma once

namespace AppFlow
{
	inline bool& QuitFlag(void)
	{
		static bool flag = false;
		return flag;
	}

	inline bool& FreezeFlag(void)
	{
		static bool flag = false;
		return flag;
	}

	// ゲーム終了を要求する
	inline void RequestQuit(void) { QuitFlag() = true; }
	inline bool IsQuitRequested(void) { return QuitFlag(); }

	// ポーズ中・ゲームオーバー中にEffekseerの再生も止めたいときに使う
	inline void SetEffectFreeze(bool freeze) { FreezeFlag() = freeze; }
	inline bool IsEffectFrozen(void) { return FreezeFlag(); }
}