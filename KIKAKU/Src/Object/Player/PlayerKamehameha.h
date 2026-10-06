#pragma once
#include <DxLib.h>

// プレイヤーのかめはめ波
//   構え(溜め) → 発射 → 終了 の流れと、手元の気弾モデル・エフェクト・ライトをまとめて持つ
//   Player は「撃つかどうか」を決めて Start() を呼び、毎フレーム Update() / Draw() を呼ぶだけ
class PlayerKamehameha
{
public:

	// Player から毎フレーム渡してもらう情報
	struct Context
	{
		VECTOR playerPos;		// プレイヤーの足元の位置
		VECTOR forward;			// プレイヤーの正面(水平)
		bool hasTarget;			// ロックオン中で、狙える敵がいるか
		VECTOR targetPos;		// 狙う敵の足元の位置
		float deltaTime;		// 1フレームの時間(秒)
	};

	PlayerKamehameha(void);
	~PlayerKamehameha(void) = default;

	// 気弾のモデル・ビームのモデル・ライトを用意する
	void Init(const VECTOR& playerPos);

	// プレイヤーのモデルが変わったら呼ぶ(手のフレームを探し直す)
	void SetModel(int modelId);

	// 撃ち始める(気が足りるか・撃てる状態かは Player 側で確認しておく)
	void Start(const Context& ctx);

	// 1フレーム更新する
	//   faceAim : 狙っている向き。プレイヤーをこの向きへ向けてほしいときに入る(なければ長さ0)
	//   戻り値  : かめはめ波が終わったフレームで true
	bool Update(const Context& ctx, VECTOR& faceAim);

	// 途中でやめる(被弾したときなど)
	void Cancel(void);

	// 描画(3D)
	void Draw(const Context& ctx);

	// 状態
	bool IsActive(void) const { return isActive_; }		// 構え?発射の間
	bool IsBeam(void) const { return isBeam_; }			// ビームが出ている間
	VECTOR GetDir(void) const { return dir_; }			// 発射の向き(敵側の判定用)

	// ビームの判定用
	VECTOR GetStartPos(void) const;
	VECTOR GetEndPos(const VECTOR& defaultForward) const;
	float GetRadius(void) const;

private:

	// 手元の気弾モデルを描く(rotSpeed: 回転の速さ)
	void DrawChargeModel(const VECTOR& pos, float scale, float rotSpeed) const;

	// 発光ライト
	void UpdateLight(const Context& ctx) const;
	void SetLight(bool enable) const;

	// エフェクトの再生速度を、実際の時間に合わせる
	void UpdateEffectSpeed(void);

	bool isActive_;
	bool isBeam_;
	float timer_;
	VECTOR dir_;

	int modelId_;			// プレイヤーのモデル(手の位置を取る)
	int leftHandFrame_;
	int rightHandFrame_;

	int lightHandle_;
	int chargeModel_;
	int beamModel_;

	// エフェクト速度用: 前回の時刻(マイクロ秒)
	LONGLONG lastCount_;
};