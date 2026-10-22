#pragma once
#include <DxLib.h>

// ブレイク演出(画像版)
//   1. 白いフラッシュと一緒に BREAK! の画像が拡大しながら出る
//   2. 画像が震える
//   3. 画像が無数の破片に砕けて飛び散り、回転しながら落ちて消えていく
// 演出は自前の時計(Update の dt)で進むので、画面が止まっていても動く
//
// 使う画像: リソースマネージャーの SRC::BREAK (背景が透明なPNG)
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