#pragma once
#include <DxLib.h>

// プレイヤーの変身の流れ
//   溜めモーション → 途中でモデル差し替え → モーション終了 の進行と、
//   気溜め・爆発(雷)のエフェクトをまとめて持つ
//   モデルの差し替えやアニメの再生は Player の仕事なので、
//   Update() が「今フレームに何をしてほしいか」を結果で返す
class PlayerTransform
{
public:

	// Update() の結果
	struct Result
	{
		bool swap = false;			// このフレームでモデルを差し替えてほしい
		float step = 0.0f;			// swap のとき: 溜めモーションの再生位置(フレーム)。続きから流す
		bool finished = false;		// このフレームで変身が終わった
	};

	PlayerTransform(void);
	~PlayerTransform(void) = default;

	// 変身を始める(気が足りるかなどは Player 側で確認しておく)
	//   toSuper: true なら変身する、false なら元に戻る
	void Start(bool toSuper, const VECTOR& playerPos);

	// 1フレーム更新する
	//   animEnd: 溜めモーションが最後まで流れたか(Player のアニメ管理から渡す)
	Result Update(float deltaTime, const VECTOR& playerPos, bool animEnd);

	// 状態
	bool IsActive(void) const { return isActive_; }
	bool IsToSuper(void) const { return toSuper_; }		// 変身する側か(元に戻る側ではない)

private:

	bool isActive_;
	bool toSuper_;
	bool isSwapped_;		// モデルを差し替えたか
	bool isBurstPlayed_;	// 爆発(雷)を出したか
	float timer_;
};