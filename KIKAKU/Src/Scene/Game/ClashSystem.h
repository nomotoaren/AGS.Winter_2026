#pragma once
#include <memory>
#include <vector>
#include <DxLib.h>
class Player;
class EnemyBase;
class MeleeEnemy;

// 相殺勝負
//   かめはめ波が、敵の「大きな気弾」や「ビーム」にぶつかったとき、
//   連打で押し合う勝負を管理する。
//   ぶつかっている光の位置は、優劣に応じて自分と敵の間を動く。
//
// GameScene との関わり
//   ・毎フレーム Update を呼ぶ(決着がついたフレームだけ WIN / LOSE が返る)
//   ・描画は Draw を呼ぶ(3D空間に光線と光の球を描く)
//   ・勝負中かどうかは IsActive で調べる(勝負中は通常のかめはめ波ダメージを止める)
class ClashSystem
{

public:

	// 今フレームの結果
	enum class RESULT
	{
		NONE,	// 勝負なし / 勝負中
		WIN,	// 押し勝った
		LOSE	// 押し負けた
	};

	// 押し勝ったときのヒットストップの長さ(秒)
	static constexpr float WIN_HIT_STOP = 0.15f;

	ClashSystem(void);
	~ClashSystem(void);

	// 状態を初期状態に戻す(ステージ切り替え・シーン開始用。敵には触らない)
	void Reset(void);

	// 更新
	RESULT Update(float dt, Player& player, std::vector<std::unique_ptr<EnemyBase>>& enemies);

	// 描画
	void Draw(const Player& player) const;

	// 勝負中か
	bool IsActive(void) const { return state_.active; }

private:

	// 勝負の状態
	struct State
	{
		bool active = false;
		MeleeEnemy* enemy = nullptr;
		VECTOR pos = { 0.0f, 0.0f, 0.0f };			// ぶつかっている光の位置
		float rate = 0.5f;							// 優劣(0=負け 1=勝ち)
		float timer = 0.0f;
		bool isBeam = false;						// 相手は大きな気弾ではなく、ビーム
		VECTOR enemySide = { 0.0f, 0.0f, 0.0f };	// 敵側の光線の出どころ
	};

	// ぶつかった場所の情報
	struct Hit
	{
		bool isBeam = false;
		VECTOR pos = { 0.0f, 0.0f, 0.0f };
		VECTOR enemySide = { 0.0f, 0.0f, 0.0f };
	};

	State state_;

	// --- 勝負の開始 ---
	// かめはめ波が、敵の気弾かビームにぶつかったか調べる
	void TryStart(Player& player, std::vector<std::unique_ptr<EnemyBase>>& enemies);
	bool FindBigBlastHit(const Player& player, MeleeEnemy& enemy, const VECTOR& beamStart, const VECTOR& beamEnd, Hit& hit) const;
	bool FindBeamHit(const Player& player, MeleeEnemy& enemy, const VECTOR& beamStart, const VECTOR& beamEnd, Hit& hit) const;
	void Begin(Player& player, MeleeEnemy& enemy, const Hit& hit);

	// --- 勝負中 ---
	RESULT UpdateActive(float dt, Player& player);
	void UpdateRate(float dt, const Player& player);
	void MoveLight(float dt, const Player& player);
	void Cancel(Player& player, bool alive);
	void Finish(bool win, int enemyDamage, Player& player);

	// 押し勝ち / 押し負けの処理
	void ApplyWin(MeleeEnemy& enemy, const VECTOR& dir, Player& player);
	void ApplyLose(const VECTOR& dir, int enemyDamage, Player& player);

};