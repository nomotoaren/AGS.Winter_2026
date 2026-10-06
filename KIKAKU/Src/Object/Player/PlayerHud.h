#pragma once
#include <string>
#include <vector>
#include <DxLib.h>

class Player;

// プレイヤーのHUD(ドラゴンボール カカロット風)
//   右下 : 名前 / HP / 気 / アイコン
//   右上 : バトル条件(敵のHPバーの下)
//   右中 : COMBO 表示
//   左下 : コマンド一覧
//   ダメージ数字は AddDamagePopup() で出す
// 使い方:
//   Init() を1回、毎フレーム Update(player)、3D描画のあとに Draw(player)
class PlayerHud
{
public:

	PlayerHud(void);
	~PlayerHud(void);

	void Init(void);
	void Update(const Player& player);
	void Draw(const Player& player);

	// 名前とアイコンに出す文字
	void SetName(const std::string& name);
	void SetIconText(const std::string& text);

	// 画面右上に出す「バトル条件」の文字
	void SetObjective(const std::string& text);

	// ダメージ数字を出す(worldPos = 3D空間の位置)
	void AddDamagePopup(const VECTOR& worldPos, int damage);

private:

	struct DamagePopup
	{
		VECTOR pos;
		int value;
		float timer;
	};

	void DrawObjective(void) const;
	void DrawCombo(void) const;
	void DrawCommandPanel(const Player& player) const;
	void DrawDamagePopups(void) const;

	// 残量に応じたHPの色
	unsigned int GetHpColor(float rate) const;

	int fontName_;
	int fontNum_;
	int fontIcon_;
	int fontSmall_;
	int fontCmd_;
	int fontCombo_;
	int fontDamage_;

	std::string name_;
	std::string iconText_;
	std::string objective_;

	float prevHp_;		// 前フレームのHP
	float delayedHp_;	// 遅れて減るバーのHP
	float delayWait_;	// 減り始めるまでの待ち時間
	float flashTimer_;	// 被弾フラッシュ
	float time_;		// ピンチ点滅用

	int shownCombo_;	// 表示中のコンボ数
	float comboHold_;	// コンボが終わったあと表示を残す時間
	float comboPop_;	// コンボが増えた瞬間の演出

	int palette_;		// コマンド一覧のページ(0=通常 1=必殺技パレット 2=フォームチェンジ)
	bool tabHeld_;		// TABキーを押しているか

	std::vector<DamagePopup> popups_;
};