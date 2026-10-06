#pragma once
#include <DxLib.h>

// プレイヤーの近接攻撃
//   ・追撃(Fキーで敵へ飛びこむ)
//   ・コンボ(1~8段目の進行、ヒット判定の時間、敵への吸い付き、瞬間移動の位置)
//   の「状態と判断」をまとめて持つ
//   アニメの再生や実際の移動は Player の仕事なので、Update 系は「どうしてほしいか」を結果で返す
class PlayerAttack
{
public:

	// Player から毎フレーム渡してもらう情報
	struct Context
	{
		VECTOR playerPos;		// プレイヤーの位置
		VECTOR targetPos;		// 敵の位置
		bool hasTarget;			// 狙える敵がいるか
		float deltaTime;		// 1フレームの時間(秒)
	};

	//------------------------------------------------------------
	// 追撃
	//------------------------------------------------------------

	// 追撃の終わり方
	enum class ChaseEnd
	{
		NONE,		// まだ続いている
		CANCEL,		// 中断(攻撃しない)
		ATTACK,		// 届いた → 敵の前に位置を合わせて攻撃へ
		LUNGE,		// 飛びこむ時間を使い切った → その場で攻撃(空振りになる)
	};

	struct ChaseResult
	{
		VECTOR move = { 0.0f, 0.0f, 0.0f };		// このフレームの移動量
		VECTOR faceDir = { 0.0f, 0.0f, 0.0f };	// 向いてほしい向き(長さ0なら変えない)
		ChaseEnd end = ChaseEnd::NONE;
	};

	//------------------------------------------------------------
	// コンボ
	//------------------------------------------------------------

	// 攻撃中の1フレームの結果
	struct ComboResult
	{
		bool hasFollow = false;					// 敵へ吸い付く移動がある
		VECTOR follow = { 0.0f, 0.0f, 0.0f };	// 吸い付く移動量
		VECTOR faceDir = { 0.0f, 0.0f, 0.0f };	// 向いてほしい向き(水平。長さ0なら変えない)
		bool advance = false;					// 次の段へ進んでほしい
		bool finish = false;					// コンボを終わらせてほしい
	};

	// 次の段へ進むときにしてほしいこと
	struct ComboStep
	{
		int combo = 0;					// 進んだ後の段(2~8)
		bool warp = false;				// 瞬間移動してほしい
		VECTOR warpPos = { 0.0f, 0.0f, 0.0f };
		bool setAfterImagePos = false;	// 瞬間移動前の位置を残像の位置として記録してほしい(4段目)
		bool lookPitch = false;			// 敵を見るときに上下の傾きも付けてほしい(8段目)
	};

	PlayerAttack(void);
	~PlayerAttack(void) = default;

	// 毎フレームの最初に呼ぶ(「攻撃を開始したフレーム」の印を消す)
	void BeginFrame(void) { trigger_ = false; }

	// 追撃
	void StartChase(void);
	ChaseResult UpdateChase(const Context& ctx);
	void CancelChase(void);

	// コンボ
	void StartAttack(void);				// 1段目から始める
	void RequestNext(void) { nextRequested_ = true; }	// 次の段の先行入力
	// playRate: 今のモーションの再生割合(0~1) / animEnd: モーションが終わったか
	ComboResult UpdateCombo(const Context& ctx, float playRate, bool animEnd);
	ComboStep AdvanceCombo(const Context& ctx);		// 次の段へ進む
	void Reset(void);					// 攻撃をやめる(終了・被弾など)

	// 状態
	bool IsChasing(void) const { return isChasing_; }
	bool IsAttack(void) const { return isAttack_; }
	int GetCombo(void) const { return combo_; }
	bool IsTrigger(void) const { return trigger_; }

	// 攻撃判定
	bool IsHitTiming(void) const;
	bool HasHit(void) const { return hasHit_; }
	void SetHit(void) { hasHit_ = true; }

	// 追撃が敵に届いたとき、敵の手前に合わせる位置(水平)。合わせられなければ false
	static bool CalcSnapPos(const Context& ctx, VECTOR& outPos);

private:

	// 攻撃中の、敵への向きと吸い付き
	void CalcFollow(const Context& ctx, ComboResult& result) const;

	// 追撃を終える
	void EndChase(void);

	// 追撃
	bool isChasing_;
	float chaseTimer_;			// 追撃を始めてからの時間
	float chaseStuckTimer_;		// 前に進めていない時間
	float chaseLastDistance_;	// 前フレームの敵との距離

	// コンボ
	bool isAttack_;
	int combo_;
	bool nextRequested_;
	float attackTimer_;			// 今の段が始まってからの時間
	bool hasHit_;				// 今の段が既にヒットしたか
	bool trigger_;				// このフレームで攻撃(段)を開始したか
};