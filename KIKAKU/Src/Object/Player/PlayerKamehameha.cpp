#include <cmath>
#include "../../Manager/ResourceManager.h"
#include "../../Manager/EffekseerEffect.h"
#include "../../Utility/AsoUtility.h"
#include "PlayerConfig.h"
#include "PlayerKamehameha.h"

namespace cfg = PlayerConfig;

namespace
{
	constexpr float EPSILON = 0.001f;

	VECTOR NormalizeSafe(const VECTOR& v)
	{
		return (VSize(v) > EPSILON) ? VNorm(v) : VGet(0.0f, 0.0f, 0.0f);
	}
}

PlayerKamehameha::PlayerKamehameha(void)
	:
	isActive_(false),
	isBeam_(false),
	timer_(0.0f),
	dir_(AsoUtility::VECTOR_ZERO),
	modelId_(-1),
	leftHandFrame_(-1),
	rightHandFrame_(-1),
	lightHandle_(-1),
	chargeModel_(-1),
	beamModel_(-1),
	lastCount_(0)
{
}

void PlayerKamehameha::Init(const VECTOR& playerPos)
{
	chargeModel_ =
		ResourceManager::GetInstance().LoadModelDuplicate(ResourceManager::SRC::CHARGE);

	beamModel_ =
		ResourceManager::GetInstance().LoadModelDuplicate(ResourceManager::SRC::KAMEHAMEHA);

	// 発光ライト
	lightHandle_ = CreatePointLightHandle(playerPos, 700.0f, 0.0f, 0.003f, 0.0f);
	SetLightEnableHandle(lightHandle_, false);
}

void PlayerKamehameha::SetModel(int modelId)
{
	modelId_ = modelId;
	leftHandFrame_ = MV1SearchFrame(modelId_, "mixamorig:LeftHand");
	rightHandFrame_ = MV1SearchFrame(modelId_, "mixamorig:RightHand");
}

void PlayerKamehameha::Start(const Context& ctx)
{
	isActive_ = true;
	isBeam_ = false;
	timer_ = 0.0f;

	lastCount_ = GetNowHiPerformanceCount();

	// エフェクト(再生速度は、毎フレーム Update で時間に合わせて決める)
	EffekseerEffect::GetInstance()->PlayKamehameEffect(
		GetStartPos(), ctx.forward, cfg::Kamehame::EFFECT_SCALE, 1.0f);
}

bool PlayerKamehameha::Update(const Context& ctx, VECTOR& faceAim)
{
	faceAim = AsoUtility::VECTOR_ZERO;

	if (!isActive_)
	{
		return false;
	}

	timer_ += ctx.deltaTime;

	UpdateEffectSpeed();

	// チャージ中は敵を追従して照準を合わせる
	if (!isBeam_ && ctx.hasTarget)
	{
		// 足元ではなく敵の中心(胸あたり)を狙う
		VECTOR aimPos = ctx.targetPos;
		aimPos.y += cfg::Kamehame::ENEMY_CENTER_HEIGHT;

		const VECTOR aim = VSub(aimPos, GetStartPos());

		if (VSize(aim) > EPSILON)
		{
			dir_ = VNorm(aim);
			faceAim = aim;		// 体も敵の方へ向けてもらう(水平のみ)
		}
	}

	// ビーム発射(この時点の dir_ で固定される)
	if (!isBeam_ && timer_ >= cfg::Kamehame::SHOT_TIME)
	{
		if (VSize(dir_) <= EPSILON)
		{
			dir_ = ctx.forward;
		}

		isBeam_ = true;

		// ビームを当たり判定の長さまで伸ばす(この向きと位置で固定)
		EffekseerEffect::GetInstance()->UpdateKamehameEffect(
			VAdd(GetStartPos(), VScale(dir_, cfg::Kamehame::EFFECT_FORWARD)), dir_);
		EffekseerEffect::GetInstance()->SetKamehameEffectScale(
			cfg::Kamehame::BEAM_SCALE_XY, cfg::Kamehame::BEAM_SCALE_XY, cfg::Kamehame::BEAM_SCALE_Z);
	}

	// 溜め中は、手元と狙う向きに追従させる
	if (!isBeam_)
	{
		VECTOR effectDir = dir_;

		if (VSize(effectDir) <= EPSILON)
		{
			effectDir = ctx.forward;
		}

		// 溜め中は、手元の気弾と同じ位置に出す
		EffekseerEffect::GetInstance()->UpdateKamehameEffect(GetStartPos(), effectDir);
	}

	// 終了
	if (timer_ >= cfg::Kamehame::END_TIME)
	{
		Cancel();
		return true;
	}

	return false;
}

void PlayerKamehameha::Cancel(void)
{
	isActive_ = false;
	isBeam_ = false;
	timer_ = 0.0f;

	EffekseerEffect::GetInstance()->StopKamehameEffect();
	SetLight(false);
}

void PlayerKamehameha::UpdateEffectSpeed(void)
{
	// ゲーム側の経過時間ではなく、本物の時計で測る
	const LONGLONG nowCount = GetNowHiPerformanceCount();
	float dt = (float)(nowCount - lastCount_) / 1000000.0f;
	lastCount_ = nowCount;

	if (dt > 0.1f) { dt = 0.1f; }
	if (dt < 0.0f) { dt = 0.0f; }

	// エフェクトは「1回更新するごとに1フレーム」進むので、
	// 更新が速い環境でも、ビームが発射のタイミングで出るように速度を合わせる
	const float speed = cfg::Kamehame::EFFECT_TIME_SCALE * (isBeam_
		? 60.0f * dt
		: cfg::Kamehame::EFFECT_FIRE_FRAME * dt / cfg::Kamehame::SHOT_TIME);

	EffekseerEffect::GetInstance()->SetKamehameEffectSpeed(speed);
}

void PlayerKamehameha::Draw(const Context& ctx)
{
	// 使っていなければライトを消して終了
	if (!isActive_)
	{
		SetLight(false);
		return;
	}

	// 手のボーンが見つかっていない
	if (leftHandFrame_ == -1 || rightHandFrame_ == -1)
	{
		return;
	}

	// 左右の手の真ん中
	const VECTOR chargePos = GetStartPos();

	UpdateLight(ctx);

	// チャージ中
	if (!isBeam_)
	{
		float rate = timer_ / cfg::Kamehame::SHOT_TIME;
		if (rate > 1.0f) { rate = 1.0f; }

		float scale = 0.08f + rate * 0.14f;

		// 発射直前は少し膨らませる
		if (rate > 0.85f)
		{
			float burstRate = (rate - 0.85f) / 0.15f;
			scale += burstRate * 0.06f;
		}

		DrawChargeModel(chargePos, scale, 3.0f);
		return;
	}

	// 発射中: 手元の気弾
	DrawChargeModel(chargePos, 0.18f, 5.0f);

	// 発射直後は手元の気弾が小さくなっていく
	float beamTime = timer_ - cfg::Kamehame::SHOT_TIME;

	if (beamTime < cfg::Kamehame::TRANSITION_TIME)
	{
		float t = beamTime / cfg::Kamehame::TRANSITION_TIME;
		if (t < 0.0f) { t = 0.0f; }
		if (t > 1.0f) { t = 1.0f; }

		DrawChargeModel(chargePos, 0.1f * (1.0f - t), 5.0f);
	}

	// ビーム本体はエフェクトで出すので、前のモデルは使わない
	if (!cfg::Kamehame::USE_OLD_BEAM)
	{
		return;
	}

	// ビーム本体(発射時に固定した dir_ を使う)
	const VECTOR forward = dir_;

	MV1SetPosition(beamModel_, VAdd(chargePos, VScale(forward, 20.0f)));

	const float rotY = atan2f(forward.x, forward.z);
	const float horizontal = sqrtf(forward.x * forward.x + forward.z * forward.z);
	const float rotX = -atan2f(forward.y, horizontal);	// 上下が逆なら符号を反転

	MV1SetRotationXYZ(beamModel_, { rotX, rotY, 0.0f });
	MV1SetScale(beamModel_, { 0.5f, 0.5f, 1.0f });	// ビームサイズ
	MV1DrawModel(beamModel_);
}

void PlayerKamehameha::DrawChargeModel(const VECTOR& pos, float scale, float rotSpeed) const
{
	const float rot = timer_ * rotSpeed;

	MV1SetPosition(chargeModel_, pos);
	MV1SetScale(chargeModel_, { scale, scale, scale });
	MV1SetRotationXYZ(chargeModel_, { rot * 0.5f, rot, 0.0f });
	MV1DrawModel(chargeModel_);
}

void PlayerKamehameha::UpdateLight(const Context& ctx) const
{
	if (lightHandle_ == -1)
	{
		return;
	}

	SetLightEnableHandle(lightHandle_, true);

	// 体の少し前方に置く
	VECTOR lightPos = ctx.playerPos;
	lightPos.y += 100.0f;
	lightPos = VAdd(lightPos, VScale(ctx.forward, 120.0f));

	SetLightPositionHandle(lightHandle_, lightPos);
	SetLightDifColorHandle(lightHandle_, GetColorF(1.0f, 1.0f, 1.0f, 1.0f));
}

void PlayerKamehameha::SetLight(bool enable) const
{
	if (lightHandle_ != -1)
	{
		SetLightEnableHandle(lightHandle_, enable);
	}
}

VECTOR PlayerKamehameha::GetStartPos(void) const
{
	if (leftHandFrame_ == -1 || rightHandFrame_ == -1)
	{
		return VGet(0.0f, 0.0f, 0.0f);
	}

	const VECTOR leftHandPos = MV1GetFramePosition(modelId_, leftHandFrame_);
	const VECTOR rightHandPos = MV1GetFramePosition(modelId_, rightHandFrame_);

	return
	{
		(leftHandPos.x + rightHandPos.x) * 0.5f,
		(leftHandPos.y + rightHandPos.y) * 0.5f,
		(leftHandPos.z + rightHandPos.z) * 0.5f
	};
}

VECTOR PlayerKamehameha::GetEndPos(const VECTOR& defaultForward) const
{
	VECTOR dir = NormalizeSafe(dir_);

	// 発射前などで未設定なら正面を使う
	if (VSize(dir) <= EPSILON)
	{
		dir = NormalizeSafe(defaultForward);
	}

	return VAdd(GetStartPos(), VScale(dir, cfg::Kamehame::BEAM_LENGTH));
}

float PlayerKamehameha::GetRadius(void) const
{
	return cfg::Kamehame::BEAM_RADIUS;
}