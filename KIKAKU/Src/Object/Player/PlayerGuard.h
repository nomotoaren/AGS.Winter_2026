#pragma once

// プレイヤーのガード
//   ガード / ガードブレイク / ガードバースト と、ガード耐久値の回復をまとめて持つ
//   Player は入力と「始められる状態か」を渡し、返ってきた出来事に合わせてアニメを再生するだけ
class PlayerGuard
{
public:

	// Player から毎フレーム渡してもらう情報
	struct Context
	{
		float deltaTime;		// 1フレームの時間(秒)
		bool canStart;			// ガードを始められる状態か(攻撃中などは false)
		bool hold;				// ガードの入力を押しているか
		bool burstTrg;			// バーストの入力を押したか
	};

	// Update() の結果(このフレームに起きた出来事)
	struct Result
	{
		bool started = false;			// ガードを始めた
		bool released = false;			// ガードをやめた
		bool breakEnded = false;		// ガードブレイクが終わった
		bool burstEnded = false;		// バーストが終わった
		bool burstRequested = false;	// バーストを出したい(気が足りるか確認して StartBurst() を呼ぶ)
	};

	PlayerGuard(void);
	~PlayerGuard(void) = default;

	// 1フレーム更新する
	Result Update(const Context& ctx);

	// バーストを出す(気の消費は Player 側で済ませておく)
	void StartBurst(void);

	// ガード中に攻撃を受けた
	//   戻り値: ガードブレイクしたら true
	bool Damage(float damage);

	// 状態
	bool IsGuard(void) const { return isGuard_; }
	bool IsBreak(void) const { return isBreak_; }
	bool IsBurst(void) const { return isBurst_; }
	bool IsBurstTrigger(void) const { return burstTrigger_; }	// バーストを出したフレームだけ true
	bool IsBusy(void) const { return isGuard_ || isBreak_ || isBurst_; }

private:

	bool isGuard_;
	float hp_;					// ガード耐久値
	float recoverTimer_;		// 耐久値が回復し始めるまでの時間

	bool isBreak_;
	float breakTimer_;

	bool isBurst_;
	float burstTimer_;
	bool burstTrigger_;
};