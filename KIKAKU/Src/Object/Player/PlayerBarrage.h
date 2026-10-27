#pragma once
#include <vector>
#include <DxLib.h>

// 連続エネルギー弾(構え -> 左右の手から交互に連射 -> 終わり)
//   ・枠の中心(エイムポイント)は3Dの点。最初は敵の中心で、スティックで少しずらせる
//   ・弾は枠の中のランダムな点へ飛んで、着弾したら GameScene がダメージを与える
//   ・弾の管理も、このクラスの中で全部やる(Player は Start / Update / Draw / Cancel を呼ぶだけ)
class PlayerBarrage
{
public:

	enum class STATE
	{
		IDLE,		// 使っていない
		CHARGE,		// 構え(撃ち始めるまで)
		FIRE,		// 連射中
		END			// 撃ち終わって、動けるようになるまで
	};

	// 毎フレーム Player から渡す情報
	struct Context
	{
		float deltaTime;
		VECTOR playerPos;
		VECTOR forward;		// プレイヤーの正面
		bool hasTarget;		// ロックオンしている敵がいるか
		VECTOR targetPos;	// その敵の足元
		float stickX;		// 狙いのずらし操作(-1.0 ~ 1.0)。Y は上がプラス
		float stickY;
	};

	struct Shot
	{
		VECTOR start;		// 撃った位置(手元)
		VECTOR offset;		// 枠の中心からのずれ(3D)
		VECTOR arc;			// 軌道の膨らみ(真ん中で最大になる)
		VECTOR pos;
		VECTOR dir;			// 進む向き(尾の向き)
		float age;			// 撃ってからの経過時間
		float flightTime;	// 着弾までにかける時間
		float life;			// 残り寿命
		bool exploded;		// 着弾したか
		bool handled;		// 着弾の処理(ダメージなど)を済ませたか
		bool dead;
	};

	PlayerBarrage(void);
	~PlayerBarrage(void);

	// モデルの手のボーンを探す(Player の初期化で1回)
	void SetModel(int modelId);

	void Start(void);

	// 毎フレーム。連射が終わったフレームだけ true を返す
	bool Update(const Context& ctx);

	// 被弾などで構えを中断する(飛んでいる弾は、そのまま飛んでいく)
	void Cancel(void);

	void Draw(void) const;

	bool IsActive(void) const { return state_ != STATE::IDLE; }

	// 1発撃つたびに1回だけ true(腕を突き出すアニメを出し直すのに使う)
	bool ConsumeFired(void);

	// 枠の中心(3D)
	VECTOR GetAimPoint(void) const { return aimPoint_; }

	// 枠を動かした分だけカメラも向きを変える(ラジアン)
	float GetAimYaw(void) const { return aimYaw_; }
	float GetAimPitch(void) const { return aimPitch_; }

	// 枠の半径(3D)。この中にいる敵に当たる
	static float RingRadius(void);

	std::vector<Shot>& GetShots(void) { return shots_; }

private:

	STATE state_;
	float timer_;
	int firedCount_;
	bool firedFlag_;

	int modelId_;
	int rightHandFrame_;
	int leftHandFrame_;

	// 枠
	VECTOR aimPoint_;
	float aimOffsetX_;	// カメラの右方向へのずらし
	float aimOffsetY_;	// カメラの上方向へのずらし
	float aimYaw_;
	float aimPitch_;

	std::vector<Shot> shots_;

	void UpdateAim(const Context& ctx);
	void FireShot(const Context& ctx);
	void UpdateShots(float deltaTime);
	VECTOR GetHandPos(int index, const VECTOR& fallback) const;
};
