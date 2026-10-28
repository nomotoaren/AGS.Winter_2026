#include <DxLib.h>
#include "../Manager/SceneManager.h"
#include "../Manager/Camera.h"
#include "../Manager/InputManager.h"
#include "../Manager/PadInput.h"
#include "../Manager/EffekseerEffect.h"
#include "../Object/Player/Player.h"
#include "../Object/Enemy/EnemyBase.h"
#include "../Object/Enemy/Melee/MeleeEnemy.h"
#include "ClashSystem.h"

namespace
{
	float Clamp01(float v)
	{
		return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
	}

	// 2本の線分の最短距離(c1, c2 は、それぞれの線分上の最も近い点)
	float SegmentDistance(const VECTOR& p1, const VECTOR& q1, const VECTOR& p2, const VECTOR& q2,
		VECTOR& c1, VECTOR& c2)
	{
		const VECTOR d1 = VSub(q1, p1);
		const VECTOR d2 = VSub(q2, p2);
		const VECTOR r = VSub(p1, p2);
		const float a = VDot(d1, d1);
		const float e = VDot(d2, d2);
		const float f = VDot(d2, r);
		float s = 0.0f;
		float t = 0.0f;

		if (a <= 1.0e-6f && e <= 1.0e-6f)
		{
			s = 0.0f;
			t = 0.0f;
		}
		else if (a <= 1.0e-6f)
		{
			t = Clamp01(f / e);
		}
		else
		{
			const float c = VDot(d1, r);

			if (e <= 1.0e-6f)
			{
				s = Clamp01(-c / a);
			}
			else
			{
				const float bb = VDot(d1, d2);
				const float denom = a * e - bb * bb;

				s = (denom > 1.0e-6f) ? Clamp01((bb * f - c * e) / denom) : 0.0f;
				t = (bb * s + f) / e;

				if (t < 0.0f)
				{
					t = 0.0f;
					s = Clamp01(-c / a);
				}
				else if (t > 1.0f)
				{
					t = 1.0f;
					s = Clamp01((bb - c) / a);
				}
			}
		}

		c1 = VAdd(p1, VScale(d1, s));
		c2 = VAdd(p2, VScale(d2, t));

		return VSize(VSub(c1, c2));
	}
}

ClashSystem::ClashSystem(void)
{
}

ClashSystem::~ClashSystem(void)
{
}

void ClashSystem::Reset(void)
{
	state_ = State();
}

ClashSystem::RESULT ClashSystem::Update(
	float dt, Player& player, std::vector<std::unique_ptr<EnemyBase>>& enemies)
{
	if (!state_.active)
	{
		TryStart(player, enemies);
		return RESULT::NONE;
	}

	return UpdateActive(dt, player);
}

//------------------------------------------------------------
// 勝負の開始
//------------------------------------------------------------
void ClashSystem::TryStart(Player& player, std::vector<std::unique_ptr<EnemyBase>>& enemies)
{
	// かめはめ波のビームが出ていなければ、ぶつからない
	if (!player.IsKamehameBeam())
	{
		return;
	}

	const VECTOR beamStart = player.GetKamehameStartPos();
	const VECTOR beamEnd = player.GetKamehameEndPos();

	for (auto& enemy : enemies)
	{
		if (!enemy || enemy->IsDead())
		{
			continue;
		}

		MeleeEnemy* me = dynamic_cast<MeleeEnemy*>(enemy.get());
		if (me == nullptr)
		{
			continue;
		}

		Hit hit;

		// 大きな気弾 → ビームの順に調べる
		if (FindBigBlastHit(player, *me, beamStart, beamEnd, hit) ||
			FindBeamHit(player, *me, beamStart, beamEnd, hit))
		{
			Begin(player, *me, hit);
			return;
		}
	}
}

bool ClashSystem::FindBigBlastHit(
	const Player& player, MeleeEnemy& enemy,
	const VECTOR& beamStart, const VECTOR& beamEnd, Hit& hit) const
{
	VECTOR blastPos = { 0.0f, 0.0f, 0.0f };
	float blastRadius = 0.0f;
	int blastDamage = 0;

	if (!enemy.GetBigBlast(blastPos, blastRadius, blastDamage))
	{
		return false;
	}

	// 気弾の中心から、かめはめ波の線分までの最短距離を調べる
	const VECTOR beamVec = VSub(beamEnd, beamStart);
	const float len2 = VDot(beamVec, beamVec);

	float t = (len2 > 0.001f) ? VDot(VSub(blastPos, beamStart), beamVec) / len2 : 0.0f;
	t = Clamp01(t);

	const VECTOR closest = VAdd(beamStart, VScale(beamVec, t));

	if (VSize(VSub(blastPos, closest)) > blastRadius + player.GetKamehameRadius())
	{
		return false;
	}

	hit.isBeam = false;
	hit.pos = blastPos;
	hit.enemySide = VAdd(enemy.GetTransform().pos, VGet(0.0f, 100.0f, 0.0f));
	return true;
}

bool ClashSystem::FindBeamHit(
	const Player& player, MeleeEnemy& enemy,
	const VECTOR& beamStart, const VECTOR& beamEnd, Hit& hit) const
{
	VECTOR origin = { 0.0f, 0.0f, 0.0f };
	VECTOR dir = { 0.0f, 0.0f, 1.0f };
	float radius = 0.0f;
	int damage = 0;

	if (!enemy.GetBeam(origin, dir, radius, damage))
	{
		return false;
	}

	VECTOR c1 = { 0.0f, 0.0f, 0.0f };
	VECTOR c2 = { 0.0f, 0.0f, 0.0f };

	const float d = SegmentDistance(beamStart, beamEnd, origin, VAdd(origin, VScale(dir, 3000.0f)), c1, c2);

	// 敵のビームは太いので、中心線が少し近づけばぶつかったことにする
	if (d > radius * 0.6f + player.GetKamehameRadius())
	{
		return false;
	}

	hit.isBeam = true;
	hit.pos = VScale(VAdd(c1, c2), 0.5f);
	hit.enemySide = origin;
	return true;
}

void ClashSystem::Begin(Player& player, MeleeEnemy& enemy, const Hit& hit)
{
	state_.active = true;
	state_.enemy = &enemy;
	state_.isBeam = hit.isBeam;
	state_.pos = hit.pos;
	state_.enemySide = hit.enemySide;
	state_.rate = PlayerConfig::Clash::START_RATE;
	state_.timer = 0.0f;

	// 相手のビーム・気弾を止めて、当たらないようにする
	if (hit.isBeam)
	{
		enemy.SetBeamClash(true);
	}
	else
	{
		enemy.SetBigBlastFrozen(true);
	}

	// ビームのエフェクトは、ぶつかった所までにする(あとは自前で描く)
	EffekseerEffect::GetInstance()->SetKamehameEffectScale(
		PlayerConfig::Kamehame::BEAM_SCALE_XY,
		PlayerConfig::Kamehame::BEAM_SCALE_XY,
		VSize(VSub(hit.pos, player.GetKamehameStartPos())) / 30.0f);

	mainCamera.StartShake(0.2f, 6.0f);
}

//------------------------------------------------------------
// 勝負中
//------------------------------------------------------------
ClashSystem::RESULT ClashSystem::UpdateActive(float dt, Player& player)
{
	state_.timer += dt;

	UpdateRate(dt, player);

	// 相手がまだ生きているか(死んだり、気弾・ビームが消えたら勝負は終わり)
	// ついでに、負けたとき用に気弾・ビームのダメージも取っておく
	VECTOR blastPos = { 0.0f, 0.0f, 0.0f };
	VECTOR beamOrigin = { 0.0f, 0.0f, 0.0f };
	VECTOR beamDir = { 0.0f, 0.0f, 1.0f };
	float radius = 0.0f;
	int damage = 0;

	MeleeEnemy& enemy = *state_.enemy;

	const bool alive = !enemy.IsDead() &&
		(state_.isBeam ?
			enemy.GetBeam(beamOrigin, beamDir, radius, damage) :
			enemy.GetBigBlast(blastPos, radius, damage));

	// かめはめ波が途切れた(被弾など) / 敵や気弾がなくなった → 勝負を終える
	if (!alive || !player.IsKamehame())
	{
		Cancel(player, alive);
		return RESULT::NONE;
	}

	// 敵側の光線の出どころを更新する
	state_.enemySide = state_.isBeam ?
		beamOrigin :
		VAdd(enemy.GetTransform().pos, VGet(0.0f, 100.0f, 0.0f));

	MoveLight(dt, player);

	player.SetClash(true, state_.rate);

	// 決着
	const bool win = (state_.rate >= 1.0f);
	const bool lose = (state_.rate <= 0.0f) || (state_.timer >= PlayerConfig::Clash::MAX_TIME);

	if (win || lose)
	{
		Finish(win, damage, player);
		return win ? RESULT::WIN : RESULT::LOSE;
	}

	return RESULT::NONE;
}

void ClashSystem::UpdateRate(float dt, const Player& player)
{
	InputManager& ins = InputManager::GetInstance();

	// 連打で押し返す
	const bool press =
		ins.IsTrgDown(KEY_INPUT_F) ||
		ins.IsTrgDown(KEY_INPUT_U) ||
		PadInput::IsFightTrg() ||
		PadInput::IsKiBlastTrg();

	if (press)
	{
		state_.rate += PlayerConfig::Clash::PUSH_PER_PRESS;
	}

	// 敵は、時間とともに押してくる
	state_.rate -= PlayerConfig::Clash::ENEMY_PUSH_PER_SEC * dt;

	// ヒートアップ中は押し返しやすい
	if (player.IsHeatUp())
	{
		state_.rate += PlayerConfig::Clash::HEAT_BONUS_PER_SEC * dt;
	}

	if (state_.rate > 1.0f)
	{
		state_.rate = 1.0f;
	}
}

void ClashSystem::MoveLight(float dt, const Player& player)
{
	namespace cc = PlayerConfig::Clash;

	// 光の「行き先」を、優劣から決める
	//   rate が 0(負け) → 自分の手元寄り / rate が 1(勝ち) → 敵寄り
	const VECTOR start = player.GetKamehameStartPos();
	const float place = cc::POINT_MIN + (cc::POINT_MAX - cc::POINT_MIN) * state_.rate;
	const VECTOR target = VAdd(start, VScale(VSub(state_.enemySide, start), place));

	// いきなり飛ばず、行き先へなめらかに近づく
	state_.pos = VAdd(state_.pos, VScale(VSub(target, state_.pos), Clamp01(cc::POINT_FOLLOW * dt)));

	// 自分のビームのエフェクトを、光の位置まで伸び縮みさせる
	EffekseerEffect::GetInstance()->SetKamehameEffectScale(
		PlayerConfig::Kamehame::BEAM_SCALE_XY,
		PlayerConfig::Kamehame::BEAM_SCALE_XY,
		VSize(VSub(state_.pos, start)) / 30.0f);

	// 大きな気弾は、光と一緒に動かす
	if (!state_.isBeam)
	{
		state_.enemy->SetBigBlastPos(state_.pos);
	}
}

void ClashSystem::Cancel(Player& player, bool alive)
{
	// 相手のビーム・気弾を動かし直す
	if (state_.isBeam)
	{
		state_.enemy->SetBeamClash(false);
	}
	else if (alive)
	{
		state_.enemy->SetBigBlastFrozen(false);
	}

	state_.active = false;
	state_.enemy = nullptr;
	player.SetClash(false, 0.5f);
}

void ClashSystem::Finish(bool win, int enemyDamage, Player& player)
{
	const VECTOR hitPos = state_.pos;
	MeleeEnemy& enemy = *state_.enemy;

	// 相手の気弾・ビームを終わらせる
	if (state_.isBeam)
	{
		enemy.EndBeam();
	}
	else
	{
		enemy.KillBigBlast();
	}

	// 自分から敵への向き(水平)
	VECTOR dir = VSub(enemy.GetTransform().pos, player.GetTransform().pos);
	dir.y = 0.0f;
	dir = (VSize(dir) > 0.001f) ? VNorm(dir) : VGet(0.0f, 0.0f, 1.0f);

	EffekseerEffect::GetInstance()->PlayHitEffect(hitPos, 0.0f);

	if (win)
	{
		ApplyWin(enemy, dir, player);
	}
	else
	{
		ApplyLose(dir, enemyDamage, player);
	}

	state_.active = false;
	state_.enemy = nullptr;
	player.SetClash(false, 0.5f);
}

void ClashSystem::ApplyWin(MeleeEnemy& enemy, const VECTOR& dir, Player& player)
{
	// 押し勝った: 大きな気弾を押し返して、敵に大ダメージ
	const VECTOR enemyPos = enemy.GetTransform().pos;
	const int dmg = player.GetAttackDamage(PlayerConfig::Clash::WIN_DAMAGE);

	enemy.Damage(dmg);
	enemy.AddStun(PlayerConfig::Clash::WIN_STUN);
	player.AddDamagePopup(VAdd(enemyPos, VGet(0.0f, 130.0f, 0.0f)), dmg);

	enemy.AddKnockBack(VNorm(VGet(dir.x, 0.2f, dir.z)), PlayerConfig::Clash::WIN_KNOCK);

	EffekseerEffect::GetInstance()->
		PlayHitEffect(VAdd(enemyPos, VGet(0.0f, 80.0f, 0.0f)), 0.0f);

	mainCamera.StartShake(0.3f, 14.0f);

	// (追撃はブレイクしたときだけ)
}

void ClashSystem::ApplyLose(const VECTOR& dir, int enemyDamage, Player& player)
{
	// 押し負けた: 大きな気弾を受ける
	player.Damage(enemyDamage * PlayerConfig::Clash::LOSE_DAMAGE_RATE);
	player.AddKnockBack(dir, PlayerConfig::Clash::LOSE_KNOCK);

	mainCamera.StartShake(0.3f, 12.0f);
}

//------------------------------------------------------------
// 描画
//------------------------------------------------------------
void ClashSystem::Draw(const Player& player) const
{
	if (!state_.active || state_.enemy == nullptr)
	{
		return;
	}

	const VECTOR bs = player.GetKamehameStartPos();
	const VECTOR es = state_.enemySide;
	const VECTOR bp = state_.pos;

	const float tt = (float)GetNowCount() * 0.001f;
	const float pulse = 0.5f + 0.5f * sinf(tt * 40.0f);

	SetUseLighting(FALSE);

	// 自分側のビーム(青)
	SetDrawBlendMode(DX_BLENDMODE_ADD, 170);
	DrawCapsule3D(bs, bp, 24.0f + 5.0f * pulse, 10, GetColor(60, 140, 255), GetColor(60, 140, 255), TRUE);
	SetDrawBlendMode(DX_BLENDMODE_ADD, 230);
	DrawCapsule3D(bs, bp, 11.0f, 8, GetColor(230, 245, 255), GetColor(230, 245, 255), TRUE);

	// 敵側(紫)
	SetDrawBlendMode(DX_BLENDMODE_ADD, 170);
	DrawCapsule3D(es, bp, 24.0f + 5.0f * pulse, 10, GetColor(190, 70, 255), GetColor(190, 70, 255), TRUE);
	SetDrawBlendMode(DX_BLENDMODE_ADD, 230);
	DrawCapsule3D(es, bp, 11.0f, 8, GetColor(255, 225, 255), GetColor(255, 225, 255), TRUE);

	// ぶつかっている光の球
	SetDrawBlendMode(DX_BLENDMODE_ADD, 120);
	DrawSphere3D(bp, 80.0f + 30.0f * pulse, 20, GetColor(255, 220, 120), GetColor(255, 220, 120), TRUE);
	SetDrawBlendMode(DX_BLENDMODE_ADD, 200);
	DrawSphere3D(bp, 48.0f + 14.0f * pulse, 20, GetColor(255, 250, 200), GetColor(255, 250, 200), TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	DrawSphere3D(bp, 28.0f, 16, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);

	SetUseLighting(TRUE);
}