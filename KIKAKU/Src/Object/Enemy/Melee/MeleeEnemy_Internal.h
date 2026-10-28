#pragma once
// MeleeEnemy の .cpp(MeleeEnemy.cpp / MeleeEnemy_Melee.cpp / MeleeEnemy_Special.cpp / MeleeEnemy_AI.cpp / MeleeEnemy_Draw.cpp)で
// 共通に使う定数と小さな部品。MeleeEnemy.cpp が長くなりすぎたので、ファイルを分けるときにここへまとめた
#include <cmath>
#include <cstddef>
#include <algorithm>
#include <DxLib.h>
#include "../../../Manager/SceneManager.h"
#include "MeleeEnemy.h"
#include "../definition/EnemyUtil.h"

namespace MeleeEnemyInternal
{
	// 共通の部品(EnemyUtil)も、この名前空間から使えるようにする
	using namespace EnemyUtil;

	// 高さの範囲
	constexpr float kMinHeight = 100.0f;
	constexpr float kMaxHeight = 2000.0f;

	// かめはめ波で押される速さ
	constexpr float kKamehamePushStartSpeed = 3.0f;
	constexpr float kKamehamePushAccel = 0.35f;
	constexpr float kKamehamePushMaxSpeed = 12.0f;

	// ノックバック
	constexpr float kKnockBackThreshold = 0.1f;
	constexpr float kKnockBackDamping = 0.85f;
	constexpr float kKnockBackDamageTime = 0.18f;

	// ダウン
	constexpr float kDownTime = 1.0f;

	// ガード
	constexpr float kGuardMaxHp = 100.0f;
	constexpr float kGuardTime = 0.8f;
	constexpr float kGuardBreakTime = 2.0f;
	constexpr float kGuardCoolTime = 2.0f;
	constexpr float kGuardBreakStun = 35.0f;		// ガードを割られたときに溜まるスタンゲージ

	// プレイヤーの攻撃への反応(攻撃が始まった瞬間に、1回だけ決める)
	constexpr float kReactDistance = 220.0f;		// この距離以内で攻撃されたら反応する
	constexpr int   kDodgeChance = 25;				// かわす確率(%)
	constexpr int   kGuardChance = 0;				// ガードする確率(%)。この敵はガードしないので 0
	constexpr float kDodgeCoolTime = 5.0f;			// 回避の連発を防ぐ時間(秒)
	constexpr float kDodgeBehindDistance = 110.0f;	// 回避で、プレイヤーの背後のこの距離へ移動する
	constexpr float kDodgeInvincibleTime = 0.35f;	// 回避の直後に、攻撃を受けない時間(秒)

	// 近接攻撃
	constexpr int   kMaxCombo = 4;
	constexpr int   kFollowComboMax = 3;			// 1~3段目はプレイヤーに付いていく
	constexpr float kComboInterval = 0.55f;
	constexpr float kAttackDistance = 90.0f;
	constexpr float kAttackFollowRate = 0.25f;
	constexpr float kLookPitchRate = 0.35f;			// 見上げ・見下ろしの最大比率
	constexpr float kMeleeHitRange = 140.0f;		// 近接攻撃が当たる水平距離
	constexpr float kMeleeHitHeight = 150.0f;		// 近接攻撃が当たる高さの差
	constexpr int   kComboDamage = 10;
	constexpr int   kFinisherDamage = 30;			// 4段目

	// 気弾
	constexpr float kKiChargeTime = 0.8f;			// 気を溜める時間(秒)。この間に避ける・近づける
	constexpr float kKiSpeed = 10.0f;				// 気弾の速さ(1/60秒あたり)
	constexpr float kKiRadius = 18.0f;
	constexpr float kKiLife = 4.0f;					// 気弾が消えるまでの時間(秒)
	constexpr float kKiCoolTime = 6.0f;				// 次に気弾を撃てるまでの時間(秒)
	constexpr int   kKiDamage = 30;
	constexpr float kBlastHitMargin = 35.0f;		// プレイヤーの体の大きさ(気弾との当たり判定に足す)
	constexpr float kPlayerCenterHeight = 70.0f;	// プレイヤーの足元から体の中心までの高さ
	constexpr float kChestHeight = 90.0f;			// 敵の足元から胸までの高さ

	// 突進
	constexpr float kRushReadyTime = 1.3f;			// 構えて溜める時間(秒)。殴ってもひるまず、この間に避ける
	constexpr float kRushSpeed = 22.0f;				// 突進の速さ(1/60秒あたり)
	constexpr float kRushTime = 0.6f;				// 突進する最大時間(秒)
	constexpr float kRushHitRange = 90.0f;			// プレイヤーに当たる距離
	constexpr float kRushCoolTime = 9.0f;
	constexpr int   kRushDamage = 50;

	// 距離による行動の選び方
	constexpr float kNearDistance = 400.0f;			// これより近い: 近接寄り
	constexpr float kFarDistance = 650.0f;			// これより遠い: 高速接近か気弾

	// AI
	constexpr float kWaitTime = 1.2f;
	constexpr float kSideMoveTime = 1.3f;
	constexpr float kArriveDistance = 10.0f;
	constexpr float kBoostStopDistance = 110.0f;
	constexpr float kAttackApproachSpeed = 8.0f;	// 近接攻撃へ近づく速さ(1/60秒あたり)
	constexpr float kAttackStartSlack = 60.0f;		// 攻撃位置のこの距離まで近づけば殴り始める(完全に着くまで待たない)
	constexpr float kAttackGiveUpTime = 1.6f;		// これだけ追っても届かなければ、諦めて別の行動へ
	constexpr float kAttackGiveUpCool = 2.5f;		// 諦めたあと、近接攻撃を選ばない時間(秒)
	constexpr float kRecoverTime = 0.4f;			// 気弾・突進のあとの隙(秒)

	// 空中での旋回(プレイヤーの周りを、距離を変えながら回る)
	constexpr float kOrbitAngularSpeed = 0.9f;		// 1秒に回る角度(ラジアン)
	constexpr float kOrbitMaxSpeed = 4.5f;			// 1/60秒あたりの最大移動量
	constexpr float kOrbitMinRadius = 140.0f;
	constexpr int   kOrbitRadiusRange = 100;		// 半径は kOrbitMinRadius ~ +この値
	constexpr float kOrbitHeightMin = -80.0f;		// プレイヤーとの高さの差の最小
	constexpr int   kOrbitHeightRange = 300;		// 高さの差は kOrbitHeightMin ~ +この値
	constexpr float kOrbitRadiusLerp = 0.05f;
	constexpr float kOrbitHeightLerp = 0.04f;
	constexpr float kWaitDrift = 0.35f;				// 様子見の間は、旋回をこの倍率でゆっくり行う
	constexpr float kHoverBobAmp = 25.0f;			// 様子見の間の上下のふわふわ
	constexpr float kHoverBobSpeed = 2.0f;
	constexpr float kWaitMinRadius = 140.0f;
	constexpr float kWaitMaxRadius = 240.0f;

	// 追撃(吹き飛ばした相手の先へ回り込んで、叩き落とす)
	constexpr int   kChaseChance = 70;				// 追撃に入る確率(%)
	constexpr float kChaseCoolTime = 7.0f;
	constexpr float kChaseStartDelay = 0.1f;		// 様子見に戻ってから追撃に入るまで
	constexpr float kChasePendingLimit = 2.0f;		// 追撃の予定が残る時間
	constexpr float kChaseVanishTime = 0.35f;		// 姿を消している時間
	constexpr float kChaseAppearDistance = 130.0f;	// 吹き飛ぶ向きの先、この距離に現れる
	constexpr float kChaseAppearHeight = 70.0f;		// プレイヤーより上に現れる高さ
	constexpr float kChaseWindupTime = 0.4f;		// 現れてから叩き落とすまで(ガード・回避できる)
	constexpr float kChaseRecoverTime = 0.7f;
	constexpr float kSmashRange = 190.0f;
	constexpr float kSmashFromHeight = 150.0f;		// 上から叩くので、吹き飛ぶ向きは下になる
	constexpr int   kSmashDamage = 20;

	// ガード崩し(投げ)
	constexpr float kGuardWatchRange = 450.0f;		// この距離以内でガードされ続けると狙う
	constexpr float kGuardWatchTrigger = 1.2f;		// この秒数ガードされたら、投げに来る
	constexpr float kGrabCoolTime = 9.0f;
	constexpr float kGrabReadyTime = 0.6f;			// 構えて接近する時間(ここで避けられる)
	constexpr float kGrabApproachSpeed = 8.0f;
	constexpr float kGrabHitRange = 150.0f;
	constexpr int   kGrabDamage = 30;
	constexpr float kGrabKnock = 45.0f;
	constexpr float kGrabRecover = 0.6f;
	constexpr float kGrabMissRecover = 1.0f;		// 外したときの大きな隙

	// 範囲大技
	constexpr float kBigChargeTime = 1.3f;			// 溜め。ここで殴れば中断できる
	// 反撃(殴られ続けたとき、プレイヤーを吹き飛ばす)
	constexpr int   kBurstHits = 7;					// この回数殴られたら反撃
	constexpr float kBurstWindow = 2.5f;			// 殴られた回数を数える時間
	constexpr float kBurstRange = 320.0f;			// 反撃が届く距離
	constexpr float kBurstCoolTime = 5.0f;
	constexpr float kBurstFxTime = 1.2f;
	constexpr int   kBurstDamage = 8;
	constexpr float kBurstKnock = 90.0f;			// 吹き飛ばす強さ(約450の距離。壁を突き抜けない程度)
	constexpr float kBurstStun = 0.8f;				// 吹き飛ばされたあと、プレイヤーが動けない時間(秒)

	// 敵の気(見えない)
	constexpr float kEnemyKiMax = 100.0f;
	constexpr float kEnemyKiRegen = 7.0f;			// 1秒あたりの回復量

	constexpr float kBigCoolTime = 12.0f;
	constexpr float kBigRadius = 190.0f;
	constexpr float kBigWarnTime = 1.2f;			// 警告が出てから爆発するまで
	constexpr float kBigStagger = 0.3f;				// 複数出すときの、時間のずれ
	constexpr float kBigSpread = 260.0f;			// 複数出すときの、位置のずれ
	constexpr int   kBigDamage = 60;
	constexpr float kBigRecoverTime = 0.9f;
	constexpr float kBigRiseSpeed = 0.6f;			// 溜めている間に浮き上がる(1/60秒あたり)

	// 極太ビーム
	constexpr float kBeamChargeTime = 1.8f;			// 溜めの長さ(殴れば中断できる)
	constexpr float kBeamLockTime = 1.1f;			// 溜め始めてからこの時間で、狙いを固定する
	constexpr float kBeamFireTime = 2.2f;			// 発射している時間(秒)
	constexpr float kBeamCoolTime = 14.0f;
	constexpr float kBeamRadius = 110.0f;			// ビームの太さ(当たり判定)
	constexpr float kBeamLength = 3000.0f;
	constexpr float kBeamHitMargin = 35.0f;			// プレイヤーの体の大きさ
	constexpr float kBeamBallOffset = 70.0f;		// 溜める球の位置(胸の前)
	constexpr float kBeamGrowTime = 0.12f;			// 発射してからビームが伸びきるまで
	constexpr float kBeamFadeTime = 0.2f;			// 発射の終わりに細くなる時間
	constexpr int   kBeamDamage = 40;
	constexpr float kBeamTickTime = 0.45f;			// 当たり続けたときの、ダメージの間隔
	constexpr float kBeamTrackRate = 1.2f;			// 発射中に、狙いがプレイヤーを追う強さ(大きいほど追う)
	constexpr float kBeamKnock = 30.0f;
	constexpr float kBeamRecoverTime = 1.0f;		// 撃ったあとの大きな隙

	// 技の名前(画面右上に帯で出す)
	constexpr const char* kNameBeam = "ギャリック砲";
	constexpr const char* kNameBarrage = "連続エネルギー弾";
	constexpr const char* kNameBarrageBig = "ビッグバンアタック";
	constexpr const char* kNameBigBlast = "ギガンティックボム";
	constexpr const char* kNameSlam = "メテオダイブ";

	// 気弾の連射
	constexpr float kBarrageChargeTime = 1.0f;		// 赤いオーラをまとって気を溜める時間(この間、上へ昇る)
	constexpr float kBarrageInterval = 0.035f;		// 1発ごとの間隔(1秒に約28発)
	constexpr int   kBarrageCount = 50;				// 追尾する気弾の数
	constexpr int   kBarrageCountEnraged = 70;		// HPが半分以下のとき
	constexpr float kBarrageSpeed = 24.0f;			// 1/60秒あたり(単発の気弾より少し遅い)
	constexpr float kBarrageSpread = 280.0f;			// 狙いのばらつき(広く散らす)
	constexpr int   kBarrageDirectChance = 25;		// プレイヤーを直接狙う弾の割合(%)
	constexpr float kBarrageDirectSpread = 40.0f;	// 直接狙う弾のばらつき
	constexpr float kBarrageOriginSpread = 160.0f;	// 弾が出る位置のばらつき(敵の周りの雲から出る)
	constexpr float kBarrageRiseSpeed = 180.0f;		// 溜めている間に昇る速さ(1秒あたり)
	constexpr float kBarrageHomingTurn = 3.0f;		// 追尾で曲がる強さ(1秒あたり)
	constexpr float kBarrageHomingDelay = 0.35f;	// 放射状に飛んでから曲がり始めるまでの時間(秒)
	constexpr float kBarrageHomingTime = 2.0f;		// 追尾する時間(秒)。過ぎたら直進するので避けられる

	// 連射のあとの、大きな気弾
	constexpr float kBarrageBigChargeTime = 1.2f;	// 大きな気弾を溜める時間(秒)。この間に近づいて殴れる
	constexpr float kBarrageBigRadius = 55.0f;
	constexpr float kBarrageBigSpeed = 8.0f;
	constexpr int   kBarrageBigDamage = 40;
	constexpr float kBarrageBigHomingTurn = 2.0f;
	constexpr float kBarrageBigHomingTime = 3.0f;
	constexpr int   kBarrageDamage = 6;
	constexpr float kBarrageCoolTime = 12.0f;
	constexpr float kBarrageRecoverTime = 1.0f;

	// 急降下(上昇 → 狙いを固定 → 急降下)
	constexpr float kSlamHeight = 420.0f;			// プレイヤーより上へ上がる高さ
	constexpr float kSlamRiseSpeed = 8.0f;			// 1/60秒あたり
	constexpr float kSlamChargeTime = 0.9f;			// 上昇して溜める時間
	constexpr float kSlamLockTime = 0.4f;			// 狙いを固定してから降下するまでの時間(ここで避ける)
	constexpr float kSlamDiveSpeed = 40.0f;			// 1/60秒あたり
	constexpr float kSlamDiveMaxTime = 0.7f;
	constexpr float kSlamHitRange = 110.0f;			// 降下中に直接当たる距離
	constexpr float kSlamArrive = 70.0f;			// 狙った場所へ着いたとみなす距離
	constexpr int   kSlamDamage = 40;				// 直接当たったとき
	constexpr float kSlamImpactRadius = 220.0f;		// 衝撃波の半径
	constexpr int   kSlamImpactDamage = 30;			// 衝撃波(直接当たったときは入らない)
	constexpr float kSlamCoolTime = 13.0f;
	constexpr float kSlamRecoverTime = 1.0f;

	// 高速移動の軌跡
	constexpr float kTrailLife = 0.35f;
	constexpr float kTrailCut = 300.0f;				// これ以上離れたら、つながない
	constexpr size_t kTrailMax = 40;

	// 行動の種類(DecideAction の中だけで使う)
	enum ACTION
	{
		ACT_ATTACK,		// 近づいて近接攻撃
		ACT_SIDE,		// 横移動
		ACT_KI,			// 気弾
		ACT_RUSH,		// 突進
		ACT_BOOST,		// 高速接近
		ACT_GRAB,		// 投げ(ガード不能)
		ACT_BIG,		// 範囲大技
		ACT_BEAM,		// 極太ビーム
		ACT_BARRAGE,	// 気弾の連射
		ACT_SLAM,		// 急降下
		ACT_MAX
	};
}
