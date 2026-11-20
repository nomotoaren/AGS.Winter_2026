#pragma once

namespace AppFlow
{

	inline static bool& QuitFlag(void)
	{
		static bool flag = false;
		return flag;
	}

	inline static bool& FreezeFlag(void)
	{
		static bool flag = false;
		return flag;
	}

	// ゲーム終了を要求する
	inline static void RequestQuit(void) { QuitFlag() = true; }
	inline static bool IsQuitRequested(void) { return QuitFlag(); }

	// ポーズ中・ゲームオーバー中にEffekseerの再生も止めたいときに使う
	inline static void SetEffectFreeze(bool freeze) { FreezeFlag() = freeze; }
	inline static bool IsEffectFrozen(void) { return FreezeFlag(); }
}