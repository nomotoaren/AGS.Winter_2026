#pragma once
#include <DxLib.h>

// ブレイク演出(
class BreakEffect
{

public:

	// 再生開始(worldPos: 敵の位置 / totalTime: 演出全体の長さ[秒])
	static void Play(const VECTOR& worldPos, float totalTime);

	// 更新(毎フレーム、全体停止より前に呼ぶ)
	static void Update(float dt);

	// 描画(HUDと同じタイミング)
	static void Draw(void);

	// 再生中か
	static bool IsPlaying(void);

};