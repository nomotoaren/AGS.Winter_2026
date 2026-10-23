#pragma once

// ゲームの結果(ゲームシーンで書き込み、クリア画面で読む)
namespace GameResult
{
	// クリアまでにかかった時間(秒)
	// static変数をヘッダに直接書くとファイルごとに別物になるので、
	// 関数内のstatic変数にして「全体で1つ」にしている
	inline float& PlayTime(void)
	{
		static float time = 0.0f;
		return time;
	}
}