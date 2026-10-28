#include <DxLib.h>
#include <cmath>
#include <algorithm>
#include "MeleeEnemy.h"
#include "../../../Application.h"
#include "../../Player/Player.h"
#include "../../../Manager/Camera.h"
#include "../../../Manager/EffekseerEffect.h"
#include "../../../Manager/ResourceManager.h"
#include "../../../Manager/SceneManager.h"
#include "../../../Utility/AsoUtility.h"
#include "MeleeEnemy_Internal.h"

using namespace MeleeEnemyInternal;

// 描画
void MeleeEnemy::Draw(void)
{
	if (isDead_)
	{
		return;
	}

	// 敵本体
	if (!isHidden_)
	{
		SetUseLighting(FALSE);
		MV1DrawModel(transform_.modelId);

		// スーパーアーマー中(大技の溜めなど)は、体が赤く光る
		if (IsSuperArmor())
		{
			const float glow = 0.5f + 0.5f * sinf(hoverTime_ * 14.0f);

			SetDrawBlendMode(DX_BLENDMODE_ADD, 120 + (int)(80.0f * glow));
			MV1SetDifColorScale(transform_.modelId, GetColorF(1.0f, 0.15f, 0.1f, 1.0f));
			MV1DrawModel(transform_.modelId);
			MV1SetDifColorScale(transform_.modelId, GetColorF(1.0f, 1.0f, 1.0f, 1.0f));
			SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		}

		SetUseLighting(TRUE);
	}

	// スーパーアーマー中(大技の溜めなど)は赤いオーラをまとう。この間は殴っても怯まない
	if (!isHidden_ && IsSuperArmor())
	{
		const float pulse = 0.5f + 0.5f * sinf(hoverTime_ * 14.0f);

		SetUseLighting(FALSE);
		SetWriteZBuffer3D(FALSE);
		SetDrawBlendMode(DX_BLENDMODE_ADD, 70 + (int)(50.0f * pulse));
		DrawSphere3D(GetChestPos(), 85.0f + 8.0f * pulse, 20, GetColor(255, 30, 10), GetColor(255, 30, 10), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetWriteZBuffer3D(TRUE);
		SetUseLighting(TRUE);
	}

	// 反撃の衝撃波
	if (burstFx_ > 0.0f && !isHidden_)
	{
		// 最初の0.3割で一気に広がり、しばらく残ってから消える
		const float t = 1.0f - burstFx_ / kBurstFxTime;
		const float grow = Saturate(t / 0.25f);
		const float fade = (t < 0.6f) ? 1.0f : Saturate((1.0f - t) / 0.4f);

		SetUseLighting(FALSE);
		SetWriteZBuffer3D(FALSE);
		SetDrawBlendMode(DX_BLENDMODE_ADD, (int)(130.0f * fade));
		DrawSphere3D(GetChestPos(), 50.0f + 270.0f * grow, 24, GetColor(170, 215, 255), GetColor(170, 215, 255), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetWriteZBuffer3D(TRUE);
		SetUseLighting(TRUE);
	}

	// 撃った気弾
	for (const auto& blast : blasts_)
	{
		blast.Draw();
	}

	// 範囲攻撃(警告・爆発)
	for (const auto& blast : areaBlasts_)
	{
		blast.Draw();
	}

	// 高速移動の軌跡
	DrawTrail();

	// 気弾・突進などの予兆(プレイヤーが避ける目安)
	DrawTelegraph();

	// HPバー・スタンゲージ(2D)は DrawOverlay で、最後にまとめて描く
}

// 気を溜めている球・突進の構えの光
void MeleeEnemy::DrawTelegraph(void) const
{
	if (aiState_ == AI_STATE::KI_CHARGE)
	{
		const float rate = aiTimer_ / kKiChargeTime;
		const VECTOR forward = transform_.quaRot.GetForward();
		const VECTOR pos = VAdd(GetChestPos(), VScale(forward, 50.0f));

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 190);
		DrawSphere3D(pos, 8.0f + 36.0f * rate, 16, GetColor(150, 40, 255), GetColor(150, 40, 255), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawSphere3D(pos, 4.0f + 14.0f * rate, 16, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);

		SetUseLighting(TRUE);
	}

	if (aiState_ == AI_STATE::RUSH_READY)
	{
		// だんだん赤く、大きくなる
		const float rate = aiTimer_ / kRushReadyTime;
		const float pulse = 0.5f + 0.5f * sinf(aiTimer_ * 40.0f);

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 160 + (int)(80.0f * pulse));
		DrawSphere3D(GetChestPos(), 30.0f + 30.0f * rate, 16, GetColor(255, 20, 0), GetColor(255, 20, 0), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetUseLighting(TRUE);
	}

	// 範囲大技の溜め: 頭の上に大きな球を作る
	if (aiState_ == AI_STATE::BIG_CHARGE)
	{
		const float rate = Saturate(aiTimer_ / kBigChargeTime);

		DrawAuraColumn(transform_.pos, rate);
		const VECTOR pos = VAdd(transform_.pos, VGet(0.0f, 170.0f, 0.0f));

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 190);
		DrawSphere3D(pos, 10.0f + 60.0f * rate, 20, GetColor(255, 50, 0), GetColor(255, 50, 0), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawSphere3D(pos, 5.0f + 28.0f * rate, 16, GetColor(255, 230, 200), GetColor(255, 230, 200), TRUE);

		SetUseLighting(TRUE);
	}

	// 投げの構え: オレンジに光る(ガードしても防げない合図)
	if (aiState_ == AI_STATE::GRAB_READY)
	{
		const float pulse = 0.5f + 0.5f * sinf(aiTimer_ * 36.0f);

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170 + (int)(70.0f * pulse));
		DrawSphere3D(GetChestPos(), 50.0f + 20.0f * pulse, 16, GetColor(255, 140, 0), GetColor(255, 140, 0), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetUseLighting(TRUE);
	}

	// 追撃: 消えた場所に残像の光 / 現れたら黄色く光って叩く予告
	if (aiState_ == AI_STATE::CHASE_VANISH)
	{
		const float rate = Saturate(aiTimer_ / kChaseVanishTime);

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(180.0f * (1.0f - rate)));
		DrawSphere3D(vanishFrom_, 60.0f * (1.0f - rate) + 10.0f, 16, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetUseLighting(TRUE);
	}

	if (aiState_ == AI_STATE::CHASE_APPEAR)
	{
		const float pulse = 0.5f + 0.5f * sinf(aiTimer_ * 40.0f);

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170 + (int)(80.0f * pulse));
		DrawSphere3D(GetChestPos(), 55.0f, 16, GetColor(255, 230, 0), GetColor(255, 230, 0), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetUseLighting(TRUE);
	}

	// ビームの溜め: 大きな球を作る。狙いを固定したら、赤い帯(これから撃つ線)を出す
	if (aiState_ == AI_STATE::BEAM_CHARGE)
	{
		const float rate = Saturate(aiTimer_ / kBeamChargeTime);
		const float pulse = 0.5f + 0.5f * sinf(aiTimer_ * 25.0f);
		const VECTOR ball = beamLocked_ ? beamOrigin_ : GetBeamOrigin();
		const float r = 12.0f + 70.0f * rate;

		DrawAuraColumn(transform_.pos, rate);

		SetUseLighting(FALSE);

		if (beamLocked_)
		{
			const VECTOR end = VAdd(beamOrigin_, VScale(beamDir_, kBeamLength));

			// 当たる範囲(半透明)
			SetDrawBlendMode(DX_BLENDMODE_ALPHA, 85 + (int)(55.0f * pulse));
			DrawCapsule3D(beamOrigin_, end, kBeamRadius, 12, GetColor(255, 0, 0), GetColor(255, 0, 0), TRUE);

		}

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
		DrawSphere3D(ball, r * (1.2f + 0.1f * pulse), 20, GetColor(150, 20, 255), GetColor(150, 20, 255), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 230);
		DrawSphere3D(ball, r, 20, GetColor(210, 90, 255), GetColor(210, 90, 255), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawSphere3D(ball, r * 0.55f, 16, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);

		SetUseLighting(TRUE);
	}

	// 連射のあとの大きな気弾の溜め: 胸の前に大きな球を作る
	if (aiState_ == AI_STATE::KI_BARRAGE && barrageBigPhase_)
	{
		const float rate = Saturate(barrageBigTimer_ / kBarrageBigChargeTime);
		const VECTOR pos = VAdd(GetChestPos(), VScale(transform_.quaRot.GetForward(), 80.0f));

		DrawAuraColumn(transform_.pos, rate);

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 190);
		DrawSphere3D(pos, 14.0f + (kBarrageBigRadius * 1.8f) * rate, 20, GetColor(150, 40, 255), GetColor(150, 40, 255), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawSphere3D(pos, 7.0f + (kBarrageBigRadius * 0.8f) * rate, 20, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);

		SetUseLighting(TRUE);
	}

	// 気弾の連射の溜め: 胸の前に小さな球
	if (aiState_ == AI_STATE::KI_BARRAGE && aiTimer_ < kBarrageChargeTime)
	{
		const float rate = Saturate(aiTimer_ / kBarrageChargeTime);
		const VECTOR pos = VAdd(GetChestPos(), VScale(transform_.quaRot.GetForward(), 50.0f));

		// 赤いオーラの柱(溜めの演出)
		DrawAuraColumn(transform_.pos, rate);

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 190);
		DrawSphere3D(pos, 8.0f + 26.0f * rate, 16, GetColor(150, 40, 255), GetColor(150, 40, 255), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawSphere3D(pos, 4.0f + 10.0f * rate, 16, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);

		SetUseLighting(TRUE);
	}

	// 急降下: 上昇中はピンクのオーラ、狙いを固定したら降下先に赤い輪と線
	if (aiState_ == AI_STATE::SLAM_RISE)
	{
		const float rate = Saturate(aiTimer_ / kSlamChargeTime);
		const float pulse = 0.5f + 0.5f * sinf(aiTimer_ * 30.0f);

		DrawAuraColumn(transform_.pos, rate);

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 150 + (int)(60.0f * pulse));
		DrawSphere3D(GetChestPos(), 50.0f + 20.0f * rate, 16, GetColor(255, 40, 150), GetColor(255, 40, 150), TRUE);

		if (slamLocked_)
		{
			// 降下先(衝撃波の範囲)
			SetDrawBlendMode(DX_BLENDMODE_ALPHA, 80 + (int)(50.0f * pulse));
			DrawSphere3D(slamTarget_, kSlamImpactRadius, 20, GetColor(255, 0, 0), GetColor(255, 0, 0), TRUE);

			SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
			DrawRing3D(slamTarget_, kSlamImpactRadius, GetColor(255, 230, 0));

			// 降りる道すじ
			SetDrawBlendMode(DX_BLENDMODE_ALPHA, 200 + (int)(55.0f * pulse));
			DrawCapsule3D(GetChestPos(), slamTarget_, 9.0f, 6, GetColor(255, 40, 0), GetColor(255, 40, 0), TRUE);
		}

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetUseLighting(TRUE);
	}

	// ビーム発射: 光の太い帯(外側が紫、中心が白い芯)
	if (aiState_ == AI_STATE::BEAM_FIRE && !beamClash_)
	{
		const float grow = Saturate(aiTimer_ / kBeamGrowTime);
		const float remain = kBeamFireTime - aiTimer_;
		const float fade = (remain < kBeamFadeTime) ? Saturate(remain / kBeamFadeTime) : 1.0f;
		const float wobble = 1.0f + 0.08f * sinf(aiTimer_ * 60.0f);
		const float w = kBeamRadius * fade * wobble;

		const VECTOR end = VAdd(beamOrigin_, VScale(beamDir_, kBeamLength * grow));

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90);
		DrawCapsule3D(beamOrigin_, end, w * 1.7f, 16, GetColor(190, 60, 255), GetColor(190, 60, 255), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
		DrawCapsule3D(beamOrigin_, end, w * 1.2f, 16, GetColor(235, 150, 255), GetColor(235, 150, 255), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawCapsule3D(beamOrigin_, end, w * 0.6f, 12, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);

		// 出口の光の球
		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
		DrawSphere3D(beamOrigin_, w * 2.2f, 20, GetColor(235, 150, 255), GetColor(235, 150, 255), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetUseLighting(TRUE);
	}
}

// 高速移動の軌跡: ピンクの帯が、だんだん細く消えていく
void MeleeEnemy::DrawTrail(void) const
{
	if (trail_.size() < 2)
	{
		return;
	}

	SetUseLighting(FALSE);

	for (size_t i = 1; i < trail_.size(); i++)
	{
		if (trail_[i].cut)
		{
			continue;
		}

		const float rate = Saturate(trail_[i].life / kTrailLife);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(150.0f * rate));
		DrawCapsule3D(trail_[i - 1].pos, trail_[i].pos, 3.0f + 16.0f * rate, 8,
			GetColor(255, 120, 200), GetColor(255, 120, 200), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, (int)(200.0f * rate));
		DrawCapsule3D(trail_[i - 1].pos, trail_[i].pos, 1.5f + 5.0f * rate, 6,
			GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);
	}

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	SetUseLighting(TRUE);
}

// 大技の溜め: 足元から上へのびるピンクの光の柱(rate: 0.0~1.0 で太く明るくなる)
void MeleeEnemy::DrawAuraColumn(const VECTOR& base, float rate) const
{
	const VECTOR bottom = VAdd(base, VGet(0.0f, -50.0f, 0.0f));
	const VECTOR top = VAdd(base, VGet(0.0f, 700.0f, 0.0f));

	SetUseLighting(FALSE);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 80 + (int)(70.0f * rate));
	DrawCapsule3D(bottom, top, 35.0f + 25.0f * rate, 12, GetColor(255, 40, 150), GetColor(255, 40, 150), TRUE);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 130 + (int)(80.0f * rate));
	DrawCapsule3D(bottom, top, 12.0f + 10.0f * rate, 10, GetColor(255, 230, 245), GetColor(255, 230, 245), TRUE);

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	SetUseLighting(TRUE);
}
