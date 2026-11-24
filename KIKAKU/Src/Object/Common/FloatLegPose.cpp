#include <cmath>
#include <string>
#include "FloatLegPose.h"

namespace
{
	constexpr float kEpsilon = 0.001f;
	constexpr float kDeg2Rad = DX_PI_F / 180.0f;

	inline float Rad(float deg)
	{
		return deg * kDeg2Rad;
	}

	// 長さがほぼ 0 のときは (0,0,0) を返す正規化
	inline VECTOR NormalizeSafe(const VECTOR& v)
	{
		return (VSize(v) > kEpsilon) ? VNorm(v) : VGet(0.0f, 0.0f, 0.0f);
	}
}

// mixamo のボーンを探す
int FloatLegPose::FindMixamoFrame(int model, const char* bone)
{
	const char* prefixes[] = { "mixamorig:", "mixamorig1:", "mixamorig2:", "" };

	for (const char* p : prefixes)
	{
		const std::string name = std::string(p) + bone;
		const int frame = MV1SearchFrame(model, name.c_str());

		if (frame >= 0)
		{
			return frame;
		}
	}

	return -1;
}

// 2つの行列を t(0~1) で混ぜる
//   単純に混ぜると縮んだりゆがんだりするので、向きを直交させ直し、大きさは別に混ぜる
MATRIX FloatLegPose::BlendMatrix(const MATRIX& a, const MATRIX& b, float t)
{
	MATRIX r{};
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			r.m[i][j] = a.m[i][j] + (b.m[i][j] - a.m[i][j]) * t;
		}
	}

	auto row = [](const MATRIX& m, int i) { return VGet(m.m[i][0], m.m[i][1], m.m[i][2]); };

	// 各軸の大きさ(スケール)は別に混ぜる
	const float sx = VSize(row(a, 0)) + (VSize(row(b, 0)) - VSize(row(a, 0))) * t;
	const float sy = VSize(row(a, 1)) + (VSize(row(b, 1)) - VSize(row(a, 1))) * t;
	const float sz = VSize(row(a, 2)) + (VSize(row(b, 2)) - VSize(row(a, 2))) * t;

	VECTOR x = row(r, 0);
	VECTOR y = row(r, 1);

	if (VSize(x) < 0.0001f || VSize(y) < 0.0001f)
	{
		return (t < 0.5f) ? a : b;
	}

	x = VNorm(x);
	VECTOR z = VCross(x, y);
	if (VSize(z) < 0.0001f)
	{
		return (t < 0.5f) ? a : b;
	}
	z = VNorm(z);
	y = VCross(z, x);

	r.m[0][0] = x.x * sx; r.m[0][1] = x.y * sx; r.m[0][2] = x.z * sx; r.m[0][3] = 0.0f;
	r.m[1][0] = y.x * sy; r.m[1][1] = y.y * sy; r.m[1][2] = y.z * sy; r.m[1][3] = 0.0f;
	r.m[2][0] = z.x * sz; r.m[2][1] = z.y * sz; r.m[2][2] = z.z * sz; r.m[2][3] = 0.0f;
	r.m[3][3] = 1.0f;

	return r;
}

// 脚のボーンを探し、回転軸を求める
void FloatLegPose::Init(int modelId)
{
	modelId_ = modelId;
	rate_ = 0.0f;

	hipsFrame_ = FindMixamoFrame(modelId_, "Hips");

	frames_[LEG_L_UP] = FindMixamoFrame(modelId_, "LeftUpLeg");
	frames_[LEG_L_KNEE] = FindMixamoFrame(modelId_, "LeftLeg");
	frames_[LEG_L_FOOT] = FindMixamoFrame(modelId_, "LeftFoot");
	frames_[LEG_R_UP] = FindMixamoFrame(modelId_, "RightUpLeg");
	frames_[LEG_R_KNEE] = FindMixamoFrame(modelId_, "RightLeg");
	frames_[LEG_R_FOOT] = FindMixamoFrame(modelId_, "RightFoot");

	// ボーンからみた回転軸を求める
	for (int i = 0; i < LEG_MAX; i++)
	{
		const int frame = frames_[i];
		axisSide_[i] = VGet(1.0f, 0.0f, 0.0f);
		axisFront_[i] = VGet(0.0f, 0.0f, 1.0f);

		if (frame < 0)
		{
			continue;
		}

		// ボーンからモデルのローカル座標系への変換行列を求める
		MATRIX toModel = MV1GetFrameBaseLocalMatrix(modelId_, frame);
		for (int p = MV1GetFrameParent(modelId_, frame); p >= 0; p = MV1GetFrameParent(modelId_, p))
		{
			toModel = MMult(toModel, MV1GetFrameBaseLocalMatrix(modelId_, p));
		}

		const MATRIX toLocal = MInverse(toModel);
		axisSide_[i] = NormalizeSafe(VTransformSR(VGet(1.0f, 0.0f, 0.0f), toLocal));
		axisFront_[i] = NormalizeSafe(VTransformSR(VGet(0.0f, 0.0f, 1.0f), toLocal));
	}
}

// 腰の水平方向の移動を打ち消す
void FloatLegPose::CancelRootMotion(bool cancel)
{
	if (modelId_ < 0 || hipsFrame_ < 0)
	{
		return;
	}

	MV1ResetFrameUserLocalMatrix(modelId_, hipsFrame_);

	if (!cancel)
	{
		return;
	}

	MATRIX m = MV1GetFrameLocalMatrix(modelId_, hipsFrame_);
	const MATRIX base = MV1GetFrameBaseLocalMatrix(modelId_, hipsFrame_);

	// 水平(X・Z)だけ、元の位置に戻す
	m.m[3][0] = base.m[3][0];
	m.m[3][2] = base.m[3][2];

	MV1SetFrameUserLocalMatrix(modelId_, hipsFrame_, m);
}

// 脚のポーズを反映する
void FloatLegPose::Update(float deltaTime, bool wantPose, float side, float bobTime)
{
	if (modelId_ < 0)
	{
		return;
	}

	const Params& p = params_;

	// ポーズへ近づける
	const float target = wantPose ? 1.0f : 0.0f;

	float k = p.blendSpeed * deltaTime;
	if (k > 1.0f) { k = 1.0f; }
	rate_ += (target - rate_) * k;

	// ほぼポーズなしなら、アニメのままにする
	if (rate_ < 0.01f)
	{
		rate_ = 0.0f;

		for (int frame : frames_)
		{
			if (frame >= 0)
			{
				MV1ResetFrameUserLocalMatrix(modelId_, frame);
			}
		}
		return;
	}

	// 横移動の量
	if (side > 1.0f) { side = 1.0f; }
	if (side < -1.0f) { side = -1.0f; }

	const float sway = sinf(bobTime * 0.7f) * p.sway;
	const float spread = -side * p.sideSpread * p.sideSign;

	// アニメの姿勢とポーズを、rate_ で混ぜる
	auto apply = [&](int frame, const MATRIX& rot)
		{
			if (frame < 0)
			{
				return;
			}

			// アニメの姿勢
			MV1ResetFrameUserLocalMatrix(modelId_, frame);
			const MATRIX anim = MV1GetFrameLocalMatrix(modelId_, frame);

			// ポーズ: 元の姿勢から回したもの
			const MATRIX base = MV1GetFrameBaseLocalMatrix(modelId_, frame);
			const MATRIX pose = MMult(rot, base);

			MV1SetFrameUserLocalMatrix(modelId_, frame, BlendMatrix(anim, pose, rate_));
		};

	// swing: 脚を前後に振る
	// spreadRot: 脚を横に開く
	auto swing = [&](int leg, float deg)
		{
			return MGetRotAxis(axisSide_[leg], Rad(deg) * p.forwardSign);
		};
	auto spreadRot = [&](int leg, float deg)
		{
			return MGetRotAxis(axisFront_[leg], Rad(deg));
		};

	const float openL = p.lOpen * p.openSign;
	const float openR = -p.rOpen * p.openSign;

	// 左脚: 太ももを前へ上げ、膝を深く曲げる
	apply(frames_[LEG_L_UP], MMult(swing(LEG_L_UP, p.lThigh + sway), spreadRot(LEG_L_UP, spread + openL)));
	apply(frames_[LEG_L_KNEE], swing(LEG_L_KNEE, -(p.lKnee - sway)));
	apply(frames_[LEG_L_FOOT], swing(LEG_L_FOOT, -p.foot));

	// 右脚
	apply(frames_[LEG_R_UP], MMult(swing(LEG_R_UP, p.rThigh - sway), spreadRot(LEG_R_UP, spread + openR)));
	apply(frames_[LEG_R_KNEE], swing(LEG_R_KNEE, -p.rKnee + sway));
	apply(frames_[LEG_R_FOOT], swing(LEG_R_FOOT, -p.foot));
}