#include <DxLib.h>
#include <cmath>
#include <algorithm>
#include "MeleeEnemy.h"
#include "../../Application.h"
#include "../Player/Player.h"
#include "../../Manager/Camera.h"
#include "../../Manager/EffekseerEffect.h"
#include "../../Manager/ResourceManager.h"
#include "../../Manager/SceneManager.h"
#include "../../Utility/AsoUtility.h"

//======================================================================
// 定数・ヘルパー(このcppの中だけで使う)
//======================================================================
namespace
{
	// 「ゼロ」とみなす長さ
	constexpr float kEpsilon = 0.001f;

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
	constexpr float kGuardBreakStun = 35.0f;		// ガードを崩されたときに増えるスタンゲージ

	// プレイヤーの攻撃への反応(攻撃が始まった瞬間に、1回だけ決める)
	constexpr float kReactDistance = 220.0f;		// この距離以内で攻撃されたら反応する
	constexpr int   kDodgeChance = 25;				// 回避する確率(%)
	constexpr int   kGuardChance = 0;				// ガードする確率(%)。カカロットの敵はガードしないので 0
	constexpr float kDodgeCoolTime = 5.0f;			// 回避の連発を防ぐ時間(秒)
	constexpr float kDodgeBehindDistance = 110.0f;	// 回避で、プレイヤーの背後のこの距離へ移動する
	constexpr float kDodgeInvincibleTime = 0.35f;	// 回避の直後に、攻撃を受けない時間(秒)

	// 近接攻撃
	constexpr int   kMaxCombo = 4;
	constexpr int   kFollowComboMax = 3;			// 1~3段目はプレイヤーに付いてくる
	constexpr float kComboInterval = 0.55f;
	constexpr float kAttackDistance = 90.0f;
	constexpr float kAttackFollowRate = 0.25f;
	constexpr float kLookPitchRate = 0.35f;			// 見上げ・見下ろしの最大比率
	constexpr float kMeleeHitRange = 140.0f;		// 近接攻撃が当たる水平距離
	constexpr float kMeleeHitHeight = 150.0f;		// 近接攻撃が当たる高さの差
	constexpr float kEnemyDamageRate = 1.0f;		// 敵の攻撃全体のダメージ倍率(0.5 なら半分)。まず全体の強さはここで調整する
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
	constexpr float kRushReadyTime = 1.3f;			// 赤く光って溜める時間(秒)。殴ってもひるまず、この間に避けられる
	constexpr float kRushSpeed = 22.0f;				// 突進の速さ(1/60秒あたり)
	constexpr float kRushTime = 0.6f;				// 突進する最大時間(秒)
	constexpr float kRushHitRange = 90.0f;			// プレイヤーに当たる距離
	constexpr float kRushCoolTime = 9.0f;
	constexpr int   kRushDamage = 50;

	// 距離による行動の選び方
	constexpr float kNearDistance = 400.0f;			// これより近い: 近接戦
	constexpr float kFarDistance = 650.0f;			// これより遠い: 高速接近か気弾

	// 足元の円(SYNC)
	constexpr int   kRingSegment = 48;
	constexpr float kRingRadius = 80.0f;

	// AI
	constexpr float kNormalBattleDistance = 180.0f;	// 普段戦う距離
	constexpr float kWaitTime = 1.2f;
	constexpr float kSideMoveTime = 1.3f;
	constexpr float kArriveDistance = 10.0f;
	constexpr float kMoveWaitTime = 0.5f;
	constexpr float kBoostStopDistance = 110.0f;
	constexpr float kAttackApproachSpeed = 8.0f;	// 近接攻撃へ近づく速さ(1/60秒あたり)
	constexpr float kAttackStartSlack = 60.0f;		// 攻撃位置のこの距離まで近づけば殴り始める(完全に着くまで待たない)
	constexpr float kAttackGiveUpTime = 1.6f;		// これだけ追っても届かなければ、諦めて別の行動へ
	constexpr float kAttackGiveUpCool = 2.5f;		// 諦めたあと、近接攻撃を選ばない時間(秒)
	constexpr float kRecoverTime = 0.4f;			// 気弾・突進のあとの隙(秒)

	// 空中での旋回(プレイヤーの周りを、高さも変えながら回る)
	constexpr float kOrbitAngularSpeed = 0.9f;		// 1秒に回る角度(ラジアン)
	constexpr float kOrbitMaxSpeed = 4.5f;			// 1/60秒あたりの最大移動量
	constexpr float kOrbitMinRadius = 140.0f;
	constexpr int   kOrbitRadiusRange = 100;		// 半径は kOrbitMinRadius ~ +この値
	constexpr float kOrbitHeightMin = -80.0f;		// プレイヤーとの高さの差の最小
	constexpr int   kOrbitHeightRange = 300;		// 高さの差は kOrbitHeightMin ~ +この値
	constexpr float kOrbitRadiusLerp = 0.05f;
	constexpr float kOrbitHeightLerp = 0.04f;
	constexpr float kWaitDrift = 0.35f;				// 様子見の間は、旋回をこの倍率でゆっくり行う
	constexpr float kHoverBobAmp = 25.0f;			// 様子見の間の上下のふらつき
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
	constexpr float kGuardWatchRange = 450.0f;		// この距離以内でガードされ続けると数える
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
	constexpr float kBurstKnock = 90.0f;			// 吹き飛ばす強さ(約450の距離。すぐ殴り返せない)
	constexpr float kBurstStun = 0.8f;				// 吹き飛ばされたあと、プレイヤーが動けない時間(秒)

	// 敵の気(見えない)
	constexpr float kEnemyKiMax = 100.0f;
	constexpr float kEnemyKiRegen = 7.0f;			// 1秒あたりの回復量

	constexpr float kBigCoolTime = 12.0f;
	constexpr float kBigRadius = 190.0f;
	constexpr float kBigWarnTime = 1.2f;			// 警告が出てから爆発するまで
	constexpr float kBigStagger = 0.3f;				// 複数出すときの、爆発のずれ
	constexpr float kBigSpread = 260.0f;			// 複数出すときの、横のずれ
	constexpr int   kBigDamage = 60;
	constexpr float kBigRecoverTime = 0.9f;
	constexpr float kBigRiseSpeed = 0.6f;			// 溜めている間に浮き上がる(1/60秒あたり)

	// 極太ビーム
	constexpr float kBeamChargeTime = 1.8f;			// 溜めの長さ(殴れば中断できる)
	constexpr float kBeamLockTime = 1.1f;			// 溜め始めてからこの時間で、狙いを固定する
	constexpr float kBeamFireTime = 2.2f;			// 発射している時間(長い)
	constexpr float kBeamCoolTime = 14.0f;
	constexpr float kBeamRadius = 110.0f;			// ビームの太さ(当たり判定)
	constexpr float kBeamLength = 3000.0f;
	constexpr float kBeamHitMargin = 35.0f;			// プレイヤーの体の大きさ
	constexpr float kBeamBallOffset = 70.0f;		// 溜める球の位置(胸の前)
	constexpr float kBeamGrowTime = 0.12f;			// 発射してからビームが伸びきるまで
	constexpr float kBeamFadeTime = 0.2f;			// 発射の終わりに細くなる時間
	constexpr int   kBeamDamage = 40;
	constexpr float kBeamTickTime = 0.45f;			// 当たり続けたときの、ダメージの間隔
	constexpr float kBeamTrackRate = 1.2f;			// 発射中に、狙いがプレイヤーを追う速さ(大きいほど追う)
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
	constexpr float kBarrageInterval = 0.035f;		// 1秒に約28発		// 1発ごとの間隔
	constexpr int   kBarrageCount = 50;				// 追尾する気弾の数
	constexpr int   kBarrageCountEnraged = 70;		// HPが半分以下のとき
	constexpr float kBarrageSpeed = 24.0f;			// 1/60秒あたり(単発の気弾より少し遅い)
	constexpr float kBarrageSpread = 280.0f;			// 狙いのばらつき(広く散らす)
	constexpr int   kBarrageDirectChance = 25;		// プレイヤーを追い続ける弾の割合(%)
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
	constexpr float kMarkerRadius = 70.0f;			// 着弾点の輪の最大の大きさ
	constexpr float kMarkerSlack = 0.25f;			// 気弾が着いたあと、輪が残る時間

	// 急降下(上昇 → 狙いを固定 → 急降下)
	constexpr float kSlamHeight = 420.0f;			// プレイヤーより上へ上がる高さ
	constexpr float kSlamRiseSpeed = 8.0f;			// 1/60秒あたり
	constexpr float kSlamChargeTime = 0.9f;			// 上昇して溜める時間
	constexpr float kSlamLockTime = 0.4f;			// 狙いを固定してから降下までの時間(ここで避ける)
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
	constexpr float kTrailCut = 300.0f;				// これ以上離れたら、つなげない
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

	// Y成分を捨てる
	VECTOR ToHorizontal(VECTOR v)
	{
		v.y = 0.0f;
		return v;
	}

	// タイマーを減らして、0未満にならないようにする
	void CountDown(float& timer, float deltaTime)
	{
		if (timer > 0.0f)
		{
			timer -= deltaTime;

			if (timer < 0.0f)
			{
				timer = 0.0f;
			}
		}
	}

	// 水平方向の単位ベクトル。ほぼ真上・真下なら向いている方向を使う
	VECTOR HorizontalDirOrForward(VECTOR toTarget, const Quaternion& rot)
	{
		toTarget = ToHorizontal(toTarget);

		if (VSize(toTarget) > kEpsilon)
		{
			return VNorm(toTarget);
		}

		VECTOR forward = ToHorizontal(rot.GetForward());

		if (VSize(forward) <= kEpsilon)
		{
			forward = VGet(0.0f, 0.0f, 1.0f);
		}

		return VNorm(forward);
	}

	// 0.0~1.0 に収める
	float Saturate(float v)
	{
		if (v < 0.0f) { return 0.0f; }
		if (v > 1.0f) { return 1.0f; }
		return v;
	}

	// 点 p から線分 a-b までの最短距離
	float DistanceToSegment(const VECTOR& p, const VECTOR& a, const VECTOR& b)
	{
		const VECTOR ab = VSub(b, a);
		const float lenSq = VDot(ab, ab);

		float t = 0.0f;

		if (lenSq > kEpsilon)
		{
			t = Saturate(VDot(VSub(p, a), ab) / lenSq);
		}

		return VSize(VSub(p, VAdd(a, VScale(ab, t))));
	}

	// 水平な輪を線で描く
	void DrawRing3D(const VECTOR& center, float radius, unsigned int color)
	{
		constexpr int kSeg = 48;

		// 線は細いので、半径と高さをずらして重ねて太く見せる
		for (int k = 0; k < 5; k++)
		{
			const float r = radius + (float)(k - 2) * 2.5f;
			const float y = center.y + (float)(k % 2) * 1.5f;

			for (int i = 0; i < kSeg; i++)
			{
				const float a1 = DX_TWO_PI_F * (float)i / (float)kSeg;
				const float a2 = DX_TWO_PI_F * (float)(i + 1) / (float)kSeg;

				DrawLine3D(
					VGet(center.x + cosf(a1) * r, y, center.z + sinf(a1) * r),
					VGet(center.x + cosf(a2) * r, y, center.z + sinf(a2) * r),
					color);
			}
		}
	}

	// 重みの大きいものほど選ばれやすいランダム。選べるものが無ければ -1
	int PickWeighted(const int weights[], int count)
	{
		int total = 0;

		for (int i = 0; i < count; i++)
		{
			total += weights[i];
		}

		if (total <= 0)
		{
			return -1;
		}

		int r = GetRand(total - 1);

		for (int i = 0; i < count; i++)
		{
			if (r < weights[i])
			{
				return i;
			}

			r -= weights[i];
		}

		return count - 1;
	}
}

MeleeEnemy::MeleeEnemy(Player& player)
	:
	player_(player),
	targetPos({ 0.0f, 0.0f, 0.0f }),
	moveSpeed_(2.0f),
	isBoostChase_(false),
	boostChaseTimer_(0.0f),
	stopRange_(80.0f),
	isAttack_(false),
	attackCombo_(0),
	attackTimer_(0.0f),
	attackCoolTimer_(0.0f),
	isAttackWait_(false),
	attackWaitTimer_(0.0f),
	hasAttackHit_(false),
	isDamage_(false),
	damageTimer_(0.0f),
	isGuard_(false),
	guardTimer_(0.0f),
	guardHp_(kGuardMaxHp),
	isGuardBreak_(false),
	guardBreakTimer_(0.0f),
	guardCoolTimer_(0.0f),
	isSlamDown_(false),
	isDown_(false),
	downTimer_(0.0f)
{
	hp_ = 100;
	maxHp_ = hp_;
	hitRadius_ = 50.0f;
}

MeleeEnemy::~MeleeEnemy(void)
{
}

void MeleeEnemy::Init(void)
{
	// モデル
	transform_.SetModel(resMng_.LoadModelDuplicate(ResourceManager::SRC::ENEMY));

	InitAnimation();

	// 初期位置
	transform_.pos = { 300.0f, 1000.0f, 0.0f };
	transform_.scl = { 0.8f, 0.8f, 0.8f };

	transform_.quaRotLocal =
		Quaternion::Euler(0.0f, AsoUtility::Deg2RadF(180.0f), 0.0f);

	transform_.Update();

	InitHud("ENEMY", hp_);
}

void MeleeEnemy::InitAnimation(void)
{
	std::string path = Application::PATH_MODEL + "Enemy/Animation/";
	animationController_ = std::make_unique<AnimationController>(transform_.modelId);

	animationController_->Add((int)ANIM_TYPE::IDLE, path + "Idle.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::RUN, path + "Running.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::FAST_RUN, path + "Fast Run.mv1", 20.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK01, path + "Attack01.mv1", 45.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK02, path + "Attack02.mv1", 45.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK03, path + "Attack03.mv1", 45.0f);
	animationController_->Add((int)ANIM_TYPE::ATTACK04, path + "Attack04.mv1", 45.0f);
	animationController_->Add((int)ANIM_TYPE::DAMAGE, path + "Damege.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::GUARD, path + "Block.mv1", 60.0f);
	animationController_->Add((int)ANIM_TYPE::GUARD_BREAK, path + "GuardBreak.mv1", 20.0f);

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

void MeleeEnemy::Update(void)
{
	if (isDead_)
	{
		return;
	}

	UpdateHud();

	const float deltaTime = SceneManager::GetInstance().GetDeltaTime();

	// 撃った気弾は、敵が動けない間も飛び続ける
	UpdateBlasts(deltaTime);
	UpdateAreaBlasts(deltaTime);

	// 相殺勝負中は、その場から動かない(他の攻撃を受けても押されたりしない)
	if (clashLock_)
	{
		knockBackPow_ = AsoUtility::VECTOR_ZERO;
		animationController_->Update();
		transform_.Update();
		return;
	}

	animationController_->Update();

	CountDown(guardCoolTimer_, deltaTime);
	CountDown(attackCoolTimer_, deltaTime);
	CountDown(kiCoolTimer_, deltaTime);
	CountDown(rushCoolTimer_, deltaTime);
	CountDown(dodgeCoolTimer_, deltaTime);
	CountDown(invincibleTimer_, deltaTime);
	CountDown(bigCoolTimer_, deltaTime);
	CountDown(grabCoolTimer_, deltaTime);
	CountDown(chaseCoolTimer_, deltaTime);
	CountDown(beamCoolTimer_, deltaTime);
	CountDown(barrageCoolTimer_, deltaTime);
	CountDown(slamCoolTimer_, deltaTime);

	UpdateTrail(deltaTime);
	UpdateKiMarkers(deltaTime);

	hoverTime_ += deltaTime;

	// 追撃の予定は、しばらくしたら消える
	if (pendingChase_)
	{
		pendingChaseTimer_ -= deltaTime;

		if (pendingChaseTimer_ <= 0.0f)
		{
			pendingChase_ = false;
		}
	}

	// スタンゲージの更新(スタンが終わると OnStunEnd が呼ばれる)
	const bool isStunned = UpdateStunGauge(deltaTime);

	// 以下の関数は、そのフレームの更新を終えたときに true を返す
	if (UpdateKamehamePush()) { return; }

	UpdateCounterAndKi(deltaTime, isStunned);

	const VECTOR playerPos = player_.GetTransform().pos;
	const float distance = VSize(VSub(playerPos, transform_.pos));

	// プレイヤーが攻撃を始めた瞬間(コンボ数が 0 から増えたとき)だけ、反応を決める
	const int playerCombo = player_.GetCombo();
	const bool playerAttackStarted = (playerCombo > 0 && prevPlayerCombo_ == 0);
	prevPlayerCombo_ = playerCombo;

	UpdateGuardWatch(deltaTime, distance);

	if (isStunned) { UpdateStunned(deltaTime); return; }
	if (UpdateKnockBack()) { return; }
	if (UpdateDown(deltaTime)) { return; }
	if (UpdateDamage(deltaTime)) { return; }
	if (UpdateGuardBreak(deltaTime)) { return; }
	if (UpdateGuard(deltaTime)) { return; }
	if (playerAttackStarted && TryReactToPlayerAttack(distance)) { return; }
	if (UpdateAttack(deltaTime, playerPos)) { return; }
	if (UpdateKiCharge(deltaTime, playerPos)) { return; }
	if (UpdateRush(deltaTime, playerPos)) { return; }
	if (UpdateBigCharge(deltaTime, playerPos)) { return; }
	if (UpdateGrab(deltaTime, playerPos)) { return; }
	if (UpdateChase(deltaTime, playerPos)) { return; }
	if (UpdateBeam(deltaTime, playerPos)) { return; }
	if (UpdateKiBarrage(deltaTime, playerPos)) { return; }
	if (UpdateSlam(deltaTime, playerPos)) { return; }

	// プレイヤーを見る
	LookAtPlayerWithPitchLimit(playerPos);

	// AI
	const AIContext ai =
	{
		deltaTime,
		distance,
		playerPos,
		HorizontalDirOrForward(VSub(playerPos, transform_.pos), transform_.quaRot)
	};
	UpdateAI(ai);

	transform_.Update();
}

// かめはめ波で押されている間の処理
bool MeleeEnemy::UpdateKamehamePush(void)
{
	if (!isKamehameHit_)
	{
		return false;
	}

	kamehamePushSpeed_ += kKamehamePushAccel;

	if (kamehamePushSpeed_ > kKamehamePushMaxSpeed)
	{
		kamehamePushSpeed_ = kKamehamePushMaxSpeed;
	}

	transform_.pos =
		VAdd(transform_.pos, VScale(kamehamePushDir_, kamehamePushSpeed_));

	transform_.Update();
	return true;
}

// ノックバック中の処理(叩きつけられた場合は、地面へ着いたらダウンへ)
bool MeleeEnemy::UpdateKnockBack(void)
{
	if (VSize(knockBackPow_) <= kKnockBackThreshold)
	{
		knockBackPow_ = AsoUtility::VECTOR_ZERO;
		armoredHit_ = false;
		return false;
	}

	transform_.pos = VAdd(transform_.pos, knockBackPow_);

	// 下限(地面)
	if (transform_.pos.y <= kMinHeight)
	{
		transform_.pos.y = kMinHeight;

		// 叩きつけられて地面に着いたらダウン
		if (isSlamDown_)
		{
			isSlamDown_ = false;
			isDown_ = true;
			downTimer_ = kDownTime;

			knockBackPow_ = AsoUtility::VECTOR_ZERO;

			animationController_->Play((int)ANIM_TYPE::DAMAGE, false);

			transform_.Update();
			return true;
		}

		knockBackPow_.y = 0.0f;
	}

	// 上限
	if (transform_.pos.y > kMaxHeight)
	{
		transform_.pos.y = kMaxHeight;
		knockBackPow_.y = 0.0f;
	}

	knockBackPow_ = VScale(knockBackPow_, kKnockBackDamping);

	// ひるまない攻撃: 少し押されるだけで、行動は中断しない
	if (armoredHit_)
	{
		transform_.Update();
		return false;
	}

	if (isDamage_)
	{
		// 殴られている間は、よろめきが続く(動かない)
		damageTimer_ = kKnockBackDamageTime;
	}
	else
	{
		isDamage_ = true;
		damageTimer_ = kKnockBackDamageTime;

		// 殴られたら、気弾や突進の途中でも中断される
		CancelSpecials();

		animationController_->Play((int)ANIM_TYPE::DAMAGE, false);
	}

	transform_.Update();
	return true;
}

// ダウン中の処理
bool MeleeEnemy::UpdateDown(float deltaTime)
{
	if (!isDown_)
	{
		return false;
	}

	downTimer_ -= deltaTime;

	if (downTimer_ <= 0.0f)
	{
		downTimer_ = 0.0f;
		isDown_ = false;
		isDamage_ = false;
		damageTimer_ = 0.0f;

		ReturnToWait();
	}

	transform_.Update();
	return true;
}

// ダメージモーション中の処理
bool MeleeEnemy::UpdateDamage(float deltaTime)
{
	if (!isDamage_)
	{
		return false;
	}

	transform_.quaRot = damageRot_;

	damageTimer_ -= deltaTime;

	if (damageTimer_ <= 0.0f)
	{
		damageTimer_ = 0.0f;
		isDamage_ = false;

		ReturnToWait();
		return false;
	}

	transform_.Update();
	animationController_->Update();
	return true;
}

// ガードブレイク中の処理(終わったら続けて次の処理へ)
bool MeleeEnemy::UpdateGuardBreak(float deltaTime)
{
	if (!isGuardBreak_)
	{
		return false;
	}

	guardBreakTimer_ -= deltaTime;

	if (guardBreakTimer_ <= 0.0f)
	{
		guardBreakTimer_ = 0.0f;
		isGuardBreak_ = false;

		guardHp_ = kGuardMaxHp;
		guardCoolTimer_ = kGuardCoolTime;

		ReturnToWait();
		return false;
	}

	transform_.Update();
	return true;
}

// ガード中の処理(終わったら続けて次の処理へ)
bool MeleeEnemy::UpdateGuard(float deltaTime)
{
	if (!isGuard_)
	{
		return false;
	}

	guardTimer_ -= deltaTime;

	if (guardTimer_ <= 0.0f)
	{
		guardTimer_ = 0.0f;
		isGuard_ = false;

		ReturnToWait();
		return false;
	}

	transform_.Update();
	return true;
}

// スタン中の処理(動けない。殴られて押されるだけ)
bool MeleeEnemy::UpdateStunned(float deltaTime)
{
	(void)deltaTime;

	if (VSize(knockBackPow_) > kKnockBackThreshold)
	{
		transform_.pos = VAdd(transform_.pos, knockBackPow_);

		if (transform_.pos.y < kMinHeight) { transform_.pos.y = kMinHeight; }
		if (transform_.pos.y > kMaxHeight) { transform_.pos.y = kMaxHeight; }

		knockBackPow_ = VScale(knockBackPow_, kKnockBackDamping);
	}
	else
	{
		knockBackPow_ = AsoUtility::VECTOR_ZERO;
	}

	transform_.Update();

	// スタン中のモーション(GuardBreak)を進める
	animationController_->Update();

	return true;
}

// スタンが始まった: 今の行動をすべてやめて、よろめく
void MeleeEnemy::OnStunStart(void)
{
	ResetAttack();
	CancelBoostChase();
	CancelSpecials();

	isGuard_ = false;
	guardTimer_ = 0.0f;
	isDamage_ = false;
	damageTimer_ = 0.0f;

	animationController_->Play((int)ANIM_TYPE::GUARD_BREAK, false);
}

// スタンが終わった: 様子見に戻る
void MeleeEnemy::OnStunEnd(void)
{
	isSlamDown_ = false;

	SetAIState(AI_STATE::WAIT);
	animationController_->Play((int)ANIM_TYPE::IDLE);
}

//----------------------------------------------------------------------
// プレイヤーの攻撃への反応(ガード / 回避)
//----------------------------------------------------------------------

// プレイヤーが攻撃を始めたときに、1回だけ「回避する / ガードする / 何もしない」を決める
bool MeleeEnemy::TryReactToPlayerAttack(float distance)
{
	// 必殺技の最中は、ガードや回避に切り替えない
	if (IsSuperArmor())
	{
		return false;
	}

	if (distance > kReactDistance)
	{
		return false;
	}

	const int roll = GetRand(99);

	if (dodgeCoolTimer_ <= 0.0f && roll < kDodgeChance)
	{
		StartDodge();
		return true;
	}

	if (guardCoolTimer_ <= 0.0f && roll < kDodgeChance + kGuardChance)
	{
		StartGuard();
		return true;
	}

	return false;
}

void MeleeEnemy::StartGuard(void)
{
	isGuard_ = true;
	guardTimer_ = kGuardTime;

	ResetAttack();
	CancelBoostChase();
	CancelSpecials();

	SetAIState(AI_STATE::WAIT);

	animationController_->Play((int)ANIM_TYPE::GUARD, true, 0.0f, -1.0f, true, true);

	transform_.Update();
}

// プレイヤーの背後へ瞬間移動して、すぐ反撃する
void MeleeEnemy::StartDodge(void)
{
	dodgeCoolTimer_ = kDodgeCoolTime;
	invincibleTimer_ = kDodgeInvincibleTime;
	justDodged_ = true;

	ResetAttack();
	CancelBoostChase();
	CancelSpecials();

	// プレイヤーの背後へ
	const VECTOR playerPos = player_.GetTransform().pos;
	const VECTOR forward = HorizontalDirOrForward(player_.GetForward(), transform_.quaRot);

	VECTOR newPos = VSub(playerPos, VScale(forward, kDodgeBehindDistance));
	newPos.y = playerPos.y;

	if (newPos.y < kMinHeight)
	{
		newPos.y = kMinHeight;
	}

	transform_.pos = newPos;
	knockBackPow_ = AsoUtility::VECTOR_ZERO;

	LookAtPlayerWithPitchLimit(playerPos);

	// 反撃
	StartMeleeAttack();

	transform_.Update();
}

bool MeleeEnemy::ConsumeDodged(void)
{
	const bool result = justDodged_;
	justDodged_ = false;
	return result;
}

// 気を溜めている間・大技の直前だけ、弱い攻撃で怯まない(カカロットのスーパーアーマー)
bool MeleeEnemy::IsSuperArmor(void) const
{
	return aiState_ == AI_STATE::KI_CHARGE ||
		aiState_ == AI_STATE::BIG_CHARGE ||
		aiState_ == AI_STATE::BEAM_CHARGE ||
		aiState_ == AI_STATE::RUSH_READY ||
		aiState_ == AI_STATE::SLAM_RISE ||
		// 必殺技の本番中も、殴られても止まらない
		aiState_ == AI_STATE::BEAM_FIRE ||
		aiState_ == AI_STATE::KI_BARRAGE ||
		aiState_ == AI_STATE::SLAM_DIVE ||
		aiState_ == AI_STATE::RUSH;
}

bool MeleeEnemy::IsInvincible(void) const
{
	return invincibleTimer_ > 0.0f || clashLock_;
}

void MeleeEnemy::Damage(int damage)
{
	// 回避の直後は攻撃を受けない
	if (IsInvincible())
	{
		return;
	}

	// 殴られた回数を数える(反撃のきっかけ)
	if (hitGate_ <= 0.0f)
	{
		hitStreak_++;
		hitGate_ = 0.1f;
		hitStreakTimer_ = kBurstWindow;
	}

	EnemyBase::Damage(damage);
}

//----------------------------------------------------------------------
// 近接攻撃
//----------------------------------------------------------------------

// 近接攻撃を1段目から始める
void MeleeEnemy::StartMeleeAttack(void)
{
	isAttack_ = true;
	attackCombo_ = 1;
	attackTimer_ = 0.0f;
	hasAttackHit_ = false;

	isAttackWait_ = false;
	attackWaitTimer_ = 0.0f;

	animationController_->Play((int)ANIM_TYPE::ATTACK01, false);
}

// 攻撃中の処理
bool MeleeEnemy::UpdateAttack(float deltaTime, const VECTOR& playerPos)
{
	if (!isAttack_)
	{
		return false;
	}

	attackTimer_ += deltaTime;

	// 1~3段目は攻撃中もプレイヤーについていく
	if (attackCombo_ >= 1 && attackCombo_ <= kFollowComboMax)
	{
		FollowPlayerWhileAttacking(playerPos);
	}

	// 当たり判定のタイミングで、プレイヤーに当たったか調べる
	if (IsAttackHitTiming() && !hasAttackHit_)
	{
		CheckMeleeHit();
	}

	// 次の攻撃へ
	if (attackTimer_ >= kComboInterval)
	{
		AdvanceCombo();
	}

	transform_.Update();
	return true;
}

// 近接攻撃がプレイヤーに当たるか調べて、当てる
void MeleeEnemy::CheckMeleeHit(void)
{
	const VECTOR toPlayer = VSub(player_.GetTransform().pos, transform_.pos);

	if (VSize(ToHorizontal(toPlayer)) > kMeleeHitRange ||
		fabsf(toPlayer.y) > kMeleeHitHeight)
	{
		return;
	}

	// 射程内なら、当たった・防がれた・避けられたのどれでも、この段の判定は終わり
	hasAttackHit_ = true;

	const bool isFinisher = (attackCombo_ >= kMaxCombo);

	ApplyHitToPlayer(
		transform_.pos,
		isFinisher ? kFinisherDamage : kComboDamage,
		isFinisher ? 30.0f : 5.0f,		// 吹き飛ばす強さ
		isFinisher ? 80.0f : 20.0f,		// ガードされたときの、ガード耐久値の減り
		isFinisher ? 5.0f : 1.5f);		// ガードされたときの、押し戻す強さ

	// 4連撃の締めで吹き飛ばしたら、追撃に行く
	if (isFinisher)
	{
		QueueChase();
	}
}

// プレイヤーに攻撃を当てる(回避されたら false)
bool MeleeEnemy::ApplyHitToPlayer(const VECTOR& fromPos, int damage, float knockPower,
	float guardDamage, float guardKnock)
{
	lastHitDamaged_ = false;

	// 回避中は当たらない(ステップを始めてすぐなら、ジャスト回避になる)
	if (player_.IsDodging())
	{
		player_.TryJustDodge();
		return false;
	}

	const VECTOR playerPos = player_.GetTransform().pos;

	VECTOR hitDir = VSub(playerPos, fromPos);

	if (VSize(hitDir) > kEpsilon)
	{
		hitDir = VNorm(hitDir);
	}
	else
	{
		hitDir = VGet(0.0f, 0.0f, 1.0f);
	}

	VECTOR effectPos = playerPos;
	effectPos.y += 80.0f;

	// ガード中は、ダメージなしでガード耐久値だけが減る
	if (player_.IsGuard())
	{
		// ガードされ続けているほど、ガード崩しの技を使いたくなる
		guardWatchTimer_ += 0.6f;

		player_.GuardDamage(guardDamage);
		player_.AddKnockBack(hitDir, guardKnock);

		EffekseerEffect::GetInstance()->PlayHitEffect(effectPos, 0.0f);

		return true;
	}

	player_.LookAtDamageEnemy(transform_.pos);
	{
		int scaled = (int)(damage * kEnemyDamageRate);
		if (scaled < 1) { scaled = 1; }

		player_.Damage(scaled);
	}
	player_.AddKnockBack(hitDir, knockPower);

	EffekseerEffect::GetInstance()->PlayHitEffect(effectPos, 0.0f);
	mainCamera.StartShake(0.1f, 4.0f);

	lastHitDamaged_ = true;
	lastHitDir_ = hitDir;

	return true;
}

// 攻撃中、プレイヤーを一定の距離の位置へ近づきながら見る
void MeleeEnemy::FollowPlayerWhileAttacking(const VECTOR& playerPos)
{
	VECTOR horizontalDir =
		HorizontalDirOrForward(VSub(playerPos, transform_.pos), transform_.quaRot);

	VECTOR attackPos =
		VSub(playerPos, VScale(horizontalDir, kAttackDistance));

	VECTOR follow = VSub(attackPos, transform_.pos);

	transform_.pos =
		VAdd(transform_.pos, VScale(follow, kAttackFollowRate));

	LookAtPlayerWithPitchLimit(playerPos);
}

// 次の段の攻撃へ。最後まで終わったら待機に戻る
void MeleeEnemy::AdvanceCombo(void)
{
	attackTimer_ = 0.0f;
	hasAttackHit_ = false;

	attackCombo_++;

	if (attackCombo_ <= kMaxCombo)
	{
		// ATTACK01~04 は連番(2段目なら ATTACK02)
		const int animType = (int)ANIM_TYPE::ATTACK01 + (attackCombo_ - 1);
		animationController_->Play(animType, false);
		return;
	}

	// 攻撃終了
	ResetAttack();

	attackCoolTimer_ = ATTACK_COOL_TIME;

	ReturnToWait();
}

// プレイヤーを向く(上下の角度は制限する)
void MeleeEnemy::LookAtPlayerWithPitchLimit(const VECTOR& playerPos)
{
	VECTOR lookDir = VSub(playerPos, transform_.pos);

	float lookDistance = VSize(ToHorizontal(lookDir));

	if (lookDistance > kEpsilon)
	{
		float maxY = lookDistance * kLookPitchRate;

		if (lookDir.y > maxY) { lookDir.y = maxY; }
		if (lookDir.y < -maxY) { lookDir.y = -maxY; }
	}

	if (VSize(lookDir) > kEpsilon)
	{
		transform_.quaRot = Quaternion::LookRotation(VNorm(lookDir));
	}
}

VECTOR MeleeEnemy::GetChestPos(void) const
{
	return VAdd(transform_.pos, VGet(0.0f, kChestHeight, 0.0f));
}

//----------------------------------------------------------------------
// 気弾
//----------------------------------------------------------------------

// 気を溜めている間の処理。溜め終わったら気弾を撃つ
bool MeleeEnemy::UpdateKiCharge(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::KI_CHARGE)
	{
		return false;
	}

	animationController_->Play((int)ANIM_TYPE::IDLE);

	LookAtPlayerWithPitchLimit(playerPos);

	aiTimer_ += deltaTime;

	if (aiTimer_ >= kKiChargeTime)
	{
		FireKiBlast();

		kiCoolTimer_ = kKiCoolTime;

		// 撃ったあとに少し隙を作る
		SetAIState(AI_STATE::WAIT);
		aiTimer_ = -kRecoverTime;
	}

	transform_.Update();
	return true;
}

// プレイヤーへ向けて気弾を撃つ
void MeleeEnemy::FireKiBlast(void)
{
	const VECTOR forward = transform_.quaRot.GetForward();
	const VECTOR origin = VAdd(GetChestPos(), VScale(forward, 50.0f));

	const VECTOR target =
		VAdd(player_.GetTransform().pos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	EnemyKiBlast blast;
	blast.Init(origin, VSub(target, origin), kKiSpeed, kKiRadius, kKiLife);

	blasts_.push_back(blast);
}

// 撃った気弾を動かして、プレイヤーに当たったか調べる
void MeleeEnemy::UpdateBlasts(float deltaTime)
{
	const VECTOR playerCenter =
		VAdd(player_.GetTransform().pos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	for (auto& blast : blasts_)
	{
		blast.Update(deltaTime);

		if (blast.IsDead() || blast.IsFrozen())
		{
			continue;
		}

		const float distance = VSize(VSub(blast.GetPos(), playerCenter));

		if (distance > blast.GetRadius() + kBlastHitMargin)
		{
			continue;
		}

		// 当たった(回避されたときは、そのまま飛び続ける)
		if (ApplyHitToPlayer(blast.GetPos(), blast.GetDamage(), blast.GetKnock(), 25.0f, blast.GetKnock() * 0.25f))
		{
			blast.Kill();
		}
	}

	// 消えた気弾を片付ける
	blasts_.erase(
		std::remove_if(blasts_.begin(), blasts_.end(),
			[](const EnemyKiBlast& b) { return b.IsDead(); }),
		blasts_.end());
}

// ビーム(相殺勝負用)
bool MeleeEnemy::GetBeam(VECTOR& origin, VECTOR& dir, float& radius, int& damage) const
{
	if (aiState_ != AI_STATE::BEAM_FIRE)
	{
		return false;
	}

	origin = beamOrigin_;
	dir = beamDir_;
	radius = kBeamRadius;
	damage = kBeamDamage;
	return true;
}

void MeleeEnemy::SetBeamClash(bool clash)
{
	beamClash_ = clash;
	clashLock_ = clash;
}

void MeleeEnemy::EndBeam(void)
{
	beamClash_ = false;
	clashLock_ = false;
	beamLocked_ = false;
	beamCoolTimer_ = kBeamCoolTime;

	// 撃ったあとと同じ、大きな隙
	SetAIState(AI_STATE::WAIT);
	aiTimer_ = -kBeamRecoverTime;
}

// 大きな気弾(相殺勝負用)
bool MeleeEnemy::GetBigBlast(VECTOR& pos, float& radius, int& damage) const
{
	for (const auto& blast : blasts_)
	{
		if (!blast.IsDead() && blast.IsBig())
		{
			pos = blast.GetPos();
			radius = blast.GetRadius();
			damage = blast.GetDamage();
			return true;
		}
	}

	return false;
}

void MeleeEnemy::SetBigBlastFrozen(bool frozen)
{
	clashLock_ = frozen;

	for (auto& blast : blasts_)
	{
		if (blast.IsBig())
		{
			blast.SetFrozen(frozen);
		}
	}
}

void MeleeEnemy::KillBigBlast(void)
{
	clashLock_ = false;

	for (auto& blast : blasts_)
	{
		if (blast.IsBig())
		{
			blast.SetFrozen(false);
			blast.Kill();
		}
	}
}

void MeleeEnemy::CancelAttacksForHeatUp(void)
{
	// すでに撃ってある気弾と、範囲攻撃を全て消す(停止させる)
	blasts_.clear();
	areaBlasts_.clear();

	// 技の途中だった場合に備えて、状態を待機状態に戻しておく
	EndBeam();
}

//----------------------------------------------------------------------
// 突進
//----------------------------------------------------------------------

// 構え → 突進
bool MeleeEnemy::UpdateRush(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::RUSH_READY && aiState_ != AI_STATE::RUSH)
	{
		return false;
	}

	aiTimer_ += deltaTime;

	// 構え: その場でプレイヤーを見て溜める(この間は避けやすい)
	if (aiState_ == AI_STATE::RUSH_READY)
	{
		animationController_->Play((int)ANIM_TYPE::IDLE);

		LookAtPlayerWithPitchLimit(playerPos);

		if (aiTimer_ >= kRushReadyTime)
		{
			// 突進する向きは、構えが終わった瞬間のプレイヤーの位置で決まる
			const VECTOR target =
				VAdd(playerPos, VGet(0.0f, kPlayerCenterHeight * 0.5f, 0.0f));

			rushDir_ = VSub(target, transform_.pos);
			rushDir_ = (VSize(rushDir_) > kEpsilon) ? VNorm(rushDir_) : VGet(0.0f, 0.0f, 1.0f);

			aiState_ = AI_STATE::RUSH;
			aiTimer_ = 0.0f;

			animationController_->Play((int)ANIM_TYPE::FAST_RUN);
		}

		transform_.Update();
		return true;
	}

	// 突進中
	transform_.pos = VAdd(transform_.pos, VScale(rushDir_, kRushSpeed * deltaTime * 60.0f));

	if (transform_.pos.y < kMinHeight) { transform_.pos.y = kMinHeight; }
	if (transform_.pos.y > kMaxHeight) { transform_.pos.y = kMaxHeight; }

	transform_.quaRot = Quaternion::LookRotation(rushDir_);

	bool finished = (aiTimer_ >= kRushTime);

	// プレイヤーに当たったら終わり(回避されたら、そのまま通り過ぎる)
	if (VSize(VSub(playerPos, transform_.pos)) <= kRushHitRange)
	{
		if (ApplyHitToPlayer(transform_.pos, kRushDamage, 28.0f, 40.0f, 6.0f))
		{
			finished = true;
			QueueChase();
		}
	}

	if (finished)
	{
		rushCoolTimer_ = kRushCoolTime;

		// 突進のあとは大きな隙になる
		SetAIState(AI_STATE::WAIT);
		aiTimer_ = -kRecoverTime;

		animationController_->Play((int)ANIM_TYPE::IDLE);
	}

	transform_.Update();
	return true;
}

//----------------------------------------------------------------------
// 範囲大技
//----------------------------------------------------------------------

// 溜め → 警告の球を出す
bool MeleeEnemy::UpdateBigCharge(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::BIG_CHARGE)
	{
		return false;
	}

	animationController_->Play((int)ANIM_TYPE::IDLE);

	LookAtPlayerWithPitchLimit(playerPos);

	aiTimer_ += deltaTime;

	// 溜めている間は、ゆっくり浮き上がる
	transform_.pos.y += kBigRiseSpeed * deltaTime * 60.0f;

	if (transform_.pos.y > kMaxHeight)
	{
		transform_.pos.y = kMaxHeight;
	}

	if (aiTimer_ >= kBigChargeTime)
	{
		LaunchAreaBlasts(playerPos);

		bigCoolTimer_ = kBigCoolTime;

		// 撃ったあとの隙
		SetAIState(AI_STATE::WAIT);
		aiTimer_ = -kBigRecoverTime;
	}

	transform_.Update();
	return true;
}

// プレイヤーの今いる場所に警告の球を出す。HPが半分以下なら、左右にもずらして出す
void MeleeEnemy::LaunchAreaBlasts(const VECTOR& playerPos)
{
	const bool enraged = (hp_ * 2 <= maxHp_);
	const int count = enraged ? 3 : 1;

	const VECTOR center = VAdd(playerPos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	const VECTOR toPlayer =
		HorizontalDirOrForward(VSub(playerPos, transform_.pos), transform_.quaRot);
	const VECTOR side = VGet(toPlayer.z, 0.0f, -toPlayer.x);

	for (int i = 0; i < count; i++)
	{
		// 0番目はプレイヤーの位置、1・2番目は左右
		float offset = 0.0f;

		if (i == 1) { offset = kBigSpread; }
		if (i == 2) { offset = -kBigSpread; }

		EnemyAreaBlast blast;
		blast.Init(
			VAdd(center, VScale(side, offset)),
			kBigRadius,
			kBigWarnTime + kBigStagger * (float)i,
			kBigDamage);

		areaBlasts_.push_back(blast);
	}
}

// 範囲攻撃を進めて、爆発したときにプレイヤーが範囲内ならダメージを与える
void MeleeEnemy::UpdateAreaBlasts(float deltaTime)
{
	const VECTOR playerCenter =
		VAdd(player_.GetTransform().pos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	for (auto& blast : areaBlasts_)
	{
		blast.Update(deltaTime);

		if (!blast.ConsumeExplode())
		{
			continue;
		}

		mainCamera.StartShake(0.2f, 6.0f);

		const float distance = VSize(VSub(blast.GetPos(), playerCenter));

		if (blast.GetDamage() > 0 && distance <= blast.GetRadius())
		{
			ApplyHitToPlayer(blast.GetPos(), blast.GetDamage(), 35.0f, 80.0f, 8.0f);
		}
	}

	areaBlasts_.erase(
		std::remove_if(areaBlasts_.begin(), areaBlasts_.end(),
			[](const EnemyAreaBlast& b) { return b.IsDead(); }),
		areaBlasts_.end());
}

//----------------------------------------------------------------------
// ガード崩し(投げ)
//----------------------------------------------------------------------

// プレイヤーがガードしている時間を数える(ガードをやめると、ゆっくり減る)
void MeleeEnemy::UpdateGuardWatch(float deltaTime, float distance)
{
	if (player_.IsGuard() && distance < kGuardWatchRange)
	{
		guardWatchTimer_ += deltaTime;
		return;
	}

	CountDown(guardWatchTimer_, deltaTime * 0.5f);
}

// 構え → 接近 → 掴む。ガードしていても防げないが、回避なら避けられる
bool MeleeEnemy::UpdateGrab(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::GRAB_READY)
	{
		return false;
	}

	aiTimer_ += deltaTime;

	LookAtPlayerWithPitchLimit(playerPos);

	// 構えの間に、プレイヤーへ素早く近づく
	const VECTOR toPlayer = VSub(playerPos, transform_.pos);
	const float distance = VSize(toPlayer);

	if (distance > kGrabHitRange * 0.6f)
	{
		float step = kGrabApproachSpeed * deltaTime * 60.0f;

		if (step > distance)
		{
			step = distance;
		}

		transform_.pos = VAdd(transform_.pos, VScale(VNorm(toPlayer), step));

		animationController_->Play((int)ANIM_TYPE::FAST_RUN);
	}
	else
	{
		animationController_->Play((int)ANIM_TYPE::IDLE);
	}

	if (aiTimer_ >= kGrabReadyTime)
	{
		DoGrab();
	}

	transform_.Update();
	return true;
}

// 掴む(ガードを崩してダメージ。回避中・遠すぎるときは空振り)
void MeleeEnemy::DoGrab(void)
{
	grabCoolTimer_ = kGrabCoolTime;
	guardWatchTimer_ = 0.0f;

	const VECTOR playerPos = player_.GetTransform().pos;
	const VECTOR toPlayer = VSub(playerPos, transform_.pos);

	bool hit = false;

	if (VSize(toPlayer) <= kGrabHitRange && !player_.IsDodging())
	{
		hit = true;

		VECTOR dir = (VSize(toPlayer) > kEpsilon) ? VNorm(toPlayer) : VGet(0.0f, 0.0f, 1.0f);

		// ガード中なら、ガードを一撃で崩す
		if (player_.IsGuard())
		{
			player_.GuardDamage(9999.0f);
		}

		player_.LookAtDamageEnemy(transform_.pos);
		player_.Damage(kGrabDamage);
		player_.AddKnockBack(dir, kGrabKnock);

		EffekseerEffect::GetInstance()->PlayHitEffect(
			VAdd(playerPos, VGet(0.0f, 80.0f, 0.0f)), 0.0f);
		mainCamera.StartShake(0.2f, 8.0f);
	}

	// 当たっても外しても隙ができる(外した方が大きい)
	SetAIState(AI_STATE::WAIT);
	aiTimer_ = hit ? -kGrabRecover : -kGrabMissRecover;

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

//----------------------------------------------------------------------
// 追撃
//----------------------------------------------------------------------

// 直前の攻撃でダメージが入っていたら、追撃の予定を入れる
void MeleeEnemy::QueueChase(void)
{
	if (!lastHitDamaged_ || chaseCoolTimer_ > 0.0f)
	{
		return;
	}

	if (GetRand(99) >= kChaseChance)
	{
		return;
	}

	pendingChase_ = true;
	pendingChaseTimer_ = kChasePendingLimit;
}

// 姿を消す(吹き飛ぶ向きの先へ回り込むために)
void MeleeEnemy::StartChase(void)
{
	pendingChase_ = false;
	chaseCoolTimer_ = kChaseCoolTime;

	chaseDir_ = HorizontalDirOrForward(lastHitDir_, transform_.quaRot);
	vanishFrom_ = GetChestPos();

	ResetAttack();
	CancelBoostChase();

	isHidden_ = true;
	invincibleTimer_ = kChaseVanishTime + 0.1f;

	SetAIState(AI_STATE::CHASE_VANISH);
}

// 消える → 先回りして現れる → 叩き落とす
bool MeleeEnemy::UpdateChase(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::CHASE_VANISH && aiState_ != AI_STATE::CHASE_APPEAR)
	{
		return false;
	}

	aiTimer_ += deltaTime;

	if (aiState_ == AI_STATE::CHASE_VANISH)
	{
		if (aiTimer_ >= kChaseVanishTime)
		{
			// 吹き飛ばされているプレイヤーの、進む先へ現れる
			VECTOR newPos = VAdd(playerPos, VScale(chaseDir_, kChaseAppearDistance));
			newPos.y = playerPos.y + kChaseAppearHeight;

			if (newPos.y < kMinHeight) { newPos.y = kMinHeight; }
			if (newPos.y > kMaxHeight) { newPos.y = kMaxHeight; }

			transform_.pos = newPos;
			isHidden_ = false;

			SetAIState(AI_STATE::CHASE_APPEAR);

			LookAtPlayerWithPitchLimit(playerPos);
			animationController_->Play((int)ANIM_TYPE::ATTACK03, false);
		}
	}
	else
	{
		// 現れてから叩くまでの間は、ガードや回避ができる
		LookAtPlayerWithPitchLimit(playerPos);

		if (aiTimer_ >= kChaseWindupTime)
		{
			DoSmash();
		}
	}

	transform_.Update();
	return true;
}

// 上から叩き落とす
void MeleeEnemy::DoSmash(void)
{
	const VECTOR playerPos = player_.GetTransform().pos;

	if (VSize(VSub(playerPos, transform_.pos)) <= kSmashRange)
	{
		// 上から叩くので、プレイヤーは下へ飛ぶ
		ApplyHitToPlayer(
			VAdd(playerPos, VGet(0.0f, kSmashFromHeight, 0.0f)),
			kSmashDamage, 38.0f, 60.0f, 6.0f);
	}

	attackCoolTimer_ = ATTACK_COOL_TIME * 0.6f;

	SetAIState(AI_STATE::WAIT);
	aiTimer_ = -kChaseRecoverTime;

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

//----------------------------------------------------------------------
// 極太ビーム
//----------------------------------------------------------------------

// ビームの出る位置(胸の前)
VECTOR MeleeEnemy::GetBeamOrigin(void) const
{
	return VAdd(GetChestPos(), VScale(transform_.quaRot.GetForward(), kBeamBallOffset));
}

// 溜め(途中で狙いを固定) → 発射
bool MeleeEnemy::UpdateBeam(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::BEAM_CHARGE && aiState_ != AI_STATE::BEAM_FIRE)
	{
		return false;
	}

	animationController_->Play((int)ANIM_TYPE::IDLE);

	// 相殺勝負中は、ビームも時間も止める
	if (beamClash_ && aiState_ == AI_STATE::BEAM_FIRE)
	{
		transform_.Update();
		return true;
	}

	aiTimer_ += deltaTime;

	const VECTOR playerCenter = VAdd(playerPos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	if (aiState_ == AI_STATE::BEAM_CHARGE)
	{
		if (!beamLocked_)
		{
			// 狙いを固定するまでは、プレイヤーを追って向く
			LookAtPlayerWithPitchLimit(playerPos);

			if (aiTimer_ >= kBeamLockTime)
			{
				beamOrigin_ = GetBeamOrigin();

				VECTOR dir = VSub(playerCenter, beamOrigin_);
				beamDir_ = (VSize(dir) > kEpsilon) ? VNorm(dir) : VGet(0.0f, 0.0f, 1.0f);

				transform_.quaRot = Quaternion::LookRotation(beamDir_);
				beamLocked_ = true;
			}
		}

		if (aiTimer_ >= kBeamChargeTime)
		{
			// 発射
			if (!beamLocked_)
			{
				beamOrigin_ = GetBeamOrigin();
				beamDir_ = transform_.quaRot.GetForward();
			}

			SetAIState(AI_STATE::BEAM_FIRE);
			beamHitCool_ = 0.0f;

			mainCamera.StartShake(kBeamFireTime, 4.0f);
		}
	}
	else
	{
		// 発射中は、狙いがプレイヤーをゆっくり追う(その場に居続けると当たる)
		VECTOR desired = VSub(playerCenter, beamOrigin_);

		if (VSize(desired) > kEpsilon)
		{
			desired = VNorm(desired);

			const VECTOR blended =
				VAdd(beamDir_, VScale(VSub(desired, beamDir_), Saturate(kBeamTrackRate * deltaTime)));

			if (VSize(blended) > kEpsilon)
			{
				beamDir_ = VNorm(blended);
			}
		}

		// ビームの帯の中にプレイヤーがいれば当たる(当たり続けると、一定間隔でダメージ)
		CountDown(beamHitCool_, deltaTime);

		if (beamHitCool_ <= 0.0f)
		{
			const VECTOR beamEnd = VAdd(beamOrigin_, VScale(beamDir_, kBeamLength));

			if (DistanceToSegment(playerCenter, beamOrigin_, beamEnd) <= kBeamRadius + kBeamHitMargin)
			{
				if (ApplyHitToPlayer(beamOrigin_, kBeamDamage, kBeamKnock, 120.0f, 6.0f))
				{
					beamHitCool_ = kBeamTickTime;
				}
			}
		}

		if (aiTimer_ >= kBeamFireTime)
		{
			beamCoolTimer_ = kBeamCoolTime;
			beamLocked_ = false;

			// 撃ったあとは大きな隙
			SetAIState(AI_STATE::WAIT);
			aiTimer_ = -kBeamRecoverTime;
		}
	}

	transform_.Update();
	return true;
}

//----------------------------------------------------------------------
// 気弾の連射
//----------------------------------------------------------------------

// 少し溜めてから、時間差で何発も撃つ
bool MeleeEnemy::UpdateKiBarrage(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::KI_BARRAGE)
	{
		return false;
	}

	animationController_->Play((int)ANIM_TYPE::IDLE);

	LookAtPlayerWithPitchLimit(playerPos);

	aiTimer_ += deltaTime;

	// 溜め: 赤いオーラをまとって、上へ昇る
	if (aiTimer_ < kBarrageChargeTime)
	{
		transform_.pos.y += kBarrageRiseSpeed * deltaTime;

		if (transform_.pos.y > kMaxHeight) { transform_.pos.y = kMaxHeight; }

		transform_.Update();
		return true;
	}

	barrageShotTimer_ -= deltaTime;

	if (barrageShotsLeft_ > 0 && barrageShotTimer_ <= 0.0f)
	{
		FireBarrageShot();

		barrageShotsLeft_--;
		barrageShotTimer_ = kBarrageInterval;
	}

	// 連射が終わったら、大きな気弾を溜めて放つ
	if (barrageShotsLeft_ <= 0 && barrageShotTimer_ <= 0.0f)
	{
		if (!barrageBigPhase_)
		{
			barrageBigPhase_ = true;
			ShowSkillName(kNameBarrageBig);
			barrageBigTimer_ = 0.0f;
		}

		barrageBigTimer_ += deltaTime;

		if (barrageBigTimer_ >= kBarrageBigChargeTime)
		{
			FireBarrageBig();

			barrageBigPhase_ = false;
			barrageCoolTimer_ = kBarrageCoolTime;

			SetAIState(AI_STATE::WAIT);
			aiTimer_ = -kBarrageRecoverTime;
		}
	}

	transform_.Update();
	return true;
}

// 1発撃つ。動画のように、敵の周りから上や外へ散らして放ち、あとから曲がって降ってくる
//   プレイヤーを追い続ける弾は一部だけ。ほとんどは「撃った時のプレイヤー周辺」へ降るので、動けば避けられる
void MeleeEnemy::FireBarrageShot(void)
{
	const float os = kBarrageOriginSpread;

	// 弾が出る位置: 敵の周りでばらつかせる
	const VECTOR origin = VAdd(
		GetChestPos(),
		VGet(
			(float)(GetRand((int)(os * 2)) - (int)os),
			(float)GetRand((int)os),
			(float)(GetRand((int)(os * 2)) - (int)os)));

	// 最初に飛ぶ向き: 敵を中心に外側・上向きへ放射状に散らす
	const float yaw = (float)GetRand(359) * DX_PI_F / 180.0f;
	const float up = 0.25f + (float)GetRand(75) / 100.0f;
	const VECTOR launchDir = VGet(sinf(yaw), up, cosf(yaw));

	EnemyKiBlast blast;
	blast.Init(origin, launchDir, kBarrageSpeed, kKiRadius * 1.3f, kKiLife);
	blast.SetDamage(kBarrageDamage);
	blast.SetKnock(0.0f);	// 連射は押し出さない(数が多く、床をすり抜けるほど飛ばされるため)

	if (GetRand(99) < kBarrageDirectChance)
	{
		// プレイヤーを追い続ける弾(少しだけ)
		blast.SetHoming(&player_.GetTransform().pos, kPlayerCenterHeight, kBarrageHomingTurn, kBarrageHomingTime);
		blast.SetHomingDelay(kBarrageHomingDelay);
	}
	else
	{
		// 撃った時のプレイヤー周辺のどこかへ降る弾(追わない)
		const int spread = (int)kBarrageSpread;
		VECTOR goal = VAdd(
			VAdd(player_.GetTransform().pos, VGet(0.0f, kPlayerCenterHeight, 0.0f)),
			VGet(
				(float)(GetRand(spread * 2) - spread),
				(float)(GetRand(spread * 2) - spread) * 0.6f,
				(float)(GetRand(spread * 2) - spread)));

		const float floorY = player_.GetTransform().pos.y + 20.0f;
		if (goal.y < floorY) { goal.y = floorY; }

		blast.SetFixedGoal(goal, kBarrageHomingDelay, kBarrageHomingTurn, kBarrageHomingTime);
	}

	blasts_.push_back(blast);
}

// 大きな気弾を放つ(遅いが大きく、追尾する)
void MeleeEnemy::FireBarrageBig(void)
{
	const VECTOR forward = transform_.quaRot.GetForward();
	const VECTOR origin = VAdd(GetChestPos(), VScale(forward, 80.0f));

	const VECTOR target = VAdd(player_.GetTransform().pos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	EnemyKiBlast blast;
	blast.Init(origin, VSub(target, origin), kBarrageBigSpeed, kBarrageBigRadius, kKiLife + 1.0f);
	blast.SetDamage(kBarrageBigDamage);
	blast.SetKnock(4.0f);
	blast.SetBig(true);
	blast.SetHoming(&player_.GetTransform().pos, kPlayerCenterHeight, kBarrageBigHomingTurn, kBarrageBigHomingTime);

	blasts_.push_back(blast);
}

void MeleeEnemy::UpdateKiMarkers(float deltaTime)
{
	for (auto& marker : kiMarkers_)
	{
		marker.timer -= deltaTime;
	}

	kiMarkers_.erase(
		std::remove_if(kiMarkers_.begin(), kiMarkers_.end(),
			[](const KiMarker& m) { return m.timer <= 0.0f; }),
		kiMarkers_.end());
}

//----------------------------------------------------------------------
// 急降下(上昇 → 狙いを固定 → 急降下 → 衝撃波)
//----------------------------------------------------------------------
bool MeleeEnemy::UpdateSlam(float deltaTime, const VECTOR& playerPos)
{
	if (aiState_ != AI_STATE::SLAM_RISE && aiState_ != AI_STATE::SLAM_DIVE)
	{
		return false;
	}

	aiTimer_ += deltaTime;

	const VECTOR playerCenter = VAdd(playerPos, VGet(0.0f, kPlayerCenterHeight, 0.0f));

	if (aiState_ == AI_STATE::SLAM_RISE)
	{
		animationController_->Play((int)ANIM_TYPE::IDLE);

		if (!slamLocked_)
		{
			// 上昇して溜める
			float targetY = playerPos.y + kSlamHeight;

			if (targetY > kMaxHeight) { targetY = kMaxHeight; }

			if (transform_.pos.y < targetY)
			{
				transform_.pos.y += kSlamRiseSpeed * deltaTime * 60.0f;

				if (transform_.pos.y > targetY) { transform_.pos.y = targetY; }
			}

			LookAtPlayerWithPitchLimit(playerPos);

			if (aiTimer_ >= kSlamChargeTime)
			{
				// 狙いを固定(ここから先は、プレイヤーを追わない)
				slamTarget_ = playerCenter;
				slamLocked_ = true;
			}
		}
		else
		{
			VECTOR dir = VSub(slamTarget_, transform_.pos);

			if (VSize(dir) > kEpsilon)
			{
				transform_.quaRot = Quaternion::LookRotation(VNorm(dir));
			}

			if (aiTimer_ >= kSlamChargeTime + kSlamLockTime)
			{
				slamDir_ = (VSize(dir) > kEpsilon) ? VNorm(dir) : VGet(0.0f, -1.0f, 0.0f);

				SetAIState(AI_STATE::SLAM_DIVE);
				animationController_->Play((int)ANIM_TYPE::FAST_RUN);
			}
		}

		transform_.Update();
		return true;
	}

	// 急降下
	transform_.pos = VAdd(transform_.pos, VScale(slamDir_, kSlamDiveSpeed * deltaTime * 60.0f));

	if (transform_.pos.y < kMinHeight) { transform_.pos.y = kMinHeight; }

	transform_.quaRot = Quaternion::LookRotation(slamDir_);

	// 降下中にプレイヤーへ直接当たったか
	bool directHit = false;

	if (VSize(VSub(playerCenter, transform_.pos)) <= kSlamHitRange)
	{
		directHit = ApplyHitToPlayer(transform_.pos, kSlamDamage, 40.0f, 100.0f, 7.0f);
	}

	// 狙った場所に着いた / 当たった / 時間切れ → 衝撃波
	const VECTOR toTarget = VSub(slamTarget_, transform_.pos);
	const bool arrived =
		VDot(toTarget, slamDir_) <= 0.0f ||
		VSize(toTarget) <= kSlamArrive ||
		transform_.pos.y <= kMinHeight;

	if (directHit || arrived || aiTimer_ >= kSlamDiveMaxTime)
	{
		SlamImpact(directHit);
	}

	transform_.Update();
	return true;
}

// 衝撃波を出して、様子見に戻る(直接当てたときは、衝撃波のダメージは入れない)
void MeleeEnemy::SlamImpact(bool directHit)
{
	EnemyAreaBlast blast;
	blast.Init(transform_.pos, kSlamImpactRadius, 0.01f, directHit ? 0 : kSlamImpactDamage);
	areaBlasts_.push_back(blast);

	mainCamera.StartShake(0.3f, 10.0f);

	slamCoolTimer_ = kSlamCoolTime;
	slamLocked_ = false;

	SetAIState(AI_STATE::WAIT);
	aiTimer_ = -kSlamRecoverTime;

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

//----------------------------------------------------------------------
// 高速移動の軌跡
//----------------------------------------------------------------------
void MeleeEnemy::UpdateTrail(float deltaTime)
{
	for (auto& p : trail_)
	{
		p.life -= deltaTime;
	}

	trail_.erase(
		std::remove_if(trail_.begin(), trail_.end(),
			[](const TrailPoint& p) { return p.life <= 0.0f; }),
		trail_.end());

	// 突進・急降下・高速接近のあいだ、軌跡を足していく
	const bool emit =
		!isHidden_ &&
		(aiState_ == AI_STATE::RUSH ||
			aiState_ == AI_STATE::SLAM_DIVE ||
			aiState_ == AI_STATE::BOOST_ATTACK);

	if (!emit)
	{
		return;
	}

	TrailPoint p;
	p.pos = GetChestPos();
	p.life = kTrailLife;
	p.cut = !trail_.empty() && VSize(VSub(p.pos, trail_.back().pos)) > kTrailCut;

	trail_.push_back(p);

	if (trail_.size() > kTrailMax)
	{
		trail_.erase(trail_.begin());
	}
}

// 大技・突進などの最中か
bool MeleeEnemy::IsDoingSpecial(void) const
{
	return aiState_ == AI_STATE::KI_CHARGE ||
		aiState_ == AI_STATE::RUSH_READY ||
		aiState_ == AI_STATE::RUSH ||
		aiState_ == AI_STATE::BIG_CHARGE ||
		aiState_ == AI_STATE::GRAB_READY ||
		aiState_ == AI_STATE::CHASE_VANISH ||
		aiState_ == AI_STATE::CHASE_APPEAR ||
		aiState_ == AI_STATE::BEAM_CHARGE ||
		aiState_ == AI_STATE::BEAM_FIRE ||
		aiState_ == AI_STATE::KI_BARRAGE ||
		aiState_ == AI_STATE::SLAM_RISE ||
		aiState_ == AI_STATE::SLAM_DIVE ||
		aiState_ == AI_STATE::BOOST_ATTACK;
}

// 反撃と気: 毎フレーム呼ぶ(スタン中は反撃も大技もしない)
void MeleeEnemy::UpdateCounterAndKi(float deltaTime, bool isStunned)
{
	CountDown(hitGate_, deltaTime);
	CountDown(burstCool_, deltaTime);
	CountDown(burstFx_, deltaTime);

	if (hitStreakTimer_ > 0.0f)
	{
		hitStreakTimer_ -= deltaTime;

		if (hitStreakTimer_ <= 0.0f)
		{
			hitStreak_ = 0;
		}
	}

	// 気は自動で回復する
	enemyKi_ += kEnemyKiRegen * deltaTime;

	if (enemyKi_ > kEnemyKiMax)
	{
		enemyKi_ = kEnemyKiMax;
	}

	if (isStunned || isDead_ || clashLock_ || isKamehameHit_ || IsDoingSpecial())
	{
		return;
	}

	// 殴られ続けたら、プレイヤーを吹き飛ばす
	if (hitStreak_ >= kBurstHits && burstCool_ <= 0.0f)
	{
		DoCounterBurst();
		return;
	}

	// 気がたまったら、殴られていてもひるまずに大技を出す
	if (enemyKi_ >= kEnemyKiMax)
	{
		ForceSpecial();
	}
}

// 反撃: 衝撃波でプレイヤーを吹き飛ばす(ガードバースト)
void MeleeEnemy::DoCounterBurst(void)
{
	hitStreak_ = 0;
	hitStreakTimer_ = 0.0f;
	burstCool_ = kBurstCoolTime;

	// よろめきをやめて、こちらから動く
	isDamage_ = false;
	damageTimer_ = 0.0f;
	knockBackPow_ = AsoUtility::VECTOR_ZERO;
	ResetAttack();
	CancelBoostChase();
	ReturnToWait();
	if (attackCoolTimer_ < 1.5f)
	{
		attackCoolTimer_ = 1.5f;
	}

	LookAtPlayer();

	const VECTOR toPlayer = VSub(player_.GetTransform().pos, transform_.pos);

	if (VSize(toPlayer) > kBurstRange)
	{
		return;
	}

	burstFx_ = kBurstFxTime;

	ApplyHitToPlayer(transform_.pos, kBurstDamage, kBurstKnock, 60.0f, 8.0f);

	// 吹き飛ばされたあと、少しの間動けない
	if (lastHitDamaged_)
	{
		player_.ExtendDamageStun(kBurstStun);
	}

	EffekseerEffect::GetInstance()->PlayHitEffect(VAdd(transform_.pos, VGet(0.0f, 90.0f, 0.0f)), 0.0f);

	// 離れたところで、すぐに大技の溜めに入る
	ForceSpecial();
}

// 気がたまった: 大技を出す(使える中からランダム。出せるものが無ければ何もしない)
bool MeleeEnemy::ForceSpecial(void)
{
	const bool enraged = (hp_ * 2 <= maxHp_);

	int options[3] = {};
	int count = 0;

	if (bigCoolTimer_ <= 0.0f) { options[count++] = 0; }
	if (beamCoolTimer_ <= 0.0f) { options[count++] = 1; }
	if (barrageCoolTimer_ <= 0.0f) { options[count++] = 2; }

	if (count == 0)
	{
		return false;
	}

	const int pick = options[GetRand(count - 1)];

	// 今の行動(よろめき・ガード・近接攻撃)をやめる
	isDamage_ = false;
	damageTimer_ = 0.0f;
	isGuard_ = false;
	guardTimer_ = 0.0f;
	knockBackPow_ = AsoUtility::VECTOR_ZERO;
	ResetAttack();
	CancelBoostChase();

	switch (pick)
	{
	case 0:
		SetAIState(AI_STATE::BIG_CHARGE);
		break;

	case 1:
		SetAIState(AI_STATE::BEAM_CHARGE);
		beamLocked_ = false;
		break;

	default:
		SetAIState(AI_STATE::KI_BARRAGE);
		barrageShotsLeft_ = enraged ? kBarrageCountEnraged : kBarrageCount;
		barrageShotTimer_ = 0.0f;
		barrageBigPhase_ = false;
		barrageBigTimer_ = 0.0f;
		break;
	}

	return true;
}

// 気弾・突進の途中なら、やめて様子見に戻る(殴られたときなど)
void MeleeEnemy::CancelSpecials(void)
{
	if (aiState_ == AI_STATE::KI_CHARGE ||
		aiState_ == AI_STATE::RUSH_READY ||
		aiState_ == AI_STATE::RUSH ||
		aiState_ == AI_STATE::BIG_CHARGE ||
		aiState_ == AI_STATE::GRAB_READY ||
		aiState_ == AI_STATE::CHASE_VANISH ||
		aiState_ == AI_STATE::CHASE_APPEAR ||
		aiState_ == AI_STATE::BEAM_CHARGE ||
		aiState_ == AI_STATE::BEAM_FIRE ||
		aiState_ == AI_STATE::KI_BARRAGE ||
		aiState_ == AI_STATE::SLAM_RISE ||
		aiState_ == AI_STATE::SLAM_DIVE)
	{
		SetAIState(AI_STATE::WAIT);
	}

	beamLocked_ = false;
	slamLocked_ = false;

	// 追撃の途中で殴られたときなどは、姿を戻して予定も消す
	isHidden_ = false;
	pendingChase_ = false;
}

// 状態の補助
void MeleeEnemy::SetAIState(AI_STATE state)
{
	aiState_ = state;
	aiTimer_ = 0.0f;

	// ビーム発射以外になったら、相殺勝負は終わり
	if (state != AI_STATE::BEAM_FIRE)
	{
		if (beamClash_) { clashLock_ = false; }
		beamClash_ = false;
	}

	// 大技は、名前を画面に出す
	// 大技を出すと、気を使い切る
	if (state == AI_STATE::BEAM_CHARGE || state == AI_STATE::KI_BARRAGE ||
		state == AI_STATE::BIG_CHARGE || state == AI_STATE::SLAM_RISE)
	{
		enemyKi_ = 0.0f;
	}

	switch (state)
	{
	case AI_STATE::BEAM_CHARGE:	ShowSkillName(kNameBeam);		break;
	case AI_STATE::KI_BARRAGE:	ShowSkillName(kNameBarrage);	break;
	case AI_STATE::BIG_CHARGE:	ShowSkillName(kNameBigBlast);	break;
	case AI_STATE::SLAM_RISE:	ShowSkillName(kNameSlam);		break;
	default: break;
	}
}

// 待機状態に戻して、IDLEアニメーションにする
void MeleeEnemy::ReturnToWait(void)
{
	SetAIState(AI_STATE::WAIT);

	animationController_->Play((int)ANIM_TYPE::IDLE);
}

// 攻撃状態をリセット
void MeleeEnemy::ResetAttack(void)
{
	isAttack_ = false;
	attackCombo_ = 0;
	attackTimer_ = 0.0f;
	hasAttackHit_ = false;
}

void MeleeEnemy::CancelBoostChase(void)
{
	isBoostChase_ = false;
	boostChaseTimer_ = 0.0f;
}

//----------------------------------------------------------------------
// AI
//----------------------------------------------------------------------
void MeleeEnemy::UpdateAI(const AIContext& ctx)
{
	switch (aiState_)
	{
	case AI_STATE::WAIT:
		UpdateAIWait(ctx);
		break;

	case AI_STATE::SIDE_MOVE:
		UpdateAISideMove(ctx);
		break;

	case AI_STATE::MOVE:
		UpdateAIMove(ctx);
		break;

	case AI_STATE::BOOST_ATTACK:
		UpdateAIBoostAttack(ctx);
		break;

	case AI_STATE::ATTACK:
		UpdateAIAttack(ctx);
		break;

	default:
		// KI_CHARGE / RUSH_READY / RUSH は、Update の前半で処理している
		break;
	}
}

// 様子を見る。時間が経ったら、次の行動を選ぶ
void MeleeEnemy::UpdateAIWait(const AIContext& ctx)
{
	aiTimer_ += ctx.deltaTime;

	animationController_->Play((int)ANIM_TYPE::IDLE);

	// 隙の間(aiTimer_ がマイナス)は動かない。それ以外は、プレイヤーの周りをゆっくり漂う
	if (aiTimer_ >= 0.0f)
	{
		float radius = ctx.distance;

		if (radius < kWaitMinRadius) { radius = kWaitMinRadius; }
		if (radius > kWaitMaxRadius) { radius = kWaitMaxRadius; }

		OrbitPlayer(ctx, kWaitDrift, radius,
			kHoverBobAmp * sinf(hoverTime_ * kHoverBobSpeed));
	}

	// 吹き飛ばした直後なら、追撃に入る
	if (pendingChase_ && aiTimer_ >= kChaseStartDelay)
	{
		StartChase();
		return;
	}

	if (aiTimer_ < kWaitTime)
	{
		return;
	}

	DecideAction(ctx);
}

// 距離とHPから、次の行動を選ぶ
//   近い: 近接攻撃が中心 / 中くらい: 気弾・突進 / 遠い: 高速接近・気弾
//   HPが半分以下になると、気弾と突進を多く使う
void MeleeEnemy::DecideAction(const AIContext& ctx)
{
	const bool enraged = (hp_ * 2 <= maxHp_);

	const bool canAttack = (attackCoolTimer_ <= 0.0f);
	const bool canKi = (kiCoolTimer_ <= 0.0f);
	const bool canRush = (rushCoolTimer_ <= 0.0f);
	const bool canBig = (bigCoolTimer_ <= 0.0f);
	const bool canBeam = (beamCoolTimer_ <= 0.0f);
	const bool canBarrage = (barrageCoolTimer_ <= 0.0f);
	const bool canSlam = (slamCoolTimer_ <= 0.0f);

	// ガードされ続けているなら、ガード不能の投げを選ぶ
	const bool wantGrab =
		guardWatchTimer_ >= kGuardWatchTrigger &&
		grabCoolTimer_ <= 0.0f &&
		ctx.distance < kGuardWatchRange + 100.0f;

	int weights[ACT_MAX] = {};

	if (ctx.distance < kNearDistance)
	{
		// 近いからといって殴り続けない: 下がって気弾を撃ったり、動き回って間合いを取り直す
		weights[ACT_ATTACK] = canAttack ? 2 : 0;
		weights[ACT_SIDE] = 3;
		weights[ACT_KI] = canKi ? 2 : 0;
		// 近距離でも、赤く光って溜めてからの強攻撃(突進)を出す
		weights[ACT_RUSH] = canRush ? (enraged ? 5 : 4) : 0;
		weights[ACT_BIG] = canBig ? (enraged ? 2 : 1) : 0;
		weights[ACT_SLAM] = canSlam ? 1 : 0;
	}
	else if (ctx.distance < kFarDistance)
	{
		weights[ACT_KI] = canKi ? (enraged ? 5 : 3) : 0;
		weights[ACT_RUSH] = canRush ? (enraged ? 5 : 3) : 0;
		weights[ACT_SIDE] = 2;
		weights[ACT_BOOST] = 3;
		weights[ACT_BIG] = canBig ? (enraged ? 4 : 2) : 0;
		weights[ACT_BEAM] = canBeam ? (enraged ? 3 : 2) : 0;
		weights[ACT_BARRAGE] = canBarrage ? (enraged ? 4 : 3) : 0;
		weights[ACT_SLAM] = canSlam ? (enraged ? 3 : 2) : 0;
	}
	else
	{
		// 遠いときは詰めてくるのを優先(遠距離攻撃だけで居座らせない)
		weights[ACT_BOOST] = 12;
		weights[ACT_KI] = canKi ? (enraged ? 3 : 2) : 0;
		weights[ACT_BIG] = canBig ? (enraged ? 2 : 1) : 0;
		weights[ACT_BEAM] = canBeam ? (enraged ? 2 : 1) : 0;
		weights[ACT_BARRAGE] = canBarrage ? (enraged ? 3 : 2) : 0;
	}

	if (wantGrab)
	{
		weights[ACT_GRAB] = 15;
	}

	int pick = PickWeighted(weights, ACT_MAX);

	// 選べるものが無ければ、横に動いて時間を稼ぐ
	if (pick < 0)
	{
		pick = ACT_SIDE;
	}

	switch (pick)
	{
	case ACT_ATTACK:
		// 間合いを取り直さず、そのまま殴りに行く
		SetAIState(AI_STATE::ATTACK);
		break;

	case ACT_SIDE:
		SetAIState(AI_STATE::SIDE_MOVE);

		// 回る向き・距離・高さを決める(同じ向きに続けることもある)
		if (GetRand(99) < 70)
		{
			sideMoveDir_ *= -1.0f;
		}

		strafeRadius_ = kOrbitMinRadius + (float)GetRand(kOrbitRadiusRange);
		strafeHeight_ = kOrbitHeightMin + (float)GetRand(kOrbitHeightRange);
		break;

	case ACT_KI:
		SetAIState(AI_STATE::KI_CHARGE);
		break;

	case ACT_RUSH:
		SetAIState(AI_STATE::RUSH_READY);
		break;

	case ACT_BOOST:
		SetAIState(AI_STATE::BOOST_ATTACK);
		break;

	case ACT_GRAB:
		SetAIState(AI_STATE::GRAB_READY);
		break;

	case ACT_BIG:
		SetAIState(AI_STATE::BIG_CHARGE);
		break;

	case ACT_BEAM:
		SetAIState(AI_STATE::BEAM_CHARGE);
		beamLocked_ = false;
		break;

	case ACT_BARRAGE:
		SetAIState(AI_STATE::KI_BARRAGE);
		barrageShotsLeft_ = enraged ? kBarrageCountEnraged : kBarrageCount;
		barrageShotTimer_ = 0.0f;
		barrageBigPhase_ = false;
		barrageBigTimer_ = 0.0f;
		break;

	case ACT_SLAM:
		SetAIState(AI_STATE::SLAM_RISE);
		slamLocked_ = false;
		break;
	}
}

// 空中の旋回(プレイヤーの周りを、距離と高さを変えながら回る)
void MeleeEnemy::UpdateAISideMove(const AIContext& ctx)
{
	aiTimer_ += ctx.deltaTime;

	OrbitPlayer(ctx, 1.0f, strafeRadius_, strafeHeight_);

	animationController_->Play((int)ANIM_TYPE::RUN);

	// 旋回が終わったら、また次の行動を選ぶ
	if (aiTimer_ >= kSideMoveTime)
	{
		SetAIState(AI_STATE::WAIT);
	}
}

// プレイヤーを中心に、角度を進めた位置へ向かって移動する
void MeleeEnemy::OrbitPlayer(const AIContext& ctx, float speedScale, float radius, float heightOffset)
{
	const VECTOR rel = ToHorizontal(VSub(transform_.pos, ctx.playerPos));
	const float currentRadius = VSize(rel);

	float angle = 0.0f;

	if (currentRadius > kEpsilon)
	{
		angle = atan2f(rel.z, rel.x);
	}

	// 角度を進める
	angle += sideMoveDir_ * kOrbitAngularSpeed * speedScale * ctx.deltaTime;

	// 半径と高さは、目標へ少しずつ寄せる
	const float frameRate = ctx.deltaTime * 60.0f;
	const float r = currentRadius + (radius - currentRadius) * Saturate(kOrbitRadiusLerp * frameRate);

	const float targetY = ctx.playerPos.y + heightOffset;
	const float y = transform_.pos.y + (targetY - transform_.pos.y) * Saturate(kOrbitHeightLerp * frameRate);

	const VECTOR desired = VGet(
		ctx.playerPos.x + cosf(angle) * r,
		y,
		ctx.playerPos.z + sinf(angle) * r);

	// 速すぎないように、1フレームの移動量を制限する
	VECTOR move = VSub(desired, transform_.pos);
	const float maxStep = kOrbitMaxSpeed * frameRate;

	if (VSize(move) > maxStep)
	{
		move = VScale(VNorm(move), maxStep);
	}

	transform_.pos = VAdd(transform_.pos, move);

	if (transform_.pos.y < kMinHeight) { transform_.pos.y = kMinHeight; }
	if (transform_.pos.y > kMaxHeight) { transform_.pos.y = kMaxHeight; }
}

// 攻撃前に、普段戦う距離へ間合いを整える
void MeleeEnemy::UpdateAIMove(const AIContext& ctx)
{
	VECTOR normalBattlePos =
		VSub(ctx.playerPos, VScale(ctx.horizontalDir, kNormalBattleDistance));

	VECTOR moveDir = VSub(normalBattlePos, transform_.pos);
	float moveDistance = VSize(moveDir);

	if (moveDistance > kEpsilon)
	{
		moveDir = VNorm(moveDir);
	}

	if (moveDistance > kArriveDistance)
	{
		float moveAmount = moveDistance;

		if (moveAmount > moveSpeed_)
		{
			moveAmount = moveSpeed_;
		}

		transform_.pos = VAdd(transform_.pos, VScale(moveDir, moveAmount));

		animationController_->Play((int)ANIM_TYPE::RUN);
		return;
	}

	// 間合いが整ったので、少し待ってから攻撃か待機へ
	aiTimer_ += ctx.deltaTime;

	animationController_->Play((int)ANIM_TYPE::IDLE);

	if (aiTimer_ >= kMoveWaitTime)
	{
		aiTimer_ = 0.0f;

		aiState_ = (attackCoolTimer_ <= 0.0f) ? AI_STATE::ATTACK : AI_STATE::WAIT;
	}
}

// 高速接近(次の行動を選んだ時だけ入る)
void MeleeEnemy::UpdateAIBoostAttack(const AIContext& ctx)
{
	if (!isBoostChase_)
	{
		isBoostChase_ = true;
		boostChaseTimer_ = 0.0f;

		animationController_->Play((int)ANIM_TYPE::FAST_RUN);
	}

	boostChaseTimer_ += ctx.deltaTime;

	VECTOR boostTarget =
		VSub(ctx.playerPos, VScale(ctx.horizontalDir, kBoostStopDistance));

	VECTOR boostDir = VSub(boostTarget, transform_.pos);
	float boostDistance = VSize(boostDir);

	if (boostDistance > kEpsilon)
	{
		boostDir = VNorm(boostDir);
	}

	// 近づいたか、時間切れなら攻撃へ
	if (boostDistance <= kArriveDistance ||
		boostChaseTimer_ >= BOOST_CHASE_TIME)
	{
		CancelBoostChase();

		aiState_ = AI_STATE::ATTACK;
		aiTimer_ = 0.0f;

		animationController_->Play((int)ANIM_TYPE::IDLE);
		return;
	}

	float boostMove = boostDistance;
	const float boostMax = BOOST_CHASE_SPEED * ctx.deltaTime * 60.0f;

	if (boostMove > boostMax)
	{
		boostMove = boostMax;
	}

	transform_.pos = VAdd(transform_.pos, VScale(boostDir, boostMove));

	animationController_->Play((int)ANIM_TYPE::FAST_RUN);
}

// 攻撃距離まで近づいて、4連撃を開始する
void MeleeEnemy::UpdateAIAttack(const AIContext& ctx)
{
	aiTimer_ += ctx.deltaTime;

	// 追っても追いつけないときは諦めて、別の行動(気弾・突進・旋回など)へ移る
	if (aiTimer_ >= kAttackGiveUpTime)
	{
		attackCoolTimer_ = kAttackGiveUpCool;
		ReturnToWait();
		return;
	}

	VECTOR attackPos =
		VSub(ctx.playerPos, VScale(ctx.horizontalDir, kAttackDistance));

	VECTOR attackMoveDir = VSub(attackPos, transform_.pos);
	float attackMoveDistance = VSize(attackMoveDir);

	if (attackMoveDistance > kEpsilon)
	{
		attackMoveDir = VNorm(attackMoveDir);
	}

	if (attackMoveDistance > kAttackStartSlack)
	{
		float moveAmount = attackMoveDistance;
		const float approachMax = kAttackApproachSpeed * ctx.deltaTime * 60.0f;

		if (moveAmount > approachMax)
		{
			moveAmount = approachMax;
		}

		transform_.pos = VAdd(transform_.pos, VScale(attackMoveDir, moveAmount));

		animationController_->Play((int)ANIM_TYPE::RUN);
		return;
	}

	// クールタイム中なら待機へ戻る
	if (attackCoolTimer_ > 0.0f)
	{
		SetAIState(AI_STATE::WAIT);

		animationController_->Play((int)ANIM_TYPE::IDLE);
		return;
	}

	// 攻撃距離まで来たので攻撃開始
	StartMeleeAttack();
}

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

	// スーパーアーマー中(大技の溜めなど)は赤いオーラをまとう。この間は殴られても怯まない
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

	// 気弾の着弾点の輪・高速移動の軌跡
	DrawKiMarkers();
	DrawTrail();

	// 気弾・突進の予告(プレイヤーが避ける目印)
	DrawTelegraph();

	// HPバー・スタンゲージ(2D)は DrawOverlay で、最後にまとめて描く
}

// 気を溜めている光・突進の構えの光
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

	// 範囲大技の溜め: 頭の上に大きな球が育つ
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

	// 追撃: 消えた場所に残像の光 / 現れたら黄色く光って叩く構え
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

	// ビームの溜め: 大きな球が育つ。狙いを固定したら、赤い帯(これから撃つ線)が出る
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

			// 当たる範囲(薄い帯)と、中心の線
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

	// 連射のあとの大きな気弾の溜め: 胸の前に大きな球が育つ
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

		// 赤いオーラの柱(カカロットの溜め)
		DrawAuraColumn(transform_.pos, rate);

		SetUseLighting(FALSE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 190);
		DrawSphere3D(pos, 8.0f + 26.0f * rate, 16, GetColor(150, 40, 255), GetColor(150, 40, 255), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawSphere3D(pos, 4.0f + 10.0f * rate, 16, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);

		SetUseLighting(TRUE);
	}

	// 急降下: 上昇中はピンクのオーラ、狙いを固定したら降りる先に赤い輪と線
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
			// 降りる先(衝撃波の範囲)
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

	// ビーム発射: 紫の太い光線(外側→中→白い芯)
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

// 気弾の着弾点: 縮んでいく赤い輪(輪が消える頃に気弾が着く)
void MeleeEnemy::DrawKiMarkers(void) const
{
	if (kiMarkers_.empty())
	{
		return;
	}

	SetUseLighting(FALSE);

	for (const auto& marker : kiMarkers_)
	{
		const float rate = Saturate(marker.timer / marker.total);
		const float radius = kMarkerRadius * (0.25f + 0.75f * rate);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 70 + (int)(60.0f * (1.0f - rate)));
		DrawSphere3D(marker.pos, radius, 16, GetColor(255, 0, 0), GetColor(255, 0, 0), TRUE);

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
		DrawRing3D(marker.pos, radius, GetColor(255, 30, 0));
		DrawRing3D(marker.pos, radius * 0.6f, GetColor(255, 220, 0));

		SetDrawBlendMode(DX_BLENDMODE_ALPHA, 200);
		DrawSphere3D(marker.pos, 10.0f, 8, GetColor(255, 220, 0), GetColor(255, 220, 0), TRUE);
	}

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	SetUseLighting(TRUE);
}

// 高速移動の軌跡: ピンクの帯が、だんだん細く薄くなる
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

// 大技の溜め: 足元から空へのびるピンクの光の柱(rate: 0.0~1.0 で太く強くなる)
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

// 足元に、残り時間で縮む円を描く
void MeleeEnemy::DrawSyncRing(void)
{
	if (!IsSyncReady())
	{
		return;
	}

	VECTOR center = transform_.pos;

	// 床より少し上に
	center.y += 3.0f;

	// 1.0 → 0.0 (残り時間で円が縮む)
	float radius = kRingRadius * GetSyncRate();

	// index 番目の円周上の点
	auto ringPoint = [&](int index, float r, float yOffset) -> VECTOR
		{
			float angle =
				DX_TWO_PI_F *
				static_cast<float>(index) /
				static_cast<float>(kRingSegment);

			return VGet(
				center.x + cosf(angle) * r,
				center.y + yOffset,
				center.z + sinf(angle) * r);
		};

	// 半透明の塗りつぶし(中心から三角形を並べて円を作る)
	unsigned int fillColor = GetColor(80, 220, 255);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90);

	for (int i = 0; i < kRingSegment; i++)
	{
		VECTOR p1 = ringPoint(i, radius, 0.0f);
		VECTOR p2 = ringPoint(i + 1, radius, 0.0f);

		DrawTriangle3D(center, p2, p1, fillColor, true);
	}

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 255);

	// 外側の線
	float outlineRadius = radius + 3.0f;
	unsigned int lineColor = GetColor(180, 245, 255);

	for (int i = 0; i < kRingSegment; i++)
	{
		VECTOR p1 = ringPoint(i, outlineRadius, 0.5f);
		VECTOR p2 = ringPoint(i + 1, outlineRadius, 0.5f);

		DrawLine3D(p1, p2, lineColor);
	}
}

// 位置・向き
void MeleeEnemy::SetPosition(VECTOR pos)
{
	transform_.pos = pos;

	transform_.Update();
}

void MeleeEnemy::LookAtPlayer(void)
{
	VECTOR dir = ToHorizontal(VSub(player_.GetTransform().pos, transform_.pos));

	if (VSize(dir) <= kEpsilon)
	{
		return;
	}

	damageRot_ = Quaternion::LookRotation(VNorm(dir));

	transform_.quaRot = damageRot_;
}

// 攻撃
bool MeleeEnemy::IsAttackHitTiming(void) const
{
	return
		isAttack_ &&
		attackTimer_ >= ATTACK_HIT_START &&
		attackTimer_ <= ATTACK_HIT_END;
}

bool MeleeEnemy::HasAttackHit(void) const
{
	return hasAttackHit_;
}

void MeleeEnemy::SetAttackHit(void)
{
	hasAttackHit_ = true;
}

int MeleeEnemy::GetAttackCombo(void) const
{
	return attackCombo_;
}

// ガード
bool MeleeEnemy::IsGuard(void) const
{
	return isGuard_;
}

void MeleeEnemy::GuardDamage(float damage)
{
	if (!isGuard_ || isGuardBreak_)
	{
		return;
	}

	guardHp_ -= damage;

	if (guardHp_ > 0.0f)
	{
		return;
	}

	// ガードブレイク
	guardHp_ = 0.0f;

	isGuard_ = false;
	guardTimer_ = 0.0f;

	isGuardBreak_ = true;
	guardBreakTimer_ = kGuardBreakTime;

	ResetAttack();
	CancelBoostChase();
	CancelSpecials();

	animationController_->Play((int)ANIM_TYPE::GUARD_BREAK, false);

	// ガードを崩されると、スタンゲージも大きく増える
	AddStun(kGuardBreakStun);
}

void MeleeEnemy::GuardBurst(void)
{
	// 攻撃・高速接近・特殊攻撃を中断
	ResetAttack();
	CancelBoostChase();
	CancelSpecials();

	// すぐに再攻撃しないようにする
	attackCoolTimer_ = ATTACK_COOL_TIME;
}

// プレイヤーからの攻撃による状態変化
void MeleeEnemy::StartSlamDown(void)
{
	isSlamDown_ = true;
	isDown_ = false;
	downTimer_ = 0.0f;

	isAttack_ = false;
	attackTimer_ = 0.0f;

	isDamage_ = false;
	damageTimer_ = 0.0f;

	CancelSpecials();
}

bool MeleeEnemy::IsDown(void) const
{
	return isDown_;
}

void MeleeEnemy::SetKamehameHit(bool hit, VECTOR dir)
{
	if (!hit)
	{
		isKamehameHit_ = false;
		kamehamePushDir_ = AsoUtility::VECTOR_ZERO;
		kamehamePushSpeed_ = 0.0f;
		return;
	}

	// 押され始めた瞬間
	if (!isKamehameHit_)
	{
		kamehamePushSpeed_ = kKamehamePushStartSpeed;

		animationController_->Play((int)ANIM_TYPE::DAMAGE, true);
	}

	isKamehameHit_ = true;

	if (VSize(dir) > kEpsilon)
	{
		kamehamePushDir_ = VNorm(dir);
	}

	ResetAttack();
	CancelBoostChase();
	CancelSpecials();
}