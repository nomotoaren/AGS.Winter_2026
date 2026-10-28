#include <string>
#include <cmath>
#include <cfloat>
#include <algorithm>
#include <filesystem>
#include "../../Application.h"
#include "../../Utility/AsoUtility.h"
#include "../../Utility/DamageCalc.h"
#include "../../Manager/InputManager.h"
#include "../../Manager/PadInput.h"
#include "../../Manager/SceneManager.h"
#include "../../Manager/ResourceManager.h"
#include "../../Manager/Camera.h"
#include "../Common/AnimationController.h"
#include "../Common/Capsule.h"
#include "../Common/Collider.h"
#include "../Planet.h"
#include "Player.h"
#include "definition/PlayerConfig.h"
#include "../../Manager/EffekseerEffect.h"
#include "Player_Internal.h"

namespace cfg = PlayerConfig;
using namespace PlayerInternal;

// 脚のボーンを探し、回転軸を求める
void Player::InitLegFrames(void)
{
	const int model = transform_.modelId;

	legFrames_[LEG_L_UP] = FindMixamoFrame(model, "LeftUpLeg");
	legFrames_[LEG_L_KNEE] = FindMixamoFrame(model, "LeftLeg");
	legFrames_[LEG_L_FOOT] = FindMixamoFrame(model, "LeftFoot");
	legFrames_[LEG_R_UP] = FindMixamoFrame(model, "RightUpLeg");
	legFrames_[LEG_R_KNEE] = FindMixamoFrame(model, "RightLeg");
	legFrames_[LEG_R_FOOT] = FindMixamoFrame(model, "RightFoot");

	// 各ボーンから見た「キャラの左右の軸」と「キャラの前後の軸」を求めておく
	//   (ボーンごとに向きが違うので、モデル空間の軸をボーンの空間へ変換する)
	for (int i = 0; i < LEG_MAX; i++)
	{
		const int frame = legFrames_[i];
		legAxisSide_[i] = VGet(1.0f, 0.0f, 0.0f);
		legAxisFront_[i] = VGet(0.0f, 0.0f, 1.0f);

		if (frame < 0)
		{
			continue;
		}

		// ボーンからモデルの空間への行列(親をたどって掛ける)
		MATRIX toModel = MV1GetFrameBaseLocalMatrix(model, frame);
		for (int p = MV1GetFrameParent(model, frame); p >= 0; p = MV1GetFrameParent(model, p))
		{
			toModel = MMult(toModel, MV1GetFrameBaseLocalMatrix(model, p));
		}

		const MATRIX toLocal = MInverse(toModel);
		legAxisSide_[i] = NormalizeSafe(VTransformSR(VGet(1.0f, 0.0f, 0.0f), toLocal));
		legAxisFront_[i] = NormalizeSafe(VTransformSR(VGet(0.0f, 0.0f, 1.0f), toLocal));
	}
}

void Player::UpdateAfterImages(void)
{
	// 攻撃の残像
	if (isAfterImage_)
	{
		afterImageTimer_ -= scnMng_.GetDeltaTime();

		if (afterImageTimer_ <= 0.0f)
		{
			afterImageTimer_ = 0.0f;
			isAfterImage_ = false;
		}
	}

	// 高速移動の残像
	for (auto it = boostAfterImages_.begin(); it != boostAfterImages_.end();)
	{
		it->timer -= scnMng_.GetDeltaTime();

		// 多すぎるときは古いものから消す
		const bool tooMany = (boostAfterImages_.size() > 6);

		if (it->timer <= 0.0f || tooMany)
		{
			it = boostAfterImages_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

// 浮遊の見た目: 移動の向きに体を傾ける
//   当たり判定は動かさず、描画の行列だけ傾ける
void Player::UpdateFloatLean(void)
{
	const float dt = scnMng_.GetDeltaTime();
	floatBobTime_ += cfg::Float::BOB_SPEED * dt;

	float targetPitch = 0.0f;
	float targetRoll = 0.0f;

	// 自分で動いているときだけ傾ける
	if (IsFloatSelfMove())
	{
		const VECTOR forward = GetForward();
		const VECTOR right = VGet(forward.z, 0.0f, -forward.x);	// 右

		const float f = VDot(movePow_, forward);	// 前がプラス、後ろがマイナス
		const float s = VDot(movePow_, right);		// 右がプラス

		targetPitch = f * cfg::Float::LEAN_PER_SPEED - movePow_.y * cfg::Float::VERTICAL_LEAN;
		targetRoll = -s * cfg::Float::LEAN_PER_SPEED * cfg::Float::ROLL_SIGN;

		targetPitch = std::clamp(targetPitch, -cfg::Float::MAX_PITCH, cfg::Float::MAX_PITCH);
		targetRoll = std::clamp(targetRoll, -cfg::Float::MAX_ROLL, cfg::Float::MAX_ROLL);

		// 脚のポーズの倍率で、傾きも調整する
		targetPitch *= legTune_[TUNE_LEAN];
		targetRoll *= legTune_[TUNE_LEAN];
	}

	// 回避ステップ中は、進む向きへ大きく倒す
	float smooth = cfg::Float::LEAN_SMOOTH;

	if (isDodge_)
	{
		const VECTOR forward = GetForward();
		const VECTOR right = VGet(forward.z, 0.0f, -forward.x);

		const float f = VDot(dodgeDir_, forward);
		const float s = VDot(dodgeDir_, right);

		const float lean = cfg::Step::LEAN;

		targetPitch = f * lean;
		targetRoll = -s * lean * cfg::Float::ROLL_SIGN;

		smooth = cfg::Step::LEAN_SMOOTH;
	}

	// 目標の傾きへ少しずつ近づける
	float k = smooth * dt;
	if (k > 1.0f) { k = 1.0f; }

	floatPitch_ += (targetPitch - floatPitch_) * k;
	floatRoll_ += (targetRoll - floatRoll_) * k;
}

// 自分で移動している状態か(攻撃・回避などではない)
bool Player::IsFloatSelfMove(void) const
{
	return
		!isDamage_ &&
		!attack_.IsAttack() && !attack_.IsChasing() &&
		!boostChase_.IsActive() && !isDodge_ &&
		!kamehame_.IsActive() && !guard_.IsBusy() &&
		!formChange_.IsActive() &&
		!isCharging_ && !isChargeEnding_ &&
		!isKiBlast_;
}

void Player::UpdateLegTune(void)
{
#ifdef _DEBUG
	auto& ins = InputManager::GetInstance();

	if (ins.IsTrgDown(KEY_INPUT_F1))
	{
		isLegTune_ = !isLegTune_;
	}

	if (!isLegTune_)
	{
		return;
	}

	if (ins.IsTrgDown(KEY_INPUT_F2)) { legTuneSel_ = (legTuneSel_ + TUNE_MAX - 1) % TUNE_MAX; }
	if (ins.IsTrgDown(KEY_INPUT_F3)) { legTuneSel_ = (legTuneSel_ + 1) % TUNE_MAX; }

	const bool fine = ins.IsNew(KEY_INPUT_LSHIFT);
	float step = fine ? 1.0f : 5.0f;
	if (legTuneSel_ == TUNE_LEAN) { step = fine ? 0.05f : 0.25f; }

	if (ins.IsTrgDown(KEY_INPUT_F5)) { legTune_[legTuneSel_] -= step; }
	if (ins.IsTrgDown(KEY_INPUT_F6)) { legTune_[legTuneSel_] += step; }

	if (legTune_[TUNE_LEAN] < 0.0f) { legTune_[TUNE_LEAN] = 0.0f; }
#endif
}

void Player::DrawLegTune(void) const
{
#ifdef _DEBUG
	if (!isLegTune_)
	{
		return;
	}

	const int x = 20;
	int y = 20;

	DrawString(x, y, "[脚の調整] F2/F3:項目  F5/F6:値(-/+)  LSHIFT:細かく  F1:閉じる", GetColor(255, 255, 0));
	y += 22;

	for (int i = 0; i < TUNE_MAX; i++)
	{
		const unsigned int color = (i == legTuneSel_) ? GetColor(255, 120, 60) : GetColor(255, 255, 255);
		DrawFormatString(x, y, color, "%s %-26s : %.2f", (i == legTuneSel_) ? ">" : " ", kLegTuneName[i], legTune_[i]);
		y += 20;
	}
#endif
}

void Player::CancelRootMotion(void)
{
	if (hipsFrame_ < 0) { return; }

	// 移動のアニメだけ、腰の水平移動を打ち消す(それ以外は元に戻す)
	switch ((ANIM_TYPE)animationController_->GetPlayType())
	{
	case ANIM_TYPE::RUN:
	case ANIM_TYPE::FAST_RUN:
	case ANIM_TYPE::LOCK_LEFT:
	case ANIM_TYPE::LOCK_RIGHT:
	case ANIM_TYPE::LOCK_LEFT_RUN:
	case ANIM_TYPE::LOCK_RIGHT_RUN:
	case ANIM_TYPE::LOCK_BACK:
	case ANIM_TYPE::LOCK_BACK_RUN:
		break;
	default:
		MV1ResetFrameUserLocalMatrix(transform_.modelId, hipsFrame_);
		return;
	}

	const int model = transform_.modelId;

	MV1ResetFrameUserLocalMatrix(model, hipsFrame_);
	MATRIX m = MV1GetFrameLocalMatrix(model, hipsFrame_);
	const MATRIX base = MV1GetFrameBaseLocalMatrix(model, hipsFrame_);

	// 水平(X・Z)だけ、元の位置に戻す
	m.m[3][0] = base.m[3][0];
	m.m[3][2] = base.m[3][2];

	MV1SetFrameUserLocalMatrix(model, hipsFrame_, m);
}

// 浮遊移動中の脚のポーズ
//   移動アニメの上から、脚のボーンだけポーズへ寄せる
//   左脚: 太ももを前へ上げて膝を曲げる / 右脚: 少し開いて軽く曲げて垂らす
void Player::UpdateLegPose(void)
{
	namespace lp = cfg::LegPose;

	const float dt = scnMng_.GetDeltaTime();
	const int model = transform_.modelId;

	// 動いているか
	const bool moving =
		VSize(ToHorizontal(movePow_)) > Constants::Epsilon ||
		fabsf(movePow_.y) > Constants::Epsilon;

	const float target = (IsFloatSelfMove() && (moving || lp::IN_IDLE)) ? 1.0f : 0.0f;

	float k = lp::BLEND_SPEED * dt;
	if (k > 1.0f) { k = 1.0f; }
	legPoseRate_ += (target - legPoseRate_) * k;

	// ほぼポーズなしなら、アニメのままにする
	if (legPoseRate_ < 0.01f)
	{
		legPoseRate_ = 0.0f;

		for (int frame : legFrames_)
		{
			if (frame >= 0)
			{
				MV1ResetFrameUserLocalMatrix(model, frame);
			}
		}
		return;
	}

	// 横移動の量(脚を進行方向と逆へ流す)
	const VECTOR forward = GetForward();
	const VECTOR right = VGet(forward.z, 0.0f, -forward.x);
	float side = VDot(movePow_, right) / SPEED_RUN;
	side = std::clamp(side, -1.0f, 1.0f);

	const float sway = sinf(floatBobTime_ * 0.7f) * lp::SWAY;
	const float spread = -side * lp::SIDE_SPREAD * lp::SIDE_SIGN;

	// アニメの姿勢とポーズを、legPoseRate_ で混ぜる
	auto apply = [&](int frame, const MATRIX& rot)
		{
			if (frame < 0)
			{
				return;
			}

			// アニメの姿勢
			MV1ResetFrameUserLocalMatrix(model, frame);
			const MATRIX anim = MV1GetFrameLocalMatrix(model, frame);

			// ポーズ: 元の姿勢から回したもの
			const MATRIX base = MV1GetFrameBaseLocalMatrix(model, frame);
			const MATRIX pose = MMult(rot, base);

			MV1SetFrameUserLocalMatrix(model, frame, BlendMatrix(anim, pose, legPoseRate_));
		};

	auto rad = [](float deg) { return AsoUtility::Deg2RadF(deg); };

	// swing: 脚を前後に振る(キャラの左右の軸で回す)
	// spreadRot: 脚を横に開く(キャラの前後の軸で回す)
	const float fs = lp::FORWARD_SIGN;
	auto swing = [&](int leg, float deg)
		{
			return MGetRotAxis(legAxisSide_[leg], rad(deg) * fs);
		};
	auto spreadRot = [&](int leg, float deg)
		{
			return MGetRotAxis(legAxisFront_[leg], rad(deg));
		};

	// 左脚
	//   太ももを前へ上げ、膝を深く曲げる
	const float openL = legTune_[TUNE_L_OPEN] * lp::OPEN_SIGN;
	const float openR = -legTune_[TUNE_R_OPEN] * lp::OPEN_SIGN;

	apply(legFrames_[LEG_L_UP], MMult(swing(LEG_L_UP, legTune_[TUNE_L_THIGH] + sway), spreadRot(LEG_L_UP, spread + openL)));
	apply(legFrames_[LEG_L_KNEE], swing(LEG_L_KNEE, -(legTune_[TUNE_L_KNEE] - sway)));
	apply(legFrames_[LEG_L_FOOT], swing(LEG_L_FOOT, -legTune_[TUNE_FOOT]));

	// 右脚
	apply(legFrames_[LEG_R_UP], MMult(swing(LEG_R_UP, legTune_[TUNE_R_THIGH] - sway), spreadRot(LEG_R_UP, spread + openR)));
	apply(legFrames_[LEG_R_KNEE], swing(LEG_R_KNEE, -(legTune_[TUNE_R_KNEE] + sway)));
	apply(legFrames_[LEG_R_FOOT], swing(LEG_R_FOOT, -legTune_[TUNE_FOOT]));
}

// 見た目の向き・位置を transform_ に反映する
//   当たり判定用の位置と向きは変えずに、描画の行列だけ傾けたり揺らしたりする
void Player::ApplyVisualTransform(const Quaternion& logicRot)
{
	// 気溜め・変身中は、見た目だけ体の向きを回す
	Quaternion baseRot = logicRot;
	if (isCharging_ || isChargeEnding_ || formChange_.IsActive())
	{
		baseRot = logicRot.Mult(Quaternion::Euler(
			{ 0.0f, AsoUtility::Deg2RadF(cfg::Charge::VISUAL_YAW_DEG), 0.0f }));
	}
	else if (heatActivate_ > 0.0f)
	{
		// ヒートアップ発動の演出中も、見た目だけ体の向きを回す
		baseRot = logicRot.Mult(Quaternion::Euler(
			{ 0.0f, AsoUtility::Deg2RadF(cfg::HeatUp::ACTIVATE_YAW_DEG), 0.0f }));
	}

	const Quaternion visualRot =
		baseRot.Mult(Quaternion::Euler({ floatPitch_, 0.0f, floatRoll_ }));

	// 傾ける中心(体の真ん中)がずれないように位置を補正し、上下にゆらす
	const VECTOR pivot = { 0.0f, cfg::Float::PIVOT_HEIGHT, 0.0f };
	VECTOR offset = VSub(pivot, visualRot.PosAxis(pivot));
	offset.y += sinf(floatBobTime_) * cfg::Float::BOB_AMPLITUDE;

	const VECTOR logicPos = transform_.pos;

	transform_.pos = VAdd(logicPos, offset);
	transform_.quaRot = visualRot;
	transform_.Update();		// 描画用の行列を作る

	transform_.pos = logicPos;
	transform_.quaRot = logicRot;
}

void Player::Draw(void)
{
	// 攻撃の残像
	if (isAfterImage_)
	{
		DrawAfterImage(afterImageMatrix_, 0.35f, false);
	}

	// 高速移動の残像
	for (auto& image : boostAfterImages_)
	{
		DrawAfterImage(image.matrix, image.timer / Constants::BoostAfterImageDuration * 0.5f, true);
	}

	// ヒートアップ中は、体を金色っぽくする
	{
		const bool beforeGold = IsBeforeGold();

		float g = 1.0f;
		float b = 1.0f;

		if (heatUp_ && !beforeGold)
		{
			const float pulse = 0.5f + 0.5f * sinf((float)GetNowCount() * 0.001f * 8.0f);
			g = 0.88f + 0.06f * pulse;
			b = 0.55f + 0.15f * pulse;
		}

		MV1SetDifColorScale(transform_.modelId, GetColorF(1.0f, g, b, 1.0f));
	}

	// 本体
	SetUseLighting(false);
	MV1DrawModel(transform_.modelId);
	SetUseLighting(true);

	DrawHeatAura();
	DrawKiBlast();
	DrawKamehame();

	DrawLegTune();
}

float Player::GetHeatActivateElapsed(void) const
{
	if (heatActivate_ <= 0.0f)
	{
		return -1.0f;
	}

	return cfg::HeatUp::ACTIVATE_TIME - heatActivate_;
}

// ヒートアップ発動の演出中で、まだ金色になる前(光がはじけ終わる前)か
bool Player::IsBeforeGold(void) const
{
	return heatActivate_ > 0.0f &&
		GetHeatActivateElapsed() < cfg::HeatUp::ACTIVATE_BURST_END;
}

void Player::DrawHud(void)
{
	hud_.Draw(*this);
}

void Player::DrawAfterImage(const MATRIX& matrix, float opacity, bool disableLighting)
{
	MATRIX playerMatrix = MV1GetMatrix(transform_.modelId);

	MV1SetMatrix(transform_.modelId, matrix);
	MV1SetOpacityRate(transform_.modelId, opacity);

	if (disableLighting) { SetUseLighting(false); }
	MV1DrawModel(transform_.modelId);
	if (disableLighting) { SetUseLighting(true); }

	MV1SetMatrix(transform_.modelId, playerMatrix);
	MV1SetOpacityRate(transform_.modelId, 1.0f);
}

void Player::DrawKiBlast(void)
{
	barrage_.Draw();

	for (auto& blast : kiBlasts_)
	{
		blast->Draw();
	}

	for (auto& blast : finishBlasts_)
	{
		blast->Draw();
	}
}

void Player::DrawShadow(void)
{
	const float SHADOW_HEIGHT = Constants::PlayerShadowHeight;
	const float SHADOW_SIZE = Constants::PlayerShadowSize;
	const float SHADOW_MAX_ALPHA = 128.0f;

	// 高いところほど薄くする
	auto calcAlpha = [&](float y) -> int
		{
			if (y <= transform_.pos.y - SHADOW_HEIGHT)
			{
				return 0;
			}
			float rate = 1.0f - fabsf(y - transform_.pos.y) / SHADOW_HEIGHT;
			return static_cast<int>(roundf(SHADOW_MAX_ALPHA * rate));
		};

	SetUseLighting(FALSE);
	SetUseZBuffer3D(TRUE);

	// テクスチャの外は伸ばす
	SetTextureAddressMode(DX_TEXADDRESS_CLAMP);

	// 影のポリゴン
	VERTEX3D vertex[3] = { VERTEX3D(), VERTEX3D(), VERTEX3D() };
	vertex[0].dif = GetColorU8(255, 255, 255, 255);
	vertex[0].spc = GetColorU8(0, 0, 0, 0);
	vertex[0].su = 0.0f;
	vertex[0].sv = 0.0f;
	vertex[1] = vertex[0];
	vertex[2] = vertex[0];

	for (const auto& c : colliders_)
	{
		auto collider = c.lock();
		if (!collider)
		{
			continue;
		}

		// 足元の地面のポリゴンを探す
		MV1_COLL_RESULT_POLY_DIM hitDim = MV1CollCheck_Capsule(
			collider->modelId_, -1,
			transform_.pos,
			VAdd(transform_.pos, { 0.0f, -SHADOW_HEIGHT, 0.0f }),
			SHADOW_SIZE);

		for (int i = 0; i < hitDim.HitNum; i++)
		{
			const MV1_COLL_RESULT_POLY& poly = hitDim.Dim[i];

			// 地面に埋もれないように少し浮かせる
			VECTOR slide = VScale(poly.Normal, 0.5f);

			for (int v = 0; v < 3; v++)
			{
				vertex[v].pos = VAdd(poly.Position[v], slide);
				vertex[v].dif.a = calcAlpha(poly.Position[v].y);

				// プレイヤーの位置を中心にテクスチャを貼る
				vertex[v].u = (poly.Position[v].x - transform_.pos.x) / (SHADOW_SIZE * 2.0f) + 0.5f;
				vertex[v].v = (poly.Position[v].z - transform_.pos.z) / (SHADOW_SIZE * 2.0f) + 0.5f;
			}

			DrawPolygon3D(vertex, 1, imgShadow_, TRUE);
		}

		MV1CollResultPolyDimTerminate(hitDim);
	}

	SetUseLighting(TRUE);
	SetUseZBuffer3D(FALSE);
}

void Player::LeaveAfterImage(void)
{
	afterImageMatrix_ = MV1GetMatrix(transform_.modelId);
	isAfterImage_ = true;
	afterImageTimer_ = Constants::AfterImageDuration;
}

// 高速移動の残像を1枚残す
void Player::AddBoostAfterImage(void)
{
	BoostAfterImage image{};
	image.matrix = MV1GetMatrix(transform_.modelId);
	image.timer = Constants::BoostAfterImageDuration;
	boostAfterImages_.push_back(image);
}

void Player::DrawKamehame(void)
{
	kamehame_.Draw(MakeKamehameContext());
}

// ヒートアップのオーラ
void Player::DrawHeatAura(void) const
{
	if (!heatUp_ && heatActivate_ <= 0.0f)
	{
		return;
	}

	const float t = (float)GetNowCount() * 0.001f;
	const float pulse = 0.5f + 0.5f * sinf(t * 14.0f);
	const VECTOR base = transform_.pos;

	SetUseLighting(FALSE);

	// オーラで体が隠れないように、Zバッファに書かない
	SetWriteZBuffer3D(FALSE);

	// 発動の演出: 光の柱 → 光がはじける
	if (heatActivate_ > 0.0f)
	{
		const float t = cfg::HeatUp::ACTIVATE_TIME - heatActivate_;

		if (t < cfg::HeatUp::ACTIVATE_FLASH)
		{
			const float rate = t / cfg::HeatUp::ACTIVATE_FLASH;
			const VECTOR top = VAdd(base, VGet(0.0f, 120.0f + 380.0f * rate, 0.0f));

			SetDrawBlendMode(DX_BLENDMODE_ADD, 70 + (int)(60.0f * pulse));
			DrawCapsule3D(VAdd(base, VGet(0.0f, -20.0f, 0.0f)), top, 34.0f + 10.0f * rate, 12,
				GetColor(110, 170, 255), GetColor(110, 170, 255), TRUE);

			SetDrawBlendMode(DX_BLENDMODE_ADD, 120);
			DrawCapsule3D(VAdd(base, VGet(0.0f, -20.0f, 0.0f)), top, 14.0f, 8,
				GetColor(230, 245, 255), GetColor(230, 245, 255), TRUE);

			// 立ち昇る光の粒
			SetDrawBlendMode(DX_BLENDMODE_ADD, 200);

			for (int i = 0; i < 14; i++)
			{
				const float up = fmodf(t * 2.2f + i * 0.31f, 1.0f);
				const float ang = i * 0.9f + t * 3.0f;
				const VECTOR p = VAdd(base, VGet(cosf(ang) * 40.0f, up * 300.0f, sinf(ang) * 40.0f));

				DrawSphere3D(p, 1.5f + 4.0f * (1.0f - up), 6, GetColor(170, 215, 255), GetColor(170, 215, 255), TRUE);
			}
		}
		else if (t < cfg::HeatUp::ACTIVATE_BURST_END)
		{
			const float u = (t - cfg::HeatUp::ACTIVATE_FLASH) / (cfg::HeatUp::ACTIVATE_BURST_END - cfg::HeatUp::ACTIVATE_FLASH);
			const float fade = 1.0f - 0.85f * u;

			SetDrawBlendMode(DX_BLENDMODE_ADD, (int)(170.0f * fade));
			DrawSphere3D(VAdd(base, VGet(0.0f, 80.0f, 0.0f)), 50.0f + 300.0f * u, 24,
				GetColor(190, 225, 255), GetColor(190, 225, 255), TRUE);

			SetDrawBlendMode(DX_BLENDMODE_ADD, (int)(200.0f * fade));
			DrawSphere3D(VAdd(base, VGet(0.0f, 80.0f, 0.0f)), 30.0f + 120.0f * u, 20,
				GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);
		}
	}

	// 金色になる前は、ここまで
	const bool beforeGold = IsBeforeGold();

	if (!heatUp_ || beforeGold)
	{
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetWriteZBuffer3D(TRUE);
		SetUseLighting(TRUE);
		return;
	}

	// 金色のオーラ
	SetDrawBlendMode(DX_BLENDMODE_ADD, 22 + (int)(14.0f * pulse));
	DrawCapsule3D(VAdd(base, VGet(0.0f, 30.0f, 0.0f)), VAdd(base, VGet(0.0f, 130.0f, 0.0f)),
		46.0f + 6.0f * pulse, 12, GetColor(255, 200, 60), GetColor(255, 200, 60), TRUE);

	SetDrawBlendMode(DX_BLENDMODE_ADD, 10);
	DrawCapsule3D(VAdd(base, VGet(0.0f, 20.0f, 0.0f)), VAdd(base, VGet(0.0f, 150.0f, 0.0f)),
		70.0f + 10.0f * pulse, 12, GetColor(255, 235, 140), GetColor(255, 235, 140), TRUE);

	// 炎のようなゆらめき
	constexpr int   kFlameCount = 8;
	constexpr float kFlameRadius = 52.0f;	// 体からの距離
	constexpr float kFlameWidth = 14.0f;	// 炎の太さ
	constexpr int   kFlameAlpha = 70;

	for (int i = 0; i < kFlameCount; i++)
	{
		const float ang = (float)i / (float)kFlameCount * DX_TWO_PI_F + t * 1.5f;
		const float flicker = 0.5f + 0.5f * sinf(t * 11.0f + (float)i * 1.7f);
		const float height = 110.0f + 90.0f * flicker;

		const VECTOR root = VAdd(base, VGet(cosf(ang) * kFlameRadius, 10.0f, sinf(ang) * kFlameRadius));
		const VECTOR tip = VAdd(root, VGet(-cosf(ang) * 14.0f, height, -sinf(ang) * 14.0f));

		const unsigned int color = (i % 2 == 0) ? GetColor(255, 200, 60) : GetColor(255, 235, 140);

		SetDrawBlendMode(DX_BLENDMODE_ADD, kFlameAlpha);
		DrawCone3D(tip, root, kFlameWidth, 8, color, color, TRUE);
	}

	// 立ち昇る光の粒
	SetDrawBlendMode(DX_BLENDMODE_ADD, 200);

	for (int i = 0; i < 16; i++)
	{
		const float up = fmodf(t * 1.8f + i * 0.37f, 1.0f);
		const float ang = i * 0.8f + t * 2.0f;
		const VECTOR p = VAdd(base, VGet(cosf(ang) * 55.0f, 20.0f + up * 170.0f, sinf(ang) * 55.0f));

		DrawSphere3D(p, 1.0f + 5.0f * (1.0f - up), 6, GetColor(255, 230, 120), GetColor(255, 230, 120), TRUE);
	}

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	SetWriteZBuffer3D(TRUE);
	SetUseLighting(TRUE);
}
