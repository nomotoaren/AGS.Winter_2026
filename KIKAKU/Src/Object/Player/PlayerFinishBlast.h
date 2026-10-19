#pragma once
#include <DxLib.h>

// プレイヤーのフィニッシュ気弾(気弾を連続で撃ったときに出る、大きくて強い気弾)
//   狙っている敵がいれば、少しだけ曲がって向かう。当たり判定とダメージは GameScene が行う
class PlayerFinishBlast
{
public:

	PlayerFinishBlast(void);
	~PlayerFinishBlast(void) = default;

	void Init(const VECTOR& pos, const VECTOR& dir);

	// aimPos: 狙っている位置(なければ nullptr)
	void Update(float deltaTime, const VECTOR* aimPos);
	void Draw(void) const;

	void Kill(void) { isDead_ = true; }
	bool IsDead(void) const { return isDead_; }
	const VECTOR& GetPos(void) const { return pos_; }
	const VECTOR& GetDir(void) const { return dir_; }
	float GetRadius(void) const;

private:

	VECTOR pos_;
	VECTOR dir_;
	float time_;
	bool isDead_;
};