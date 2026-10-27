#pragma once

// 敵のHPバーまわり(EnemyHud / EnemyStunBar)で共通の配置
//   基準解像度(幅1500)でのレイアウト。実際の画面幅に合わせて拡大縮小する
namespace EnemyHudLayout
{
	constexpr float BASE_WIDTH = 1500.0f;

	constexpr float MARGIN_X = 50.0f;		// 画面右端からアイコンまで
	constexpr float MARGIN_Y = 28.0f;		// 画面上端からアイコンまで
	constexpr float ICON_RADIUS = 40.0f;
	constexpr float HP_BAR_HEIGHT = 16.0f;

	// 画面幅から求めた、アイコンとHPバーの位置
	struct Anchor
	{
		float scale;	// 基準解像度からの倍率
		float iconR;	// アイコンの半径
		float iconX;	// アイコンの中心
		float iconY;
		float barRight;	// HPバーの右端
		float barY;		// HPバーの上端
	};

	inline Anchor Compute(int screenW)
	{
		Anchor a{};
		a.scale = screenW / BASE_WIDTH;
		a.iconR = ICON_RADIUS * a.scale;
		a.iconX = screenW - MARGIN_X * a.scale - a.iconR - 10.0f * a.scale;
		a.iconY = MARGIN_Y * a.scale + a.iconR;
		a.barRight = a.iconX - a.iconR * 0.35f;
		a.barY = a.iconY - 20.0f * a.scale;
		return a;
	}
}