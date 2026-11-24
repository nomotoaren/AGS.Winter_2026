#pragma once
#include <DxLib.h>

// 浮遊して移動するときの、脚のポーズ(プレイヤーと敵で共通)
//   移動アニメの上から、脚のボーンだけポーズへ寄せる
//   左脚: 太ももを前へ上げて膝を曲げる / 右脚: 少し開いて軽く曲げて垂らす
//   モデルは mixamo のボーン名(LeftUpLeg など)を持っていること
class FloatLegPose
{

public:

	// ポーズの角度(度)。数字はプレイヤーと同じ
	struct Params
	{
		float blendSpeed = 6.0f;		// ポーズへ寄せる速さ

		float lThigh = 45.0f;			// 左: 太ももを前へ上げる角度
		float lKnee = 100.0f;			// 左: 膝を曲げる角度
		float lOpen = 5.0f;				// 左: 横へ開く角度
		float rThigh = 0.0f;
		float rKnee = 40.0f;
		float rOpen = 18.0f;
		float foot = 30.0f;				// 足首

		float sideSpread = 12.0f;		// 横移動のとき、脚を流す角度
		float sway = 4.0f;				// ゆれの大きさ(度)

		// 向きが逆になったら -1
		float forwardSign = 1.0f;
		float sideSign = 1.0f;
		float openSign = 1.0f;
	};

	FloatLegPose(void) = default;
	~FloatLegPose(void) = default;

	// モデルのボーンを探し、回転軸を求める(モデルが変わったら呼び直す)
	void Init(int modelId);

	void SetParams(const Params& params) { params_ = params; }

	// 毎フレーム、アニメを更新したあとに呼ぶ
	//   wantPose: ポーズにするか(false なら、アニメへゆっくり戻る)
	//   side    : 横への移動の量(-1~1。キャラの右へ動くとき +)
	//   bobTime : ゆれの時間(増え続ける値。プレイヤーの floatBobTime_ と同じ)
	void Update(float deltaTime, bool wantPose, float side, float bobTime);

	// 腰の水平方向の移動を打ち消す(true: 打ち消す / false: アニメのまま)
	//   Update の前に呼ぶ
	void CancelRootMotion(bool cancel);

	// 0:アニメのまま ~ 1:完全にポーズ
	float GetRate(void) const { return rate_; }

private:

	enum LEG_FRAME
	{
		LEG_L_UP, LEG_L_KNEE, LEG_L_FOOT,
		LEG_R_UP, LEG_R_KNEE, LEG_R_FOOT,
		LEG_MAX
	};

	// mixamo のボーンを探す(名前の前に付く文字がモデルによって違うので、順に試す)
	static int FindMixamoFrame(int model, const char* bone);

	// 2つの行列を t(0~1) で混ぜる(向きを直交させ直し、大きさは別に混ぜる)
	static MATRIX BlendMatrix(const MATRIX& a, const MATRIX& b, float t);

	int modelId_ = -1;
	int hipsFrame_ = -1;

	int frames_[LEG_MAX] = { -1, -1, -1, -1, -1, -1 };
	VECTOR axisSide_[LEG_MAX] = {};		// 各ボーンから見た「キャラの左右の軸」(脚を前後に振る軸)
	VECTOR axisFront_[LEG_MAX] = {};	// 各ボーンから見た「キャラの前後の軸」(脚を横に開く軸)

	float rate_ = 0.0f;
	Params params_;

};