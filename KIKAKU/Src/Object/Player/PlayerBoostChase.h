#pragma once
#include <DxLib.h>

// プレイヤーの高速接近(ロックオン中にSPACE / L3)
//   敵へまっすぐではなく、横へ振ってから巻き込むように弧を描いて近づく
//   Player は「始めるかどうか」を決めて Start() を呼び、毎フレーム Update() の結果を反映するだけ
class PlayerBoostChase
{
public:

	// Player から渡してもらう情報
	struct Context
	{
		VECTOR playerPos;		// プレイヤーの位置
		VECTOR targetPos;		// 敵の位置
		bool hasTarget;			// ロックオン中で、狙える敵がいるか
		bool interrupted;		// 攻撃などで中断されたか
		float deltaTime;		// 1フレームの時間(秒)
	};

	// Update() の結果
	struct Result
	{
		VECTOR move = { 0.0f, 0.0f, 0.0f };		// このフレームの移動量
		VECTOR faceDir = { 0.0f, 0.0f, 0.0f };	// 向いてほしい向き(長さ0なら変えない)
		bool leaveAfterImage = false;			// 残像を残してほしい
		bool finished = false;					// このフレームで終わった
	};

	PlayerBoostChase(void);
	~PlayerBoostChase(void) = default;

	// 毎フレーム呼ぶ(再使用までの待ち時間を進める)
	void Tick(float deltaTime);

	// 始められるか(待ち時間が終わっているか)
	bool CanStart(void) const { return !isActive_ && cooldown_ <= 0.0f; }

	// 始める
	//   sideInput: 弧を膨らませる側。+1:右 -1:左 0:指定なし(前回と逆にする)
	void Start(const Context& ctx, int sideInput);

	// 1フレーム更新する
	Result Update(const Context& ctx);

	// 途中でやめる(被弾したときなど。待ち時間は付けない)
	void Cancel(void);

	bool IsActive(void) const { return isActive_; }

private:

	// 終わる(待ち時間を付ける)
	Result Finish(void);

	bool isActive_;
	float timer_;
	VECTOR dir_;			// 進行方向(毎フレーム敵へ曲げていく)
	float side_;			// 膨らむ側(+1:右 -1:左。毎回入れ替える)
	float cooldown_;		// 再使用までの待ち時間
	float afterImageTimer_;	// 次の残像までの時間
};