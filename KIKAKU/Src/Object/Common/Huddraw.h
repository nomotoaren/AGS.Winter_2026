#pragma once

// HUD(プレイヤー・敵)で共通の描画
namespace HudDraw
{
	// 右寄せで文字を描く
	void DrawStringRight(int rightX, int y, const char* str, unsigned int color, int font);

	// 重なる体力ゲージの層の色(0 が最下層)
	unsigned int HpLayerColor(int layer);

	// 重なる体力ゲージ
	//   体力を5枚の層に分け、いちばん上の層から順に減らす。減りきると下の層の色が現れる
	//   ダメージで減った分(delayedHp との差)は、赤いバーが遅れて縮む
	//   戻り値: 残りの層の数(「x3」と表示する用)
	int DrawLayeredHp(float x, float y, float w, float h, float s,
		float hp, float delayedHp, float maxHp,
		unsigned int frameColor, float frameThick);
}