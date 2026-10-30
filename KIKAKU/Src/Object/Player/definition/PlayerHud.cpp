#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <DxLib.h>
#include "../../../Manager/SceneManager.h"
#include "../../../Manager/InputManager.h"
#include "../../../Manager/PadInput.h"
#include "../Player.h"
#include "../definition/PlayerConfig.h"
#include "../../Common/HudDraw.h"
#include "PlayerHud.h"

using HudDraw::DrawStringRight;
using HudDraw::HpLayerColor;
using HudDraw::DrawLayeredHp;

namespace
{
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

	// ダメージ数字
	constexpr float POPUP_TIME = 0.9f;
	constexpr float POPUP_RISE = 60.0f;			// 1秒あたりに上がる高さ(3D空間)
	constexpr float POPUP_FADE_TIME = 0.3f;

	// 三日月形の帯(アイコンの右側を、下から上へ満ちる)。ヒートアップのテンションゲージに使う
	//   rate  : 満ちている割合(0~1。下から)
	//   wobble: 炎のゆらぎの強さ(0なら動かない)
	void DrawCrescent(float cx, float cy, float r0, float thick, float rate, float time,
		unsigned int emptyColor, unsigned int fillColor, unsigned int tipColor, float wobble)
	{
		constexpr int SEGS = 40;
		constexpr float PI = 3.14159265f;
		const float a0 = 100.0f * PI / 180.0f;		// 下の端
		const float span = 200.0f * PI / 180.0f;	// 下から上まで

		for (int pass = 0; pass < 2; pass++)
		{
			for (int i = 0; i < SEGS; i++)
			{
				const float u0 = (float)i / SEGS;
				const float u1 = (float)(i + 1) / SEGS;

				// 満ちている範囲だけ(pass 1)。空の範囲は全体(pass 0)
				if (pass == 1 && u0 >= rate)
				{
					break;
				}

				float uu1 = u1;
				if (pass == 1 && uu1 > rate) { uu1 = rate; }

				const float th0 = a0 - span * u0;
				// 隣の区切りとの継ぎ目が見えないように、わずかに重ねる
				const float th1 = a0 - span * uu1 - 0.012f;

				// 三日月: 両端が細く、真ん中が太い
				auto width = [&](float u)
					{
						// u=1 のとき sinf が僅かに負になり、powf が NaN になって画面を横切る線が出ていた
						float sv = sinf(PI * u);
						if (sv < 0.0f) { sv = 0.0f; }
						float w = thick * powf(sv, 0.7f);
						if (wobble > 0.0f) { w *= 1.0f + wobble * sinf(time * 11.0f + u * 9.0f); }
						return w;
					};

				const float w0 = width(u0);
				const float w1 = width(uu1);

				const float x0i = cx + cosf(th0) * r0;
				const float y0i = cy + sinf(th0) * r0;
				const float x0o = cx + cosf(th0) * (r0 + w0);
				const float y0o = cy + sinf(th0) * (r0 + w0);
				const float x1i = cx + cosf(th1) * r0;
				const float y1i = cy + sinf(th1) * r0;
				const float x1o = cx + cosf(th1) * (r0 + w1);
				const float y1o = cy + sinf(th1) * (r0 + w1);

				const unsigned int col = (pass == 0) ? emptyColor : fillColor;

				DrawTriangleAA(x0i, y0i, x0o, y0o, x1o, y1o, col, TRUE);
				DrawTriangleAA(x0i, y0i, x1o, y1o, x1i, y1i, col, TRUE);

				// 満ちている部分は、外側を明るくする
				if (pass == 1)
				{
					const float xm0 = cx + cosf(th0) * (r0 + w0 * 0.55f);
					const float ym0 = cy + sinf(th0) * (r0 + w0 * 0.55f);
					const float xm1 = cx + cosf(th1) * (r0 + w1 * 0.55f);
					const float ym1 = cy + sinf(th1) * (r0 + w1 * 0.55f);

					DrawTriangleAA(xm0, ym0, x0o, y0o, x1o, y1o, tipColor, TRUE);
					DrawTriangleAA(xm0, ym0, x1o, y1o, xm1, ym1, tipColor, TRUE);
				}
			}
		}
	}

	// 右から左へ減るゲージ(rate: 現在値 0~1)
	void DrawGauge(float x, float y, float w, float h, float s,
		float rate, unsigned int color, unsigned int frameColor, float frameThick)
	{
		rate = std::clamp(rate, 0.0f, 1.0f);

		// 影
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
		DrawBoxAA(x - 3.0f * s, y - 3.0f * s, x + w + 3.0f * s, y + h + 3.0f * s,
			GetColor(10, 10, 14), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

		// 空の部分
		DrawBoxAA(x, y, x + w, y + h, GetColor(28, 30, 34), TRUE);

		const float right = x + w;

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
	palette_(0)
{
}

PlayerHud::~PlayerHud(void)
{
	DeleteFonts();
}

void PlayerHud::DeleteFonts(void)
{
	for (int* font : { &fontName_, &fontNum_, &fontIcon_, &fontSmall_, &fontCmd_, &fontCombo_, &fontDamage_ })
	{
		if (*font != -1)
		{
			DeleteFontToHandle(*font);
			*font = -1;
		}
	}
}

void PlayerHud::Init(void)
{
	// 2回呼ばれても、前のフォントが残らないようにする
	DeleteFonts();

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

void PlayerHud::AddDamagePopup(const VECTOR& worldPos, int damage, bool critical)
{
	DamagePopup popup{};
	popup.pos = worldPos;
	popup.value = damage;
	popup.timer = POPUP_TIME;
	popup.critical = critical;

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
	const bool tabHeld = InputManager::GetInstance().IsNew(KEY_INPUT_TAB);

	if (PadInput::IsFormPalette() || InputManager::GetInstance().IsNew(KEY_INPUT_H))
	{
		palette_ = 2;
	}
	else if (tabHeld || PadInput::IsSpecialPalette())
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

void PlayerHud::Draw(const Player& player)
{
	int sw = 0;
	int sh = 0;
	GetDrawScreenSize(&sw, &sh);
	const float s = sw / BASE_WIDTH;

	const float maxHp = (float)Player::MAX_HP;
	const float hp = (float)player.GetHp();

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

	// 重なる体力ゲージ(層ごとに色が違い、減った分は赤い)
	const int layerNum = DrawLayeredHp(barX, barY, barW, barH, s,
		hp, delayedHp_, maxHp, frameColor, frameThick);

	// 残りの層の数(「x4」)をバーの左に出す
	if (layerNum > 0)
	{
		char layerBuf[16];
		std::snprintf(layerBuf, sizeof(layerBuf), "x%d", layerNum);

		DrawStringRight((int)(barX - 8.0f * s), (int)(barY - 3.0f * s),
			layerBuf, HpLayerColor(layerNum - 1), fontNum_);
	}

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

	DrawGauge(kiX, kiY, kiW, kiH, s, kiRate, kiColor, GetColor(165, 135, 70), 2.0f * s);

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

	// ---- テンションゲージ(アイコンの右側の三日月。満タン+気力MAXで長押しするとヒートアップ) ----
	{
		const bool heat = player.IsHeatUp();
		const float tRate = std::clamp(player.GetTension() / Player::MAX_TENSION, 0.0f, 1.0f);
		const bool ready = player.CanHeatUp();

		unsigned int fill = GetColor(255, 125, 25);
		unsigned int tip = GetColor(255, 200, 70);
		float wobble = 0.0f;

		if (heat)
		{
			const float pulse = 0.5f + 0.5f * sinf(time_ * 12.0f);
			fill = GetColor(255, 215 + (int)(40.0f * pulse), 50 + (int)(80.0f * pulse));
			tip = GetColor(255, 250, 190);
			wobble = 0.12f;
		}
		else if (tRate >= 1.0f)
		{
			const float pulse = 0.5f + 0.5f * sinf(time_ * 8.0f);
			fill = GetColor(255, 200 + (int)(55.0f * pulse), 50);
			tip = GetColor(255, 245, 160);
			wobble = 0.06f;
		}

		const float r0 = iconR + 7.0f * s;
		const float thick = 24.0f * s;

		// 外側の影
		// (半透明だと重なりが濃く見えるので、ふつうの色で描く)
		DrawCrescent(iconX + 2.0f * s, iconY + 2.0f * s, r0, thick + 3.0f * s, 1.0f, time_,
			GetColor(22, 22, 28), GetColor(22, 22, 28), GetColor(22, 22, 28), 0.0f);

		DrawCrescent(iconX, iconY, r0, thick, tRate, time_,
			GetColor(55, 52, 28), fill, tip, wobble);

		// 発動できるときの案内
		if (ready)
		{
			const char* hint = PadInput::IsConnected() ? "△長押しでヒートアップ!" : "T長押しでヒートアップ!";
			const float blink = 0.6f + 0.4f * sinf(time_ * 8.0f);

			DrawStringRight((int)(iconX + iconR + 20.0f * s), (int)(iconY - iconR - 62.0f * s), hint,
				GetColor((int)(255 * blink), (int)(235 * blink), (int)(120 * blink)), fontSmall_);
		}
		else if (heat)
		{
			DrawStringRight((int)(iconX + iconR + 20.0f * s), (int)(iconY - iconR - 62.0f * s), "HEAT UP!",
				GetColor(255, 235, 120), fontSmall_);
		}

		// 長押しの進み具合(三日月の外側に白い帯)
		const float hold = player.GetHeatHoldRate();

		if (hold > 0.0f)
		{
			DrawCrescent(iconX, iconY, r0 + thick + 4.0f * s, 4.0f * s, hold, time_,
				GetColor(40, 40, 44), GetColor(240, 240, 245), GetColor(240, 240, 245), 0.0f);
		}
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
	DrawBattleHints(player);
	DrawDamagePopups();
}

// 追撃の案内と、相殺勝負のゲージ
void PlayerHud::DrawBattleHints(const Player& player) const
{
	int sw = 0;
	int sh = 0;
	GetDrawScreenSize(&sw, &sh);
	const float s = sw / BASE_WIDTH;

	// ---- 追撃できる間の案内 ----
	if (player.IsPursuitReady())
	{
		const float rate = std::clamp(player.GetPursuitRate(), 0.0f, 1.0f);
		const bool heat = player.IsHeatUp();

		char text[48];
		std::snprintf(text, sizeof(text), heat ? "追撃!!" : (PadInput::IsConnected() ? "追撃  [□]" : "追撃  [F]"));

		const int tw = GetDrawStringWidthToHandle(text, (int)std::strlen(text), fontName_);
		const float cx = sw * 0.5f;
		const float y = sh * 0.64f;

		const float pulse = 0.5f + 0.5f * sinf(time_ * 16.0f);

		DrawStringToHandle((int)(cx - tw * 0.5f), (int)y, text,
			GetColor(255, 220 + (int)(35.0f * pulse), 90), fontName_, GetColor(120, 40, 10));

		const float bw = 240.0f * s;
		const float by = y + 46.0f * s;

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
		DrawBoxAA(cx - bw * 0.5f - 2.0f * s, by - 2.0f * s, cx + bw * 0.5f + 2.0f * s, by + 8.0f * s,
			GetColor(10, 10, 14), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawBoxAA(cx - bw * 0.5f, by, cx - bw * 0.5f + bw * rate, by + 6.0f * s,
			GetColor(255, 200, 60), TRUE);
	}

	// ---- 相殺勝負のゲージ(画面上の中央。左が自分、右が敵) ----
	if (player.IsClash())
	{
		const float rate = std::clamp(player.GetClashRate(), 0.0f, 1.0f);

		const float w = 560.0f * s;
		const float h = 24.0f * s;
		const float x = sw * 0.5f - w * 0.5f;
		const float y = sh * 0.16f;

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 180);
		DrawBoxAA(x - 4.0f * s, y - 4.0f * s, x + w + 4.0f * s, y + h + 4.0f * s, GetColor(10, 10, 14), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

		DrawBoxAA(x, y, x + w, y + h, GetColor(170, 60, 230), TRUE);
		DrawBoxAA(x, y, x + w * rate, y + h, GetColor(70, 170, 255), TRUE);

		// 境目
		DrawBoxAA(x + w * rate - 3.0f * s, y - 6.0f * s, x + w * rate + 3.0f * s, y + h + 6.0f * s,
			GetColor(255, 255, 255), TRUE);

		DrawBoxAA(x, y, x + w, y + h, GetColor(225, 232, 245), FALSE, 2.0f * s);

		const char* text = PadInput::IsConnected() ? "相殺勝負!  □ ○ 連打!" : "相殺勝負!  F / U 連打!";
		const int tw = GetDrawStringWidthToHandle(text, (int)std::strlen(text), fontName_);
		const float pulse = 0.5f + 0.5f * sinf(time_ * 20.0f);

		DrawStringToHandle((int)(sw * 0.5f - tw * 0.5f), (int)(y - 44.0f * s), text,
			GetColor(255, 235 + (int)(20.0f * pulse), 120), fontName_, GetColor(60, 40, 120));
	}
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

		if (popup.critical)
		{
			// クリティカルは、金色で大きく、黒い縁をつける
			const float ex = 1.6f;
			const int x = (int)(screen.x - w * ex * 0.5f);
			const int y = (int)(screen.y - 10);

			// 黒い縁(上下左右にずらして先に描く)
			DrawExtendStringToHandle(x + 2, y, ex, ex, buf, GetColor(0, 0, 0), fontDamage_);
			DrawExtendStringToHandle(x - 2, y, ex, ex, buf, GetColor(0, 0, 0), fontDamage_);
			DrawExtendStringToHandle(x, y + 2, ex, ex, buf, GetColor(0, 0, 0), fontDamage_);
			DrawExtendStringToHandle(x, y - 2, ex, ex, buf, GetColor(0, 0, 0), fontDamage_);
			DrawExtendStringToHandle(x, y, ex, ex, buf, GetColor(255, 220, 60), fontDamage_);
		}
		else
		{
			DrawStringToHandle((int)(screen.x - w * 0.5f), (int)screen.y,
				buf, GetColor(255, 255, 255), fontDamage_);
		}

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	}
}