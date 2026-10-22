#pragma once

// アプリ全体の「流れ」に関するフラグ置き場(ヘッダだけで完結)
//   ・ゲーム終了の要求    : RequestQuit() / IsQuitRequested()
//   ・エフェクトの停止要求 : SetEffectFreeze() / IsEffectFrozen()
//
// 使い方
//   メインループ(main.cpp / Application)で
//     while (ProcessMessage() == 0 && !AppFlow::IsQuitRequested()) { ... }
//   のように終了要求を見る
namespace AppFlow
{
	// static変数をヘッダに直接書くとファイルごとに別物になるので、
	// 関数内のstatic変数にして「全体で1つ」にしている
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

	// ゲーム終了を要求する(実際に終わるのはメインループが見たとき)
	inline void RequestQuit(void) { QuitFlag() = true; }
	inline bool IsQuitRequested(void) { return QuitFlag(); }

	// ポーズ中・ゲームオーバー中にEffekseerの再生も止めたいときに使う
	inline void SetEffectFreeze(bool freeze) { FreezeFlag() = freeze; }
	inline bool IsEffectFrozen(void) { return FreezeFlag(); }
}