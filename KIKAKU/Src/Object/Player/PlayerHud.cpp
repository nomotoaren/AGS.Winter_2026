#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <DxLib.h>
#include "../../Manager/SceneManager.h"
#include "../../Manager/InputManager.h"
#include "../../Manager/PadInput.h"
#include "Player.h"
#include "PlayerHud.h"

namespace
{
	// 基準解像度(横1500)でのレイアウト。実際の画面幅に合わせて拡大縮小する
	constexpr float BASE_WIDTH = 1500.0f;

	constexpr float MARGIN_X = 50.0f;
	constexpr float MARGIN_Y = 40.0f;

	// アイコン(右下)
	constexpr float ICON_RADIUS = 62.0f;

	// HPバー / 気ゲージ(右端がアイコンの裏にくる。右から左へ減る)
	constexpr float BAR_WIDTH = 440.0f;
	constexpr float BAR_HEIGHT = 20.0f;
	constexpr float KI_WIDTH = 340.0f;
	constexpr float KI_HEIGHT = 14.0f;

	constexpr float DELAY_WAIT_TIME = 0.5f;		// 被弾してから黄色いバーが減り始めるまで
	constexpr float DELAY_SPEED_RATE = 0.45f;	// 黄色いバーの減る速さ(最大HPに対する割合/秒)
	constexpr float FLASH_TIME = 0.25f;

	// 右上のバトル条件(敵のHPバーの下)
	constexpr float OBJ_TOP = 128.0f;
	constexpr float OBJ_WIDTH = 520.0f;
	constexpr float OBJ_STRIP_H = 34.0f;

	// コンボ表示(右中)
	constexpr float COMBO_HOLD_TIME = 1.3f;		// コンボ終了後も表示を残す時間
	constexpr float COMBO_POP_TIME = 0.18f;

	// 左下のコマンド一覧
	constexpr float CMD_LEFT = 40.0f;
	constexpr float CMD_BOTTOM = 40.0f;
	constexpr float CMD_WIDTH = 380.0f;
	constexpr float CMD_ROW_H = 32.0f;
	constexpr float CMD_ROW_GAP = 4.0f;
	constexpr float CMD_TITLE_H = 36.0f;
	constexpr float CMD_KEY_MIN_W = 32.0f;
	constexpr int CMD_MAX_ROWS = 8;				// 1ページの最大行数(高さを固定する)
	constexpr float CMD_TF_KI_COST = 30.0f;		// 変身に必要な気(Player.cpp の TRANSFORM_KI_COST と同じ値)

	// ダメージ数字
	constexpr float POPUP_TIME = 0.9f;
	constexpr float POPUP_RISE = 60.0f;			// 1秒あたりに上がる高さ(3D空間)
	constexpr float POPUP_FADE_TIME = 0.3f;

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

	// 右寄せで文字を描く
	void DrawStringRight(int rightX, int y, const char* str, unsigned int color, int font)
	{
		const int w = GetDrawStringWidthToHandle(str, (int)std::strlen(str), font);
		DrawStringToHandle(rightX - w, y, str, color, font);
	}

	// 右から左へ減るゲージ。
	//   rate    : 現在値
	//   lagRate : 遅れて減る部分(rate より大きい)
	void DrawGauge(float x, float y, float w, float h, float s,
		float rate, float lagRate, unsigned int lagColor, unsigned int color,
		unsigned int frameColor, float frameThick)
	{
		rate = std::clamp(rate, 0.0f, 1.0f);
		lagRate = std::clamp(lagRate, 0.0f, 1.0f);

		// 影
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
		DrawBoxAA(x - 3.0f * s, y - 3.0f * s, x + w + 3.0f * s, y + h + 3.0f * s,
			GetColor(10, 10, 14), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

		// 空の部分
		DrawBoxAA(x, y, x + w, y + h, GetColor(28, 30, 34), TRUE);

		const float right = x + w;

		// 遅れて減る部分
		if (lagRate > rate)
		{
			DrawBoxAA(right - w * lagRate, y, right - w * rate, y + h, lagColor, TRUE);
		}

		// 現在値
		if (rate > 0.0f)
		{
			DrawBoxAA(right - w * rate, y, right, y + h, color, TRUE);

			// 上半分に光沢
			SetDrawBlendMode(DX_BLENDMODE_ALPHA, 75);
			DrawBoxAA(right - w * rate, y, right, y + h * 0.45f, GetColor(255, 255, 255), TRUE);
			SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		}

		DrawBoxAA(x, y, x + w, y + h, frameColor, FALSE, frameThick);
	}
}

PlayerHud::PlayerHud(void)
	:
	fontName_(-1),
	fontNum_(-1),
	fontIcon_(-1),
	fontSmall_(-1),
	fontCmd_(-1),
	fontCombo_(-1),
	fontDamage_(-1),
	name_("PLAYER"),
	iconText_("P"),
	objective_("敵を倒せ！"),
	prevHp_(-1.0f),
	delayedHp_(0.0f),
	delayWait_(0.0f),
	flashTimer_(0.0f),
	time_(0.0f),
	shownCombo_(0),
	comboHold_(0.0f),
	comboPop_(0.0f),
	palette_(0),
	tabHeld_(false)
{
}

PlayerHud::~PlayerHud(void)
{
	if (fontName_ != -1) { DeleteFontToHandle(fontName_); }
	if (fontNum_ != -1) { DeleteFontToHandle(fontNum_); }
	if (fontIcon_ != -1) { DeleteFontToHandle(fontIcon_); }
	if (fontSmall_ != -1) { DeleteFontToHandle(fontSmall_); }
	if (fontCmd_ != -1) { DeleteFontToHandle(fontCmd_); }
	if (fontCombo_ != -1) { DeleteFontToHandle(fontCombo_); }
	if (fontDamage_ != -1) { DeleteFontToHandle(fontDamage_); }
}

void PlayerHud::Init(void)
{
	int sw = 0;
	int sh = 0;
	GetDrawScreenSize(&sw, &sh);
	const float s = sw / BASE_WIDTH;

	fontName_ = CreateFontToHandle(nullptr, (int)(26.0f * s), 3,
		DX_FONTTYPE_ANTIALIASING_EDGE_4X4, -1, 2);
	fontNum_ = CreateFontToHandle(nullptr, (int)(22.0f * s), 3,
		DX_FONTTYPE_ANTIALIASING_EDGE_4X4, -1, 2);
	fontIcon_ = CreateFontToHandle(nullptr, (int)(52.0f * s), 3,
		DX_FONTTYPE_ANTIALIASING_EDGE_4X4, -1, 2);
	fontSmall_ = CreateFontToHandle(nullptr, (int)(22.0f * s), 3,
		DX_FONTTYPE_ANTIALIASING_EDGE_4X4, -1, 2);
	fontCmd_ = CreateFontToHandle(nullptr, (int)(21.0f * s), 4,
		DX_FONTTYPE_ANTIALIASING_EDGE_4X4, -1, 2);
	fontCombo_ = CreateFontToHandle(nullptr, (int)(84.0f * s), 5,
		DX_FONTTYPE_ANTIALIASING_EDGE_4X4, -1, 4);
	fontDamage_ = CreateFontToHandle(nullptr, (int)(48.0f * s), 5,
		DX_FONTTYPE_ANTIALIASING_EDGE_4X4, -1, 3);

	prevHp_ = -1.0f;
	popups_.clear();
}

void PlayerHud::SetName(const std::string& name) { name_ = name; }
void PlayerHud::SetIconText(const std::string& text) { iconText_ = text; }
void PlayerHud::SetObjective(const std::string& text) { objective_ = text; }

void PlayerHud::AddDamagePopup(const VECTOR& worldPos, int damage)
{
	DamagePopup popup;
	popup.pos = worldPos;
	popup.value = damage;
	popup.timer = POPUP_TIME;

	popups_.push_back(popup);
}

void PlayerHud::Update(const Player& player)
{
	const float dt = SceneManager::GetInstance().GetDeltaTime();
	const float hp = (float)player.GetHp();
	const float maxHp = (float)Player::MAX_HP;

	time_ += dt;

	// 初回は現在値に合わせる
	if (prevHp_ < 0.0f)
	{
		prevHp_ = hp;
		delayedHp_ = hp;
	}

	// 被弾: 黄色いバーはそのまま残し、少し待ってから減らす
	if (hp < prevHp_)
	{
		delayWait_ = DELAY_WAIT_TIME;
		flashTimer_ = FLASH_TIME;
	}

	// 回復: 黄色いバーも一緒に増やす
	if (hp > delayedHp_)
	{
		delayedHp_ = hp;
	}

	if (delayedHp_ > hp)
	{
		if (delayWait_ > 0.0f)
		{
			delayWait_ -= dt;
		}
		else
		{
			delayedHp_ -= maxHp * DELAY_SPEED_RATE * dt;

			if (delayedHp_ < hp)
			{
				delayedHp_ = hp;
			}
		}
	}

	if (flashTimer_ > 0.0f)
	{
		flashTimer_ -= dt;

		if (flashTimer_ < 0.0f)
		{
			flashTimer_ = 0.0f;
		}
	}

	prevHp_ = hp;

	// コマンドのページ(L1/TAB=必殺技パレット、L2+R2=フォームチェンジ)
	tabHeld_ = InputManager::GetInstance().IsNew(KEY_INPUT_TAB);

	if (PadInput::IsFormPalette() || InputManager::GetInstance().IsNew(KEY_INPUT_H))
	{
		palette_ = 2;
	}
	else if (tabHeld_ || PadInput::IsSpecialPalette())
	{
		palette_ = 1;
	}
	else
	{
		palette_ = 0;
	}

	// コンボ表示(終わっても少し残す)
	const int combo = player.GetCombo();

	if (combo > 0)
	{
		if (combo != shownCombo_)
		{
			comboPop_ = COMBO_POP_TIME;
		}

		shownCombo_ = combo;
		comboHold_ = COMBO_HOLD_TIME;
	}
	else if (comboHold_ > 0.0f)
	{
		comboHold_ -= dt;
	}

	if (comboPop_ > 0.0f)
	{
		comboPop_ -= dt;
	}

	// ダメージ数字
	for (auto it = popups_.begin(); it != popups_.end();)
	{
		it->timer -= dt;
		it->pos.y += POPUP_RISE * dt;

		if (it->timer <= 0.0f)
		{
			it = popups_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

unsigned int PlayerHud::GetHpColor(float rate) const
{
	if (rate > 0.5f)
	{
		return GetColor(75, 215, 65);	// 緑
	}

	if (rate > 0.25f)
	{
		return GetColor(245, 215, 55);	// 黄
	}

	// ピンチは赤く点滅
	const float pulse = 0.5f + 0.5f * sinf(time_ * 9.0f);
	const int r = 200 + (int)(55.0f * pulse);
	const int g = 40 + (int)(40.0f * pulse);

	return GetColor(r, g, 45);
}

void PlayerHud::Draw(const Player& player)
{
	int sw = 0;
	int sh = 0;
	GetDrawScreenSize(&sw, &sh);
	const float s = sw / BASE_WIDTH;

	const float maxHp = (float)Player::MAX_HP;
	const float hp = (float)player.GetHp();

	const float rate = std::clamp(hp / maxHp, 0.0f, 1.0f);
	const float delayedRate = std::clamp(delayedHp_ / maxHp, 0.0f, 1.0f);

	// ---- レイアウト(画面右下。アイコンが一番右、バーはその左) ----
	const float iconR = ICON_RADIUS * s;
	const float iconX = sw - MARGIN_X * s - iconR - 14.0f * s;
	const float iconY = sh - MARGIN_Y * s - iconR - 8.0f * s;

	const float barRight = iconX - iconR * 0.35f;	// 右端はアイコンの裏にもぐらせる
	const float textRight = iconX - iconR - 8.0f * s;	// 文字の右端はアイコンの手前
	const float barW = BAR_WIDTH * s;
	const float barH = BAR_HEIGHT * s;
	const float barX = barRight - barW;
	const float barY = iconY - 26.0f * s;

	// ---- 名前(バーの上、右寄せ) ----
	DrawStringRight((int)textRight, (int)(barY - 38.0f * s),
		name_.c_str(), GetColor(255, 255, 255), fontName_);

	// ---- HPバー ----
	unsigned int frameColor = GetColor(165, 135, 70);
	float frameThick = 2.0f * s;

	if (flashTimer_ > 0.0f)
	{
		frameColor = GetColor(255, 90, 70);
		frameThick = 3.5f * s;
	}

	DrawGauge(barX, barY, barW, barH, s, rate, delayedRate,
		GetColor(245, 220, 60), GetHpColor(rate), frameColor, frameThick);

	// ---- HPの数値(バーの下、右寄せ) ----
	char buf[32];
	{
		std::snprintf(buf, sizeof(buf), "%d", player.GetHp());

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 150);
		DrawBoxAA(textRight - 190.0f * s, barY + barH + 1.0f * s,
			textRight + 8.0f * s, barY + barH + 28.0f * s, GetColor(10, 10, 14), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

		DrawStringRight((int)textRight, (int)(barY + barH + 2.0f * s),
			buf, GetColor(200, 255, 130), fontNum_);
	}

	// ---- 気ゲージ(HPバーより短く、少し右) ----
	const float ki = player.GetKi();
	const float kiRate = std::clamp(ki / Player::MAX_KI, 0.0f, 1.0f);
	const bool kiFull = (ki >= Player::MAX_KI);

	const float kiW = KI_WIDTH * s;
	const float kiH = KI_HEIGHT * s;
	const float kiRight = barRight + 6.0f * s;
	const float kiX = kiRight - kiW;
	const float kiY = barY + barH + 36.0f * s;

	unsigned int kiColor = GetColor(80, 185, 255);

	if (kiFull)
	{
		const float pulse = 0.5f + 0.5f * sinf(time_ * 6.0f);
		kiColor = GetColor(110 + (int)(80.0f * pulse), 215 + (int)(30.0f * pulse), 255);
	}

	DrawGauge(kiX, kiY, kiW, kiH, s, kiRate, kiRate, kiColor, kiColor,
		GetColor(165, 135, 70), 2.0f * s);

	// かめはめ波に必要な量の目印(足りていれば明るく光る)
	{
		const float t = Player::KAMEHAME_KI_COST / Player::MAX_KI;
		const float mx = kiRight - kiW * t;
		const bool enough = (ki >= Player::KAMEHAME_KI_COST);

		DrawLineAA(mx, kiY - 2.0f * s, mx, kiY + kiH + 2.0f * s,
			enough ? GetColor(255, 240, 150) : GetColor(150, 160, 185), 2.0f * s);
	}

	// 気の数値
	{
		std::snprintf(buf, sizeof(buf), "%d", (int)ki);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 150);
		DrawBoxAA(textRight - 150.0f * s, kiY + kiH + 1.0f * s,
			textRight + 8.0f * s, kiY + kiH + 28.0f * s, GetColor(10, 10, 14), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

		DrawStringRight((int)textRight, (int)(kiY + kiH + 2.0f * s),
			buf, GetColor(170, 225, 255), fontNum_);
	}

	// ---- アイコン ----
	const unsigned int dark = GetColor(10, 14, 26);
	const bool isSuper = (player.GetForm() == Player::FORM::SUPER);

	DrawCircleAA(iconX, iconY, iconR + 6.0f * s, 48, dark, TRUE);
	DrawCircleAA(iconX, iconY, iconR, 48, GetColor(35, 60, 110), TRUE);

	// 上半分に光沢
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 60);
	DrawCircleAA(iconX, iconY - iconR * 0.28f, iconR * 0.72f, 48, GetColor(255, 255, 255), TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	DrawCircleAA(iconX, iconY, iconR, 48, GetColor(255, 200, 70), FALSE, 5.0f * s);
	DrawCircleAA(iconX, iconY, iconR + 6.0f * s, 48,
		flashTimer_ > 0.0f ? GetColor(255, 90, 70) : GetColor(225, 232, 245), FALSE, 2.0f * s);

	// 変身中は頭の上に輪っか
	if (isSuper)
	{
		DrawOvalAA(iconX, iconY - iconR - 12.0f * s, 30.0f * s, 8.0f * s, 24,
			GetColor(255, 235, 110), FALSE, 3.0f * s);
	}

	const int tw = GetDrawStringWidthToHandle(iconText_.c_str(), (int)iconText_.size(), fontIcon_);
	DrawStringToHandle((int)(iconX - tw * 0.5f), (int)(iconY - 28.0f * s),
		iconText_.c_str(), GetColor(255, 255, 255), fontIcon_);

	// ---- そのほか ----
	DrawObjective();
	DrawCombo();
	DrawCommandPanel(player);
	DrawDamagePopups();
}

// 右上の「バトル条件」(敵のHPバーの下)
void PlayerHud::DrawObjective(void) const
{
	if (objective_.empty())
	{
		return;
	}

	int sw = 0;
	int sh = 0;
	GetDrawScreenSize(&sw, &sh);
	const float s = sw / BASE_WIDTH;

	const float w = OBJ_WIDTH * s;
	const float stripH = OBJ_STRIP_H * s;
	const float right = sw - MARGIN_X * s;
	const float left = right - w;
	const float y = OBJ_TOP * s;

	// タグ(赤)
	DrawStringRight((int)right, (int)y, "バトル条件", GetColor(235, 70, 55), fontSmall_);

	// 帯(右へいくほど濃くなる)
	const float stripY = y + 30.0f * s;

	for (int i = 0; i < 24; i++)
	{
		const float t0 = (float)i / 24.0f;
		const float t1 = (float)(i + 1) / 24.0f;

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(170.0f * t0));
		DrawBoxAA(left + w * t0, stripY, left + w * t1, stripY + stripH,
			GetColor(8, 12, 24), TRUE);
	}
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	// 下線
	DrawLineAA(left, stripY + stripH, right, stripY + stripH, GetColor(235, 235, 240), 2.0f * s);

	// 目的の文字(右寄せ)
	DrawStringRight((int)(right - 10.0f * s), (int)(stripY + stripH * 0.5f - 11.0f * s),
		objective_.c_str(), GetColor(255, 255, 255), fontSmall_);
}

// 右中のコンボ表示
void PlayerHud::DrawCombo(void) const
{
	if (shownCombo_ <= 0 || comboHold_ <= 0.0f)
	{
		return;
	}

	int sw = 0;
	int sh = 0;
	GetDrawScreenSize(&sw, &sh);
	const float s = sw / BASE_WIDTH;

	// 終わりかけは薄くする
	float alpha = 1.0f;

	if (comboHold_ < 0.4f)
	{
		alpha = comboHold_ / 0.4f;
	}

	// 増えた瞬間は少し跳ねる
	const float pop = (comboPop_ > 0.0f) ? (comboPop_ / COMBO_POP_TIME) : 0.0f;

	const int right = (int)(sw - 80.0f * s);
	const int baseY = (int)(sh * 0.46f - pop * 14.0f * s);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(255.0f * alpha));

	DrawStringRight(right, baseY, "COMBO", GetColor(255, 170, 40), fontSmall_);

	char buf[16];
	std::snprintf(buf, sizeof(buf), "%d", shownCombo_);
	DrawStringRight(right, (int)(baseY + 20.0f * s), buf, GetColor(255, 255, 255), fontCombo_);

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

// 左下のコマンド一覧
//   通常 / 必殺技パレット(L1・TAB押し中) / フォームチェンジパレット(L2+R2押し中) の3ページ
//   パッドがつながっているときはコントローラーのボタンの絵、なければキーボードの文字を出す
//   使えないものは暗くなり、実行中の行は黄色く光る
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
		{ "LSHIFT", { PadIcon::CROSS },    1, false, "回避",     true, player.IsDodging() },
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
	};

	// フォームチェンジパレット
	const Command pageForm[] =
	{
		{ "G", { PadIcon::SQUARE }, 1, false,
			isSuper ? "元に戻る" : "変身",
			isSuper || ki >= CMD_TF_KI_COST, player.IsTransforming() },
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

// 敵に当たった位置に出るダメージ数字
void PlayerHud::DrawDamagePopups(void) const
{
	for (const auto& popup : popups_)
	{
		const VECTOR screen = ConvWorldPosToScreenPos(popup.pos);

		// 画面の外・カメラの後ろは描かない
		if (screen.z <= 0.0f || screen.z >= 1.0f)
		{
			continue;
		}

		float alpha = 1.0f;

		if (popup.timer < POPUP_FADE_TIME)
		{
			alpha = popup.timer / POPUP_FADE_TIME;
		}

		char buf[16];
		std::snprintf(buf, sizeof(buf), "%d", popup.value);

		const int w = GetDrawStringWidthToHandle(buf, (int)std::strlen(buf), fontDamage_);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(255.0f * alpha));
		DrawStringToHandle((int)(screen.x - w * 0.5f), (int)screen.y,
			buf, GetColor(255, 255, 255), fontDamage_);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	}
}