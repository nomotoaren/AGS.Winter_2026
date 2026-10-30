#pragma once
#include <cfloat>
#include <cmath>
#include <DxLib.h>
#include "../../Object/Player/Player.h"
#include "../../Object/Enemy/EnemyBase.h"

// GameScene の各ファイルで共通に使う定数と小さな関数
namespace GameSceneInternal
{
	//------------------------------------------------------------
	// 体の重なり(プレイヤーと敵を押し出す)
	//------------------------------------------------------------

	// プレイヤーと敵が重ならない最小の水平距離
	// (攻撃中に保つ距離 AttackDistance=90 より小さくしておく)
	constexpr float BODY_DISTANCE = 65.0f;
	// この高さ以上離れていたら重なりとみなさない(上や下にいるときは通す)
	constexpr float BODY_HEIGHT_RANGE = 130.0f;
	// 1フレームで押し出す最大量(急に飛ばないようにする)
	constexpr float BODY_MAX_PUSH = 25.0f;
	// 押し出しの配分(プレイヤー側 / 敵側)
	constexpr float BODY_PLAYER_RATE = 0.65f;

	//------------------------------------------------------------
	// 近接攻撃
	//------------------------------------------------------------

	// 攻撃が届く高さの差(これを超えると当たらない)
	constexpr float ATTACK_HEIGHT_RANGE = 150.0f;
	// 水平距離がこれ以下のときは向き判定をしない(真上・真下の敵用)
	constexpr float ATTACK_DOT_MIN_DISTANCE = 20.0f;
	// 8段目(叩き落とし)は上から叩くので広め
	constexpr int FINISH_COMBO = 8;
	constexpr float FINISH_RANGE = 280.0f;
	constexpr float FINISH_HEIGHT_RANGE = 300.0f;
	// スタン値の溜まり方
	constexpr float STUN_NORMAL = 8.0f;
	constexpr float STUN_FINISH = 40.0f;
	// 敵のガードを削る量(攻撃力の倍率がかかる)
	constexpr float GUARD_DAMAGE = 20.0f;
	// コンボ段ごとのふっ飛ばし(添え字 = 段数)
	constexpr float KNOCK_POWER[FINISH_COMBO + 1] =
	{ 0.0f, 5.0f, 7.0f, 5.0f, 7.0f, 9.0f, 7.0f, 4.0f, 100.0f };

	//------------------------------------------------------------
	// 敵の位置まわり
	//------------------------------------------------------------

	// 敵の足元から体の中心までの高さ(当たり判定の中心)
	constexpr float ENEMY_CENTER_HEIGHT = 70.0f;
	// 敵の当たり判定の半径
	constexpr float ENEMY_HIT_RADIUS = 45.0f;
	// ヒットエフェクトを出す高さ
	constexpr float HIT_EFFECT_HEIGHT = 80.0f;
	// ダメージ表示を出す高さ
	constexpr float POPUP_HEIGHT = 130.0f;

	//------------------------------------------------------------
	// 技
	//------------------------------------------------------------

	// ガードバースト
	constexpr float BURST_RANGE = 250.0f;
	constexpr float BURST_KNOCK = 40.0f;
	constexpr float BURST_SHAKE_TIME = 0.12f;
	constexpr float BURST_SHAKE_POWER = 6.0f;

	// 気弾
	constexpr float KI_BLAST_STUN = 10.0f;
	constexpr int KI_BLAST_DAMAGE_TYPE = 1;

	// 技パレットを開いている間の時間の速さ
	constexpr float PALETTE_SLOW_RATE = 0.2f;

	// フィニッシュ気弾
	constexpr float FINISH_BLAST_HIT_STOP = 0.08f;
	constexpr float FINISH_BLAST_SHAKE_TIME = 0.12f;
	constexpr float FINISH_BLAST_SHAKE_POWER = 6.0f;

	// かめはめ波
	constexpr float BEAM_DAMAGE_INTERVAL = 0.15f;
	constexpr float BEAM_STUN = 4.0f;
	constexpr int BEAM_DAMAGE_TYPE = 3;

	//------------------------------------------------------------
	// 部品
	//------------------------------------------------------------

	inline VECTOR EnemyCenter(EnemyBase& enemy)
	{
		return VAdd(enemy.GetTransform().pos, VGet(0.0f, ENEMY_CENTER_HEIGHT, 0.0f));
	}

	inline VECTOR HitEffectPos(EnemyBase& enemy)
	{
		return VAdd(enemy.GetTransform().pos, VGet(0.0f, HIT_EFFECT_HEIGHT, 0.0f));
	}

	inline VECTOR PopupPos(EnemyBase& enemy)
	{
		return VAdd(enemy.GetTransform().pos, VGet(0.0f, POPUP_HEIGHT, 0.0f));
	}

	// 長さが0に近いときは fallback を返す
	inline VECTOR NormalizeOr(const VECTOR& v, const VECTOR& fallback)
	{
		return (VSize(v) > 0.001f) ? VNorm(v) : fallback;
	}

	// バニッシュアタックの移動先: 敵の斜め後ろのうち、今のプレイヤーに近いほう
	//   back = 敵の向きの後ろ側(水平)。少し横にずらして、カメラが大きく回らないようにする
	inline VECTOR CalcVanishPos(const VECTOR& enemyPos, const VECTOR& back, const VECTOR& playerPos)
	{
		const float rad = PlayerConfig::Vanish::SIDE_ANGLE_DEG * DX_PI_F / 180.0f;

		VECTOR warpPos = enemyPos;
		float bestDist = FLT_MAX;

		// 真後ろから、左右どちらかへずらした2つの候補
		for (int k = 0; k < 2; k++)
		{
			const float ang = (k == 0) ? rad : -rad;

			// 敵の向きを ang だけ回した向きの「反対側」(= 斜め後ろ)
			const VECTOR dir = VGet(
				back.x * cosf(ang) - back.z * sinf(ang),
				0.0f,
				back.x * sinf(ang) + back.z * cosf(ang));

			VECTOR cand = VSub(enemyPos, VScale(dir, PlayerConfig::Vanish::BEHIND_DISTANCE));
			cand.y = enemyPos.y + PlayerConfig::Vanish::ABOVE_HEIGHT;

			const float d = VSize(VSub(cand, playerPos));

			if (d < bestDist)
			{
				bestDist = d;
				warpPos = cand;
			}
		}

		return warpPos;
	}
}
