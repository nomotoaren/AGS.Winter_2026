#pragma once
#include <vector>
#include <DxLib.h>

// 連続エネルギー弾の弾(手元から、狙いの枠の中の点へ飛んで爆発する)
class PlayerBarrage
{
public:

	struct Shot
	{
		VECTOR start;	// 撃った位置
		VECTOR target;	// 着弾点
		VECTOR arc;		// 軌道の膨らみ(真ん中で最大になる)
		VECTOR pos;
		VECTOR dir;		// 進む向き(尾の向き)
		float age;		// 撃ってからの経過時間
		float flightTime;	// 着弾までにかける時間
		float life;		// 残り寿命
		bool exploded;	// 着弾したか
		bool handled;	// 着弾の処理(ダメージなど)を済ませたか
		bool dead;
	};

	PlayerBarrage(void);
	~PlayerBarrage(void);

	void Fire(const VECTOR& pos, const VECTOR& target, const VECTOR& arc);
	void Update(float deltaTime);
	void Draw(void) const;

	std::vector<Shot>& GetShots(void) { return shots_; }

	// 狙いの枠(画面の座標)。x, y = 中心、半径は RingRadius()
	static VECTOR RingCenter(void);
	static float RingRadius(void);

private:

	std::vector<Shot> shots_;
};