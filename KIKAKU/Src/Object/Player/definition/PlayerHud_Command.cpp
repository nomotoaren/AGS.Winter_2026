#include <cmath>
#include <cstdio>
#include <cstring>
#include <DxLib.h>
#include "../Player.h"
#include "PlayerConfig.h"
#include "../../Common/HudDraw.h"
#include "PlayerHud.h"

using HudDraw::DrawStringRight;

namespace
{
	// 左下のコマンド一覧
	constexpr float CMD_LEFT = 40.0f;
	constexpr float CMD_BOTTOM = 40.0f;
	constexpr float CMD_WIDTH = 380.0f;
	constexpr float CMD_ROW_H = 32.0f;
	constexpr float CMD_ROW_GAP = 4.0f;
	constexpr float CMD_TITLE_H = 36.0f;
	constexpr float CMD_KEY_MIN_W = 32.0f;
	constexpr int CMD_MAX_ROWS = 8;

	// コマンド一覧に出すコントローラーのボタンの絵
	enum class PadIcon
	{
		CIRCLE,		// ○
		CROSS,		// ×
		SQUARE,		// □
		TRIANGLE,	// △
		L1,
		L2,
		R2,
		L3,
		PLUS		// 「+」(同時押し)
	};

	bool IsFaceButton(PadIcon icon)
	{
		return icon == PadIcon::CIRCLE || icon == PadIcon::CROSS ||
			icon == PadIcon::SQUARE || icon == PadIcon::TRIANGLE;
	}

	const char* PadIconText(PadIcon icon)
	{
		switch (icon)
		{
		case PadIcon::L1: return "L1";
		case PadIcon::L2: return "L2";
		case PadIcon::R2: return "R2";
		case PadIcon::L3: return "L3";
		default: return "";
		}
	}

	// ボタンの絵の横幅(h = 高さ)
	float PadIconWidth(PadIcon icon, float h, float s, int font)
	{
		if (IsFaceButton(icon))
		{
			return h;
		}

		if (icon == PadIcon::PLUS)
		{
			return 14.0f * s;
		}

		const char* text = PadIconText(icon);
		return GetDrawStringWidthToHandle(text, (int)std::strlen(text), font) + 14.0f * s;
	}

	// ボタンの絵を描く(x = 左端、cy = 縦の中心)
	void DrawPadIcon(PadIcon icon, float x, float cy, float h, float s, int font)
	{
		const float r = h * 0.5f;
		const float cx = x + r;

		if (IsFaceButton(icon))
		{
			unsigned int color = GetColor(255, 255, 255);

			switch (icon)
			{
			case PadIcon::CIRCLE:   color = GetColor(240, 85, 85);   break;
			case PadIcon::CROSS:    color = GetColor(100, 150, 255); break;
			case PadIcon::SQUARE:   color = GetColor(245, 130, 205); break;
			case PadIcon::TRIANGLE: color = GetColor(95, 215, 145);  break;
			default: break;
			}

			DrawCircleAA(cx, cy, r, 28, GetColor(22, 24, 38), TRUE);
			DrawCircleAA(cx, cy, r, 28, GetColor(190, 195, 210), FALSE, 1.5f * s);

			const float g = r * 0.48f;
			const float t = 2.2f * s;

			switch (icon)
			{
			case PadIcon::CIRCLE:
				DrawCircleAA(cx, cy, g, 24, color, FALSE, t);
				break;
			case PadIcon::CROSS:
				DrawLineAA(cx - g, cy - g, cx + g, cy + g, color, t);
				DrawLineAA(cx - g, cy + g, cx + g, cy - g, color, t);
				break;
			case PadIcon::SQUARE:
				DrawBoxAA(cx - g, cy - g, cx + g, cy + g, color, FALSE, t);
				break;
			case PadIcon::TRIANGLE:
				DrawTriangleAA(cx, cy - g * 1.15f, cx - g * 1.1f, cy + g * 0.85f,
					cx + g * 1.1f, cy + g * 0.85f, color, FALSE, t);
				break;
			default:
				break;
			}

			return;
		}

		if (icon == PadIcon::PLUS)
		{
			const float g = 4.5f * s;
			DrawLineAA(cx - g, cy, cx + g, cy, GetColor(255, 255, 255), 2.0f * s);
			DrawLineAA(cx, cy - g, cx, cy + g, GetColor(255, 255, 255), 2.0f * s);
			return;
		}

		// L1 / L2 / R2 / L3 は丸い角のボタン + 文字
		const float w = PadIconWidth(icon, h, s, font);
		const float bh = h * 0.86f;

		DrawRoundRectAA(x, cy - bh * 0.5f, x + w, cy + bh * 0.5f, bh * 0.4f, bh * 0.4f, 8,
			GetColor(235, 238, 248), TRUE);
		DrawRoundRectAA(x, cy - bh * 0.5f, x + w, cy + bh * 0.5f, bh * 0.4f, bh * 0.4f, 8,
			GetColor(90, 95, 115), FALSE, 1.5f * s);

		const char* text = PadIconText(icon);
		const int tw = GetDrawStringWidthToHandle(text, (int)std::strlen(text), font);

		DrawStringToHandle((int)(x + (w - tw) * 0.5f), (int)(cy - 10.0f * s),
			text, GetColor(25, 30, 50), font, GetColor(235, 238, 248));
	}
}

// 左下のコマンド一覧
void PlayerHud::DrawCommandPanel(const Player& player) const
{
	int sw = 0;
	int sh = 0;
	GetDrawScreenSize(&sw, &sh);
	const float s = sw / BASE_WIDTH;

	const bool isSuper = (player.GetForm() == Player::FORM::SUPER);
	const float ki = player.GetKi();

	struct Command
	{
		const char* key;		// キーボードのキー
		PadIcon icons[4];		// パッドのボタン(PLUS でつなぐ)
		int iconCount;
		bool hold;				// 長押しか
		const char* name;
		bool enabled;			// 今使えるか
		bool active;			// 今実行中か
	};

	// 通常
	const Command pageNormal[] =
	{
		{ "F",      { PadIcon::SQUARE },   1, false, "攻撃",     true, player.IsAttack() },
		{ "U",      { PadIcon::CIRCLE },   1, false, "気弾",     true, false },
		{ "SPACE",  { PadIcon::L3 },       1, false, "高速接近", true, false },
		{ "J", { PadIcon::CROSS },    1, false, "回避",     true, player.IsDodging() },
		{ "T",      { PadIcon::TRIANGLE }, 1, true,  "気溜め",   true, false },
		{ "L",      { PadIcon::L2 },       1, true,  "ガード",   true, player.IsGuard() },
		{ "TAB",    { PadIcon::L1 },       1, true,  "必殺技",   true, false },
		{ "H",      { PadIcon::L2, PadIcon::PLUS, PadIcon::R2 }, 3, true, "フォーム", true, false },
	};

	// 必殺技パレット
	const Command pageSpecial[] =
	{
		{ "R", { PadIcon::SQUARE }, 1, false, "かめはめ波",
			ki >= Player::KAMEHAME_KI_COST, player.IsKamehame() },
		{ "U", { PadIcon::CIRCLE }, 1, false, "連続気弾",
			ki >= PlayerConfig::Barrage::KI_COST, player.IsBarrageActive() },
	};

	// フォームチェンジパレット
	const Command pageForm[] =
	{
		{ "G", { PadIcon::SQUARE }, 1, false,
			isSuper ? "元に戻る" : "変身",
			isSuper || ki >= PlayerConfig::Transform::KI_COST, player.IsTransforming() },
	};

	const Command* commands = pageNormal;
	int count = (int)(sizeof(pageNormal) / sizeof(pageNormal[0]));
	const char* title = "コマンド";
	const char* hint = "";
	unsigned int titleColor = GetColor(255, 255, 255);

	if (palette_ == 2)
	{
		commands = pageForm;
		count = (int)(sizeof(pageForm) / sizeof(pageForm[0]));
		title = "フォームチェンジ";
		hint = "押している間";
		titleColor = GetColor(150, 225, 255);
	}
	else if (palette_ == 1)
	{
		commands = pageSpecial;
		count = (int)(sizeof(pageSpecial) / sizeof(pageSpecial[0]));

		title = "必殺技パレット";
		hint = "押している間";
		titleColor = GetColor(255, 235, 120);
	}

	const float rowH = CMD_ROW_H * s;
	const float rowGap = CMD_ROW_GAP * s;
	const float titleH = CMD_TITLE_H * s;
	const float w = CMD_WIDTH * s;
	const float h = titleH + (rowH + rowGap) * (float)CMD_MAX_ROWS;	// 高さはページによらず固定
	const float x = CMD_LEFT * s;
	const float y = sh - CMD_BOTTOM * s - h;

	// ---- 見出し ----
	{
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 190);
		DrawBoxAA(x, y, x + w, y + titleH - 4.0f * s, GetColor(12, 16, 30), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawBoxAA(x, y, x + 5.0f * s, y + titleH - 4.0f * s, GetColor(255, 205, 80), TRUE);

		DrawStringToHandle((int)(x + 16.0f * s), (int)(y + 4.0f * s), title, titleColor, fontSmall_);

		// 切り替えの案内(右寄せ)
		DrawStringRight((int)(x + w - 12.0f * s), (int)(y + 6.0f * s), hint,
			GetColor(255, 210, 90), fontCmd_);
	}

	// ---- 列をそろえる(キーボードのキー / パッドのボタン / 名前の位置を全行で同じにする) ----
	const float keyH = rowH - 8.0f * s;
	const float iconH = keyH - 4.0f * s;
	const float iconGap = 3.0f * s;
	const float iconPadX = 8.0f * s;

	auto calcPadW = [&](const Command& c) -> float
		{
			float inner = 0.0f;

			for (int k = 0; k < c.iconCount; k++)
			{
				inner += PadIconWidth(c.icons[k], iconH, s, fontCmd_);
				if (k > 0) { inner += iconGap; }
			}

			if (c.hold)
			{
				inner += iconGap + (float)GetDrawStringWidthToHandle("長押し", (int)std::strlen("長押し"), fontCmd_);
			}

			return inner + iconPadX * 2.0f;
		};

	float maxPadW = 0.0f;
	for (int i = 0; i < count; i++)
	{
		const float pw = calcPadW(commands[i]);
		if (pw > maxPadW) { maxPadW = pw; }
	}

	// 一番長いキー名(LSHIFT)の幅にそろえる
	const float keyColW = GetDrawStringWidthToHandle("LSHIFT", (int)std::strlen("LSHIFT"), fontCmd_) + 16.0f * s;
	const float padStartX = x + 12.0f * s + keyColW + 6.0f * s;
	const float nameX = padStartX + maxPadW + 14.0f * s;

	// ---- 各行 ----
	for (int i = 0; i < count; i++)
	{
		const Command& cmd = commands[i];

		const float ry = y + titleH + (rowH + rowGap) * (float)i;

		// 帯の色(通常=濃いオレンジ / 実行中=明るい黄 / 使えない=灰色)
		int r = 215;
		int g = 110;
		int b = 25;

		if (cmd.active)
		{
			r = 250; g = 200; b = 60;
		}
		else if (!cmd.enabled)
		{
			r = 85; g = 88; b = 98;
		}

		// 右へいくほど少し透明になる帯(左は濃くして読みやすく)
		for (int k = 0; k < 24; k++)
		{
			const float t0 = (float)k / 24.0f;
			const float t1 = (float)(k + 1) / 24.0f;

			SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(235.0f * (1.0f - t0 * 0.6f)));
			DrawBoxAA(x + w * t0, ry, x + w * t1, ry + rowH, GetColor(r, g, b), TRUE);
		}

		// 上下の金色の線
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 220);
		DrawLineAA(x, ry, x + w * 0.95f, ry, GetColor(255, 228, 140), 1.5f * s);
		DrawLineAA(x, ry + rowH, x + w * 0.95f, ry + rowH, GetColor(255, 228, 140), 1.5f * s);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

		float cursor = x + 12.0f * s;
		const float ky = ry + 4.0f * s;
		const float cy = ky + keyH * 0.5f;

		// キーボードのキー(丸い角の枠)
		{
			const int kw = GetDrawStringWidthToHandle(cmd.key, (int)std::strlen(cmd.key), fontCmd_);

			// (Windows.h の max マクロと衝突するので std::max は使わない)
			const float keyMin = CMD_KEY_MIN_W * s;
			const float keyText = kw + 16.0f * s;
			const float keyW = (keyText > keyMin) ? keyText : keyMin;

			DrawRoundRectAA(cursor, ky, cursor + keyW, ky + keyH, keyH * 0.5f, keyH * 0.5f, 10,
				GetColor(245, 246, 250), TRUE);
			DrawRoundRectAA(cursor, ky, cursor + keyW, ky + keyH, keyH * 0.5f, keyH * 0.5f, 10,
				GetColor(90, 95, 115), FALSE, 1.5f * s);

			// キーの文字は縁を白地と同じ色にして、濃い文字だけが見えるようにする
			DrawStringToHandle((int)(cursor + (keyW - kw) * 0.5f), (int)(cy - 10.0f * s),
				cmd.key, GetColor(25, 30, 50), fontCmd_, GetColor(245, 246, 250));

		}

		cursor = padStartX;

		// コントローラーのボタンの絵(暗い丸い枠の中に並べる)
		{
			const float padW = calcPadW(cmd);

			SetDrawBlendMode(DX_BLENDMODE_ALPHA, 215);
			DrawRoundRectAA(cursor, ky, cursor + padW, ky + keyH, keyH * 0.5f, keyH * 0.5f, 10,
				GetColor(14, 18, 32), TRUE);
			SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
			DrawRoundRectAA(cursor, ky, cursor + padW, ky + keyH, keyH * 0.5f, keyH * 0.5f, 10,
				GetColor(255, 228, 140), FALSE, 1.5f * s);

			float ix = cursor + iconPadX;

			for (int k = 0; k < cmd.iconCount; k++)
			{
				if (k > 0) { ix += iconGap; }

				DrawPadIcon(cmd.icons[k], ix, cy, iconH, s, fontCmd_);
				ix += PadIconWidth(cmd.icons[k], iconH, s, fontCmd_);
			}

			if (cmd.hold)
			{
				DrawStringToHandle((int)(ix + iconGap), (int)(cy - 10.0f * s),
					"長押し", GetColor(255, 235, 150), fontCmd_);
			}
		}

		// 名前
		DrawStringToHandle((int)nameX, (int)(cy - 10.0f * s),
			cmd.name, GetColor(255, 255, 255), fontCmd_);
	}
}