#include <DxLib.h>
#include <cmath>
#include <cfloat>
#include <vector>
#include "../Application.h"
#include "../Utility/AsoUtility.h"
#include "../Utility/DamageCalc.h"
#include "../Manager/SceneManager.h"
#include "../Manager/Camera.h"
#include "../Manager/InputManager.h"
#include "../Manager/PadInput.h"
#include "../Manager/EffekseerEffect.h"
#include "../Manager/AppFlow.h"
#include "../Manager/PlayRecord.h"
#include "../Common/MenuUI.h"
#include "../Object/Common/Capsule.h"
#include "../Object/Common/Collider.h"
#include "../Object/SkyDome.h"
#include "../Object/Stage.h"
#include "../Object/Player/Player.h"
#include "../Object/Enemy/EnemyBase.h"
#include "../Object/Enemy/MeleeEnemy.h"
#include "../Object/Enemy/BreakEffect.h"
#include "../Object/Planet.h"
#include "../Renderer/PixelMaterial.h"
#include "../Renderer/PixelRenderer.h"
#include "GameScene.h"
#include "ClashSystem.h"

namespace
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

	VECTOR EnemyCenter(EnemyBase& enemy)
	{
		return VAdd(enemy.GetTransform().pos, VGet(0.0f, ENEMY_CENTER_HEIGHT, 0.0f));
	}

	VECTOR HitEffectPos(EnemyBase& enemy)
	{
		return VAdd(enemy.GetTransform().pos, VGet(0.0f, HIT_EFFECT_HEIGHT, 0.0f));
	}

	VECTOR PopupPos(EnemyBase& enemy)
	{
		return VAdd(enemy.GetTransform().pos, VGet(0.0f, POPUP_HEIGHT, 0.0f));
	}

	// 長さが0に近いときは fallback を返す
	VECTOR NormalizeOr(const VECTOR& v, const VECTOR& fallback)
	{
		return (VSize(v) > 0.001f) ? VNorm(v) : fallback;
	}

	MeleeEnemy* AsMelee(EnemyBase* enemy)
	{
		return dynamic_cast<MeleeEnemy*>(enemy);
	}

	// バニッシュアタックの移動先: 敵の斜め後ろのうち、今のプレイヤーに近いほう
	//   back = 敵の向きの後ろ側(水平)。少し横にずらして、カメラが大きく回らないようにする
	VECTOR CalcVanishPos(const VECTOR& enemyPos, const VECTOR& back, const VECTOR& playerPos)
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

//-----------------------------------------------------------------------------
// 生成 / 破棄 / 初期化
//-----------------------------------------------------------------------------

GameScene::GameScene(void)
	:
	player_(nullptr),
	skyDome_(nullptr),
	stage_(nullptr),
	clash_(nullptr),
	menu_(nullptr),
	mode_(MODE::MAIN),
	postEffectScreen_(-1),
	gameState_(GAME_STATE::PLAY),
	playTime_(0.0f),
	clearDelayTimer_(0.0f),
	clearTimer_(0.0f),
	gameOverTimer_(0.0f),
	isResultRequested_(false),
	confirmAction_(MENU_ACTION::NONE),
	isMenuDecided_(false),
	isHitStop_(false),
	hitStopTimer_(0.0f),
	breakFreeze_(0.0f),
	lockOnTarget_(nullptr),
	isLockOn_(false),
	vanishTarget_(nullptr),
	vanishTimer_(0.0f),
	vanishDir_{ 0.0f, 0.0f, 1.0f },
	pursuitTarget_(nullptr),
	pursuitTimer_(0.0f),
	pursuitDir_{ 0.0f, 0.0f, 1.0f },
	justSlowTimer_(0.0f),
	justSlowFrame_(0),
	heatWasActivating_(false),
	heatShookBurst_(false),
	heatShookClose_(false),
	kamehameDamageTimer_(0.0f)
{
}

GameScene::~GameScene(void)
{
	DeleteGraph(postEffectScreen_);

	// シーンを抜けるときは、エフェクト停止を必ず解除しておく
	AppFlow::SetEffectFreeze(false);
}

void GameScene::Init(void)
{
	// HUD(自分・敵のHPバーなど)は、エフェクトや弾より手前に出す
	SceneManager::GetInstance().SetOverlayDraw([this]()
		{
			for (auto& enemy : enemies_)
			{
				if (enemy && !enemy->IsDead())
				{
					enemy->DrawOverlay();
				}
			}

			if (player_)
			{
				DrawBarrageReticle();
				player_->DrawHud();
			}

			// ブレイク演出(自前の時計で動く)
			BreakEffect::Draw();

			// ポーズ / ゲームオーバーのメニュー(一番手前)
			DrawMenuOverlay();
		});

	ResetState();

	// 前のプレイで残ったブレイク演出を終わらせる(リトライ対策)
	BreakEffect::Update(100.0f);

	clash_ = std::make_unique<ClashSystem>();
	menu_ = std::make_unique<MenuUI>();

	// プレイヤー
	player_ = std::make_unique<Player>();
	player_->Init();

	// ステージ
	stage_ = std::make_unique<Stage>(*player_);
	stage_->Init();
	stage_->ChangeStage(Stage::NAME::MAIN_PLANET);

	// スカイドーム
	skyDome_ = std::make_unique<SkyDome>(player_->GetTransform());
	skyDome_->Init();

	mainCamera.SetFollow(&player_->GetTransform());
	mainCamera.ChangeMode(Camera::MODE::FOLLOW);

	InitPostEffects();

	MakeEnemies();

	SetGlobalAmbientLight(GetColorF(1.0f, 1.0f, 1.0f, 1.0f));
}

// 途中経過を最初の状態に戻す(リトライしても前のプレイが残らないように)
void GameScene::ResetState(void)
{
	gameState_ = GAME_STATE::PLAY;
	playTime_ = 0.0f;
	clearDelayTimer_ = 0.0f;
	clearTimer_ = 0.0f;
	gameOverTimer_ = 0.0f;
	isResultRequested_ = false;

	confirmAction_ = MENU_ACTION::NONE;
	isMenuDecided_ = false;

	isHitStop_ = false;
	hitStopTimer_ = 0.0f;
	breakFreeze_ = 0.0f;

	lockOnTarget_ = nullptr;
	isLockOn_ = false;

	vanishTarget_ = nullptr;
	vanishTimer_ = 0.0f;
	vanishDir_ = VGet(0.0f, 0.0f, 1.0f);

	pursuitTarget_ = nullptr;
	pursuitTimer_ = 0.0f;
	pursuitDir_ = VGet(0.0f, 0.0f, 1.0f);

	justSlowTimer_ = 0.0f;
	justSlowFrame_ = 0;

	prevStun_.clear();

	heatWasActivating_ = false;
	heatShookBurst_ = false;
	heatShookClose_ = false;

	kamehameDamageTimer_ = 0.0f;

	AppFlow::SetEffectFreeze(false);
}

void GameScene::InitPostEffects(void)
{
	const int mainScreen = SceneManager::GetInstance().GetMainScreen();

	// 画面いっぱいの四角を描くレンダラーを作る
	auto makeRenderer = [](PixelMaterial& material)
		{
			auto renderer = std::make_unique<PixelRenderer>(material);
			renderer->MakeSquereVertex(
				Vector2(0, 0),
				Vector2(Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y));
			return renderer;
		};

	// ポストエフェクト用スクリーン
	postEffectScreen_ = MakeScreen(
		Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y, true);

	// 初期モード(ポストエフェクトなし)
	mode_ = MODE::MAIN;
}

void GameScene::MakeEnemies(void)
{
	auto enemy = std::make_unique<MeleeEnemy>(*player_);
	enemy->Init();

	enemies_.push_back(std::move(enemy));
}

//-----------------------------------------------------------------------------
// 更新
//-----------------------------------------------------------------------------

void GameScene::Update(void)
{
	const float dt = SceneManager::GetInstance().GetDeltaTime();

	// ポーズ中・ゲームオーバー中は、ゲーム本体を止める
	if (gameState_ == GAME_STATE::PAUSE)
	{
		UpdatePause();
		return;
	}

	if (gameState_ == GAME_STATE::GAME_OVER)
	{
		UpdateGameOver();
		return;
	}

	if (gameState_ == GAME_STATE::PLAY && CheckStateChange(dt))
	{
		return;
	}

	// ブレイク演出は専用の時計で進める(下の全体停止の影響を受けない)
	BreakEffect::Update(dt);

	// ブレイクした瞬間の全体停止
	if (breakFreeze_ > 0.0f)
	{
		breakFreeze_ -= dt;
		return;
	}

	if (UpdateHitStop(dt))
	{
		return;
	}

	if (gameState_ == GAME_STATE::PLAY)
	{
		UpdatePlay(dt);
	}
	else if (gameState_ == GAME_STATE::CLEAR)
	{
		UpdateClear(dt);
	}
}

bool GameScene::CheckStateChange(float dt)
{
	// HPが0になったらゲームオーバー
	if (player_->IsDead())
	{
		StartGameOver();
		return true;
	}

	// 敵を全部倒したら、少し間をおいてクリア
	if (IsAllEnemiesDead())
	{
		clearDelayTimer_ += dt;

		if (clearDelayTimer_ >= CLEAR_DELAY_TIME)
		{
			StartClear();
			return true;
		}
	}
	else
	{
		clearDelayTimer_ = 0.0f;
	}

	playTime_ += dt;

	// ポーズボタン(ESC / P / パッドのOPTIONS)
	// ヒートアップ発動中はカメラ演出が進むので、ポーズできないようにする
	InputManager& ins = InputManager::GetInstance();

	const bool pauseTrg =
		ins.IsTrgDown(KEY_INPUT_ESCAPE) ||
		ins.IsTrgDown(KEY_INPUT_P) ||
		PadInput::IsTrg(PadInput::BTN::OPTIONS);

	if (pauseTrg && !player_->IsHeatActivating())
	{
		StartPause();
		return true;
	}

	return false;
}

bool GameScene::UpdateHitStop(float dt)
{
	if (!isHitStop_)
	{
		return false;
	}

	hitStopTimer_ -= dt;

	if (hitStopTimer_ <= 0.0f)
	{
		hitStopTimer_ = 0.0f;
		isHitStop_ = false;
		return false;
	}

	return true;
}

void GameScene::UpdatePlay(float dt)
{
	skyDome_->Update();
	stage_->Update();

	UpdateLockOn();

	player_->Update();
	// 連続エネルギー弾の狙い(撃っていないときは 0 に戻る)
	mainCamera.SetAimOffset(player_->GetBarrageAimYaw(), player_->GetBarrageAimPitch());

	// 技
	UpdateVanish(dt);
	UpdatePursuit(dt);
	UpdateClash(dt);
	UpdateGuardBurst();

	// 演出
	UpdateJustDodge();
	UpdateBreakFreeze();
	UpdateHeatUpCamera();

	// 敵と体
	UpdateEnemies(dt);
	PushOutBodies();

	// 当たり判定
	UpdateMeleeHit();
	UpdateKiBlastHit();
	UpdateBarrageHit();
	UpdateFinishBlastHit();
	UpdateKamehameHit(dt);
}

//-----------------------------------------------------------------------------
// ロックオン
//-----------------------------------------------------------------------------

void GameScene::UpdateLockOn(void)
{
	// ロックオンしている敵がいなければ、一番近い敵をロックオンする(常時ロックオン)
	if (lockOnTarget_ == nullptr || lockOnTarget_->IsDead())
	{
		EnemyBase* nearest = FindNearestEnemy(FLT_MAX);

		if (nearest != nullptr)
		{
			StartLockOn(nearest);
		}
	}

	if (isLockOn_)
	{
		UpdateLockOnTracking();
	}
	else
	{
		UpdateFreeTarget();
	}
}

void GameScene::StartLockOn(EnemyBase* target)
{
	lockOnTarget_ = target;
	isLockOn_ = true;

	player_->SetLockOn(true);

	mainCamera.SetLockOnTarget(&lockOnTarget_->GetTransform());
	mainCamera.ChangeMode(Camera::MODE::LOCK_ON);
}

void GameScene::EndLockOn(void)
{
	isLockOn_ = false;
	lockOnTarget_ = nullptr;

	player_->SetLockOn(false);
	player_->ClearAttackTarget();

	mainCamera.SetLockOnTarget(nullptr);
	mainCamera.ChangeMode(Camera::MODE::FOLLOW);
}

// ロックオン中: 敵の方を向いて、攻撃の狙いを敵に合わせる
void GameScene::UpdateLockOnTracking(void)
{
	// 高速移動中はカメラを落ち着かせる(向きの追従をゆっくり、位置の追従を速く)
	mainCamera.SetBoostChase(player_->IsBoostChasing());

	if (lockOnTarget_ == nullptr || lockOnTarget_->IsDead())
	{
		EndLockOn();
		return;
	}

	// かめはめ波中は敵を追尾しない
	if (player_->IsKamehame())
	{
		return;
	}

	const VECTOR targetPos = lockOnTarget_->GetTransform().pos;

	player_->SetAttackTarget(targetPos);

	// 8段目(叩き落とし)は向きを変えない
	if (player_->GetCombo() != FINISH_COMBO)
	{
		player_->LookAtTarget(targetPos);
	}
}

// ロックオンしていないとき: 近くに敵がいれば攻撃の狙いだけ合わせる
void GameScene::UpdateFreeTarget(void)
{
	constexpr float FREE_TARGET_RANGE = 300.0f;

	EnemyBase* nearest = FindNearestEnemy(FREE_TARGET_RANGE);

	if (nearest != nullptr)
	{
		player_->SetAttackTarget(nearest->GetTransform().pos);
	}
	else
	{
		player_->ClearAttackTarget();
	}
}

EnemyBase* GameScene::FindNearestEnemy(float maxRange) const
{
	EnemyBase* nearest = nullptr;
	float nearestDistance = maxRange;

	const VECTOR playerPos = player_->GetTransform().pos;

	for (const auto& enemy : enemies_)
	{
		if (!enemy || enemy->IsDead())
		{
			continue;
		}

		const float distance =
			VSize(VSub(enemy->GetTransform().pos, playerPos));

		if (distance < nearestDistance)
		{
			nearestDistance = distance;
			nearest = enemy.get();
		}
	}

	return nearest;
}

EnemyBase* GameScene::PickTarget(float maxRange) const
{
	if (lockOnTarget_ != nullptr && !lockOnTarget_->IsDead())
	{
		return lockOnTarget_;
	}

	return FindNearestEnemy(maxRange);
}

//-----------------------------------------------------------------------------
// 技: バニッシュアタック / 追撃 / 相殺 / ガードバースト
//-----------------------------------------------------------------------------

// バニッシュアタック: ガード中に×で、敵の斜め後ろへ瞬間移動し、少し間をおいて叩き落とす
void GameScene::UpdateVanish(float dt)
{
	if (player_->ConsumeVanish())
	{
		StartVanish();
	}

	if (vanishTarget_ == nullptr)
	{
		return;
	}

	// 少し間をおいてから叩き落とす(消えて、現れて、叩く、が目で追えるように)
	vanishTimer_ -= dt;

	if (vanishTarget_->IsDead())
	{
		vanishTarget_ = nullptr;
		return;
	}

	if (vanishTimer_ > 0.0f)
	{
		return;
	}

	EnemyBase* target = vanishTarget_;
	vanishTarget_ = nullptr;

	const int damage = static_cast<int>(
		player_->GetAttackDamage(Player::ATTACK_DAMAGE) * PlayerConfig::Vanish::DAMAGE_RATE);

	DealDamage(*target, damage, PlayerConfig::Vanish::STUN);

	MeleeEnemy* melee = AsMelee(target);

	if (melee != nullptr)
	{
		melee->LookAtPlayer();
		melee->StartSlamDown();
	}

	const VECTOR slamDir = VNorm(VGet(vanishDir_.x, -0.8f, vanishDir_.z));
	target->AddKnockBack(slamDir, PlayerConfig::Vanish::KNOCK);

	EffekseerEffect::GetInstance()->PlayHitEffect(HitEffectPos(*target), 0.0f);

	ApplyHitImpact(
		PlayerConfig::Vanish::HIT_STOP,
		PlayerConfig::Vanish::SHAKE_TIME,
		PlayerConfig::Vanish::SHAKE_POWER);
}

void GameScene::StartVanish(void)
{
	// ロックオン中の敵。いなければ、範囲内で一番近い敵
	EnemyBase* target = PickTarget(PlayerConfig::Vanish::MAX_RANGE);

	if (target == nullptr)
	{
		return;
	}

	const VECTOR enemyPos = target->GetTransform().pos;

	// 敵の向きの後ろ側(水平)
	VECTOR back = target->GetTransform().quaRot.GetForward();
	back.y = 0.0f;
	back = NormalizeOr(back, VGet(0.0f, 0.0f, 1.0f));

	const VECTOR warpPos =
		CalcVanishPos(enemyPos, back, player_->GetTransform().pos);

	player_->VanishTo(warpPos, enemyPos);

	vanishTarget_ = target;
	vanishTimer_ = PlayerConfig::Vanish::HIT_DELAY;
	vanishDir_ = back;
}

// 追撃: ふっ飛ばした敵のところまで飛んでいって、追い打ちする
void GameScene::UpdatePursuit(float dt)
{
	if (player_->ConsumePursuit())
	{
		StartPursuit();
	}

	if (pursuitTarget_ == nullptr)
	{
		return;
	}

	pursuitTimer_ -= dt;

	if (pursuitTarget_->IsDead())
	{
		pursuitTarget_ = nullptr;
		return;
	}

	namespace pu = PlayerConfig::Pursuit;

	// 敵の手前まで、決まった速さで近づく
	const VECTOR enemyNow = pursuitTarget_->GetTransform().pos;
	const VECTOR playerNow = player_->GetTransform().pos;

	VECTOR flat = VSub(enemyNow, playerNow);
	flat.y = 0.0f;
	pursuitDir_ = NormalizeOr(flat, pursuitDir_);

	VECTOR stop = VSub(enemyNow, VScale(pursuitDir_, pu::DISTANCE));
	stop.y = enemyNow.y + pu::HEIGHT;

	if (stop.y < pu::MIN_HEIGHT)
	{
		stop.y = pu::MIN_HEIGHT;
	}

	const VECTOR toStop = VSub(stop, playerNow);
	const float remain = VSize(toStop);
	const float step = pu::DASH_SPEED * dt;

	const bool arrived = (remain <= step) || (pursuitTimer_ <= 0.0f);

	if (arrived)
	{
		player_->DashStepTo(stop, enemyNow);
	}
	else
	{
		player_->DashStepTo(VAdd(playerNow, VScale(toStop, step / remain)), enemyNow);
		return;
	}

	// 到着: 追い打ち
	EnemyBase* target = pursuitTarget_;
	pursuitTarget_ = nullptr;

	const int damage = static_cast<int>(
		player_->GetAttackDamage(Player::ATTACK_DAMAGE) * pu::DAMAGE_RATE);

	DealDamage(*target, damage, pu::STUN);

	MeleeEnemy* melee = AsMelee(target);

	if (melee != nullptr)
	{
		melee->LookAtPlayer();
	}

	// また遠くへふっ飛ばす
	target->AddKnockBack(VNorm(VGet(pursuitDir_.x, 0.35f, pursuitDir_.z)), pu::KNOCK);

	EffekseerEffect::GetInstance()->PlayHitEffect(HitEffectPos(*target), 0.0f);

	// (追撃はブレイク後の最初の一発だけ。続けての追撃はなし)
	ApplyHitImpact(pu::HIT_STOP, pu::SHAKE_TIME, pu::SHAKE_POWER);
}

void GameScene::StartPursuit(void)
{
	// ロックオン中の敵。いなければ、一番近い敵
	EnemyBase* target = PickTarget(FLT_MAX);

	if (target == nullptr)
	{
		return;
	}

	// 瞬間移動はせず、敵のところまで速く飛んでいく(UpdatePursuit で毎コマ動かす)
	pursuitTarget_ = target;
	pursuitTimer_ = PlayerConfig::Pursuit::HIT_DELAY;
	pursuitDir_ = VGet(0.0f, 0.0f, 1.0f);
}

// 相殺勝負: かめはめ波が敵の大きな気弾・ビームにぶつかったら、連打で押し合う
void GameScene::UpdateClash(float dt)
{
	if (clash_->Update(dt, *player_, enemies_) == ClashSystem::RESULT::WIN)
	{
		// 押し勝った瞬間に、少し止める
		StartHitStop(ClashSystem::WIN_HIT_STOP);
	}
}

void GameScene::UpdateGuardBurst(void)
{
	if (!player_->IsGuardBurstTrigger())
	{
		return;
	}

	mainCamera.StartShake(BURST_SHAKE_TIME, BURST_SHAKE_POWER);

	const VECTOR playerPos = player_->GetTransform().pos;

	for (auto& enemy : enemies_)
	{
		if (!enemy || enemy->IsDead())
		{
			continue;
		}

		VECTOR dir = VSub(enemy->GetTransform().pos, playerPos);
		const float distance = VSize(dir);

		// バーストの範囲外、またはぴったり重なっているとき
		if (distance > BURST_RANGE || distance <= 0.001f)
		{
			continue;
		}

		MeleeEnemy* melee = AsMelee(enemy.get());

		if (melee != nullptr)
		{
			melee->GuardBurst();
		}

		enemy->AddKnockBack(VNorm(dir), BURST_KNOCK);
	}
}

//-----------------------------------------------------------------------------
// 演出: ジャスト回避 / ブレイク / ヒートアップ
//-----------------------------------------------------------------------------

// ジャスト回避が成立したら、しばらく敵だけをスローモーションにする
//   (敵を数フレームに1回だけ更新する。プレイヤーは普通に動けるので、反撃できる)
void GameScene::UpdateJustDodge(void)
{
	if (player_->ConsumeJustDodge())
	{
		justSlowTimer_ = PlayerConfig::JustDodge::SLOW_TIME;
		justSlowFrame_ = 0;
	}
}

// ブレイク(スタン)した瞬間: カカロットのように全体を止めて演出を見せる
void GameScene::UpdateBreakFreeze(void)
{
	for (auto& enemy : enemies_)
	{
		if (!enemy || enemy->IsDead())
		{
			continue;
		}

		const bool now = enemy->IsStunned();
		bool& prev = prevStun_[enemy.get()];

		// スタンしていなかった敵が、スタンした瞬間
		if (now && !prev)
		{
			// 全体を止めて、ブレイク演出(自前の時計)を再生する
			breakFreeze_ = PlayerConfig::Break::FREEZE_TIME;

			const VECTOR hitPos = HitEffectPos(*enemy);
			BreakEffect::Play(hitPos, PlayerConfig::Break::FREEZE_TIME);
			EffekseerEffect::GetInstance()->PlayHitEffect(hitPos, 0.0f);
		}

		prev = now;
	}
}

// ヒートアップ発動の演出カメラ(光の柱 -> はじける -> 拳と顔のアップ -> 引き)
void GameScene::UpdateHeatUpCamera(void)
{
	namespace hu = PlayerConfig::HeatUp;

	const bool activating = player_->IsHeatActivating();

	if (!activating)
	{
		if (heatWasActivating_)
		{
			mainCamera.EndCinematic();
		}

		heatWasActivating_ = false;
		return;
	}

	// 発動した瞬間
	if (!heatWasActivating_)
	{
		heatShookBurst_ = false;
		heatShookClose_ = false;

		// 敵の飛び道具・技を消す(止まった弾が残らないように)
		for (auto& enemy : enemies_)
		{
			if (!enemy || enemy->IsDead())
			{
				continue;
			}

			MeleeEnemy* melee = AsMelee(enemy.get());

			if (melee != nullptr)
			{
				melee->CancelAttacksForHeatUp();
			}
		}

		using Key = Camera::CineKey;

		// 座標は プレイヤーからの相対: x=右 y=上 z=前(プレイヤーの向き)
		const std::vector<Key> keys =
		{
			// 正面の少し低い位置から、昇っていく光を見上げる
			{ 0.0f,                   { 110.0f,  20.0f, 460.0f }, { 0.0f, 110.0f, 0.0f }, 60.0f, false },
			{ hu::ACTIVATE_FLASH,     { 110.0f, -10.0f, 420.0f }, { 0.0f, 230.0f, 0.0f }, 62.0f, false },
			// 光がはじける: 少し引いて画角を広げる
			{ hu::ACTIVATE_BURST_END, {  90.0f,  40.0f, 470.0f }, { 0.0f, 150.0f, 0.0f }, 72.0f, false },
			// 拳と顔のアップ(カット)。画角を狭め、ゆっくり寄る
			{ hu::ACTIVATE_BURST_END, {  25.0f, 135.0f, 150.0f }, { 0.0f, 120.0f, 0.0f }, 38.0f, true },
			{ hu::ACTIVATE_CLOSE_END, {  25.0f, 135.0f, 115.0f }, { 0.0f, 120.0f, 0.0f }, 34.0f, false },
			// 引いて、金色をまとった全身を見せる(カット -> 少しずつ引く)
			{ hu::ACTIVATE_CLOSE_END, {  40.0f, 110.0f, 180.0f }, { 0.0f, 100.0f, 0.0f }, 55.0f, true },
			{ hu::ACTIVATE_TIME,      {  40.0f, 110.0f, 340.0f }, { 0.0f, 100.0f, 0.0f }, 60.0f, false },
		};

		mainCamera.StartCinematic(keys);
	}

	heatWasActivating_ = true;

	mainCamera.UpdateCinematicPivot(player_->GetTransform().pos, player_->GetForward());

	// 光がはじける瞬間と、アップになる瞬間に画面を揺らす
	const float t = player_->GetHeatActivateElapsed();

	if (!heatShookBurst_ && t >= hu::ACTIVATE_FLASH)
	{
		heatShookBurst_ = true;
		mainCamera.StartShake(0.4f, 7.0f);
	}

	if (!heatShookClose_ && t >= hu::ACTIVATE_BURST_END)
	{
		heatShookClose_ = true;
		mainCamera.StartShake(0.7f, 3.0f);
	}
}

//-----------------------------------------------------------------------------
// 敵と体
//-----------------------------------------------------------------------------

void GameScene::UpdateEnemies(float dt)
{
	bool updateEnemy = true;

	// ジャスト回避のスロー中は、数フレームに1回だけ更新する
	if (justSlowTimer_ > 0.0f)
	{
		justSlowTimer_ -= dt;
		justSlowFrame_++;

		updateEnemy = (justSlowFrame_ % PlayerConfig::JustDodge::SLOW_DIV) == 0;
	}

	// ヒートアップ発動の演出中は、敵も止まる
	if (player_->IsHeatActivating())
	{
		updateEnemy = false;
	}

	if (!updateEnemy)
	{
		return;
	}

	for (auto& enemy : enemies_)
	{
		if (enemy && !enemy->IsDead())
		{
			enemy->Update();
		}
	}
}

// プレイヤーと敵が重ならないように押し出す
void GameScene::PushOutBodies(void)
{
	for (auto& enemy : enemies_)
	{
		if (!enemy || enemy->IsDead())
		{
			continue;
		}

		const VECTOR playerPos = player_->GetTransform().pos;
		const VECTOR enemyPos = enemy->GetTransform().pos;

		// 上や下に離れているときは重なりとみなさない
		if (std::fabs(enemyPos.y - playerPos.y) > BODY_HEIGHT_RANGE)
		{
			continue;
		}

		// 敵 -> プレイヤー の水平方向
		VECTOR diff = VSub(playerPos, enemyPos);
		diff.y = 0.0f;

		const float dist = VSize(diff);

		if (dist >= BODY_DISTANCE)
		{
			continue;
		}

		VECTOR dir;

		if (dist > 0.001f)
		{
			dir = VScale(diff, 1.0f / dist);
		}
		else
		{
			// ぴったり重なっているときは、プレイヤーの後ろ側へ逃がす
			VECTOR forward = player_->GetForward();
			forward.y = 0.0f;

			dir = VScale(NormalizeOr(forward, VGet(0.0f, 0.0f, 1.0f)), -1.0f);
		}

		float overlap = BODY_DISTANCE - dist;

		if (overlap > BODY_MAX_PUSH)
		{
			overlap = BODY_MAX_PUSH;
		}

		player_->PushOut(VScale(dir, overlap * BODY_PLAYER_RATE));
		enemy->PushOut(VScale(dir, -overlap * (1.0f - BODY_PLAYER_RATE)));
	}
}

//-----------------------------------------------------------------------------
// 当たり判定
//-----------------------------------------------------------------------------

// 近接攻撃
void GameScene::UpdateMeleeHit(void)
{
	if (!player_->IsAttackHitTiming() || player_->HasAttackHit())
	{
		return;
	}

	const int combo = player_->GetCombo();
	const bool isFinish = (combo == FINISH_COMBO);

	// 形態に応じたダメージ(変身すると倍率が変わる)
	const int attackDamage = player_->GetAttackDamage(Player::ATTACK_DAMAGE);

	// 攻撃範囲(水平距離)と高さの許容範囲
	const float attackRange = isFinish ? FINISH_RANGE : ActorBase::ATTACK_RANGE;
	const float attackHeight = isFinish ? FINISH_HEIGHT_RANGE : ATTACK_HEIGHT_RANGE;

	const VECTOR playerPos = player_->GetTransform().pos;

	VECTOR forward = player_->GetForward();
	forward.y = 0.0f;
	const bool hasForward = VSize(forward) > 0.001f;

	if (hasForward)
	{
		forward = VNorm(forward);
	}

	for (auto& enemy : enemies_)
	{
		// 回避中(無敵)の敵には当たらない
		if (!enemy || enemy->IsDead() || enemy->IsInvincible())
		{
			continue;
		}

		const VECTOR toEnemy = VSub(enemy->GetTransform().pos, playerPos);

		// 水平距離と高さの差を別々に見る
		// (3D距離だと、敵が上下にいるだけで届かなくなるため)
		VECTOR horizontal = toEnemy;
		horizontal.y = 0.0f;

		const float horizontalDist = VSize(horizontal);

		if (horizontalDist > attackRange ||
			std::fabs(toEnemy.y) > attackHeight)
		{
			continue;
		}

		// 向き判定(真上・真下など水平距離がほぼ0のときは判定しない)
		if (hasForward && horizontalDist > ATTACK_DOT_MIN_DISTANCE)
		{
			const float dot =
				VDot(forward, VScale(horizontal, 1.0f / horizontalDist));

			if (dot < ActorBase::ATTACK_DOT)
			{
				continue;
			}
		}

		MeleeEnemy* melee = AsMelee(enemy.get());

		// 敵がガード中(8段目はガードを崩すので貫通)
		if (melee != nullptr && melee->IsGuard() && !isFinish)
		{
			// ガード耐久値を減らす(攻撃力が高いほど削れる)
			melee->GuardDamage(GUARD_DAMAGE * player_->GetAttackRate());

			EffekseerEffect::GetInstance()->PlayHitEffect(HitEffectPos(*enemy), 0.0f);

			StartHitStop(0.03f);

			player_->SetAttackHit();
			return;
		}

		// ヒット
		if (melee != nullptr)
		{
			melee->LookAtPlayer();
		}

		// フィニッシュは大きくスタン値を溜める
		DealDamage(*enemy, attackDamage, isFinish ? STUN_FINISH : STUN_NORMAL);

		VECTOR hitDir = NormalizeOr(toEnemy, toEnemy);

		// 8段目は下に叩き落とす
		if (isFinish)
		{
			hitDir = VNorm(VGet(forward.x, -0.8f, forward.z));

			if (melee != nullptr)
			{
				melee->StartSlamDown();
			}

			// 8段目が当たったら高速追撃できる
			player_->SetCanChase(true);
		}

		const float knock = (combo >= 1 && combo <= FINISH_COMBO) ? KNOCK_POWER[combo] : 0.0f;
		enemy->AddKnockBack(hitDir, knock);

		EffekseerEffect::GetInstance()->PlayHitEffect(HitEffectPos(*enemy), 0.0f);

		if (isFinish)
		{
			ApplyHitImpact(0.10f, 0.15f, 8.0f);
		}
		else
		{
			ApplyHitImpact(0.05f, 0.08f, 3.0f);
		}

		// クリティカルは、少し長く止めて画面を揺らす
		if (player_->IsLastCritical())
		{
			hitStopTimer_ += DamageCalc::CRITICAL_HIT_STOP;
			mainCamera.StartShake(0.10f, 5.0f);
		}

		player_->SetAttackHit();

		// 1攻撃で1体だけ
		return;
	}
}

// 気弾
void GameScene::UpdateKiBlastHit(void)
{
	for (auto& blast : player_->GetKiBlasts())
	{
		if (blast->IsDead())
		{
			continue;
		}

		for (auto& enemy : enemies_)
		{
			if (!enemy || enemy->IsDead())
			{
				continue;
			}

			if (VSize(VSub(blast->GetPos(), EnemyCenter(*enemy))) > ENEMY_HIT_RADIUS)
			{
				continue;
			}

			DealDamage(*enemy, player_->GetAttackDamage(KI_BLAST_DAMAGE_TYPE), KI_BLAST_STUN);

			blast->Hit();
			break;
		}
	}
}

// 連続エネルギー弾: 着弾した弾の爆発と、枠の中の敵へのダメージ
void GameScene::UpdateBarrageHit(void)
{
	for (auto& shot : player_->GetBarrageShots())
	{
		if (!shot.exploded || shot.handled)
		{
			continue;
		}

		shot.handled = true;

		// 着弾の光
		EffekseerEffect::GetInstance()->PlayHitEffect(shot.pos, 0.0f);

		// 着弾点から枠の半径の中にいる敵に当たる(逃げられると外れる)
		for (auto& enemy : enemies_)
		{
			if (!enemy || enemy->IsDead())
			{
				continue;
			}

			// 敵が画面の枠の中にいれば当たる(枠の外に出ていると外れる)
			const VECTOR sp = ConvWorldPosToScreenPos(EnemyCenter(*enemy));

			if (sp.z <= 0.0f || sp.z >= 1.0f)
			{
				continue;
			}

			const VECTOR rc = PlayerBarrage::RingCenter();
			const float dx = sp.x - rc.x;
			const float dy = sp.y - rc.y;

			if (std::sqrt(dx * dx + dy * dy) > PlayerBarrage::RingRadius())
			{
				continue;
			}

			DealDamage(*enemy, player_->GetAttackDamage(KI_BLAST_DAMAGE_TYPE),
				PlayerConfig::Barrage::STUN);
			break;
		}
	}
}

// フィニッシュ気弾
void GameScene::UpdateFinishBlastHit(void)
{
	for (auto& finish : player_->GetFinishBlasts())
	{
		if (finish->IsDead())
		{
			continue;
		}

		for (auto& enemy : enemies_)
		{
			if (!enemy || enemy->IsDead())
			{
				continue;
			}

			const VECTOR center = EnemyCenter(*enemy);

			if (VSize(VSub(finish->GetPos(), center)) > finish->GetRadius() + ENEMY_HIT_RADIUS)
			{
				continue;
			}

			DealDamage(*enemy,
				player_->GetAttackDamage(PlayerConfig::Finish::DAMAGE),
				PlayerConfig::Finish::STUN);

			enemy->AddKnockBack(finish->GetDir(), PlayerConfig::Finish::KNOCK);

			EffekseerEffect::GetInstance()->PlayHitEffect(center, 0.0f);

			ApplyHitImpact(
				FINISH_BLAST_HIT_STOP,
				FINISH_BLAST_SHAKE_TIME,
				FINISH_BLAST_SHAKE_POWER);

			finish->Kill();

			// (追撃はブレイクしたときだけ)
			break;
		}
	}
}

// かめはめ波
void GameScene::UpdateKamehameHit(float dt)
{
	if (!player_->IsKamehameBeam())
	{
		// ビームが終わったら、敵の「ビームを受けている」状態を戻す
		kamehameDamageTimer_ = 0.0f;

		for (auto& enemy : enemies_)
		{
			MeleeEnemy* melee = AsMelee(enemy.get());

			if (melee != nullptr)
			{
				melee->SetKamehameHit(false);
			}
		}

		return;
	}

	kamehameDamageTimer_ += dt;

	// 相殺勝負の最中は、ビームのダメージは入らない
	if (clash_->IsActive())
	{
		return;
	}

	const VECTOR beamStart = player_->GetKamehameStartPos();
	const VECTOR beamVec = VSub(player_->GetKamehameEndPos(), beamStart);
	const float beamLengthSq = VDot(beamVec, beamVec);

	if (beamLengthSq <= 0.001f)
	{
		return;
	}

	const float hitRadius = player_->GetKamehameRadius() + ENEMY_HIT_RADIUS;

	const VECTOR pushDir = NormalizeOr(player_->GetForward(), player_->GetForward());

	for (auto& enemy : enemies_)
	{
		if (!enemy || enemy->IsDead())
		{
			continue;
		}

		MeleeEnemy* melee = AsMelee(enemy.get());

		// 最初は「ビームに当たっていない」にする
		if (melee != nullptr)
		{
			melee->SetKamehameHit(false);
		}

		// ビームの線(始点から終点)の上で、敵の中心に一番近い点
		const VECTOR center = EnemyCenter(*enemy);

		float t = VDot(VSub(center, beamStart), beamVec) / beamLengthSq;

		if (t < 0.0f) { t = 0.0f; }
		if (t > 1.0f) { t = 1.0f; }

		const VECTOR closest = VAdd(beamStart, VScale(beamVec, t));

		// ビームに当たっていない
		if (VSize(VSub(center, closest)) > hitRadius)
		{
			continue;
		}

		// ビームに当たっている
		if (melee != nullptr)
		{
			melee->SetKamehameHit(true, pushDir);
		}

		// ダメージだけ一定間隔ごと
		if (kamehameDamageTimer_ >= BEAM_DAMAGE_INTERVAL)
		{
			DealDamage(*enemy, player_->GetAttackDamage(BEAM_DAMAGE_TYPE), BEAM_STUN);

			kamehameDamageTimer_ = 0.0f;
		}
	}
}

//-----------------------------------------------------------------------------
// 共通の部品
//-----------------------------------------------------------------------------

void GameScene::DealDamage(EnemyBase& enemy, int damage, float stun)
{
	enemy.Damage(damage);
	enemy.AddStun(stun);

	player_->AddDamagePopup(PopupPos(enemy), damage);
}

void GameScene::StartHitStop(float time)
{
	isHitStop_ = true;
	hitStopTimer_ = time;
}

void GameScene::ApplyHitImpact(float stopTime, float shakeTime, float shakePower)
{
	StartHitStop(stopTime);
	mainCamera.StartShake(shakeTime, shakePower);
}

bool GameScene::IsAllEnemiesDead(void) const
{
	// 敵が1体もいないステージ(作りかけのステージ)は、クリアにしない
	if (enemies_.empty())
	{
		return false;
	}

	for (const auto& enemy : enemies_)
	{
		if (enemy && !enemy->IsDead())
		{
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// 描画
//-----------------------------------------------------------------------------

void GameScene::Draw(void)
{
	// 背景
	skyDome_->Draw();
	stage_->Draw();

	player_->Draw();

	for (auto& enemy : enemies_)
	{
		if (enemy && !enemy->IsDead())
		{
			enemy->Draw();
		}
	}

	// 相殺勝負: ぶつかっている光
	clash_->Draw(*player_);

	// ステージクリア演出
	if (gameState_ == GAME_STATE::CLEAR)
	{
		DrawClear();
	}

	// ポストエフェクト(mode_ が大きいほど、重ねる枚数が増える)
	if (mode_ < MODE::MONO) { return; }
	DrawPostPass(*monoRenderer_);

	if (mode_ < MODE::SCAN) { return; }
	scanMaterial_->SetConstBuf(
		1, { SceneManager::GetInstance().GetTotalTime(), 0.0f, 0.0f, 0.0f });
	DrawPostPass(*scanRenderer_);

	if (mode_ < MODE::LENS) { return; }
	DrawPostPass(*lensRenderer_);

	if (mode_ < MODE::VINE) { return; }
	DrawPostPass(*vineRenderer_);
}

void GameScene::DrawPostPass(PixelRenderer& renderer)
{
	const int mainScreen = SceneManager::GetInstance().GetMainScreen();

	// 専用のスクリーンに加工した絵を描いて
	SetDrawScreen(postEffectScreen_);
	ClearDrawScreen();
	renderer.Draw();

	// メインに戻して貼り付ける
	SetDrawScreen(mainScreen);
	DrawGraph(0, 0, postEffectScreen_, false);
}

// 連続エネルギー弾の狙いの枠(HUDと同じく、一番手前に描く)
void GameScene::DrawBarrageReticle(void)
{
	if (!player_ || !player_->IsBarrageActive())
	{
		return;
	}

	const VECTOR c = PlayerBarrage::RingCenter();
	const float r = PlayerBarrage::RingRadius();

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 50);
	DrawCircleAA(c.x, c.y, r, 64, GetColor(255, 255, 255), TRUE);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 220);
	DrawCircleAA(c.x, c.y, r, 64, GetColor(255, 255, 255), FALSE, 3.0f);

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

//-----------------------------------------------------------------------------
// ステージクリア
//-----------------------------------------------------------------------------

void GameScene::StartClear(void)
{
	gameState_ = GAME_STATE::CLEAR;
	PlayRecord::ClearTime() = playTime_;

	clearTimer_ = 0.0f;
	isResultRequested_ = false;
}

void GameScene::UpdateClear(float dt)
{
	// クリア中はプレイヤーが更新されないので、パッド入力を自分で更新する
	PadInput::Update();

	clearTimer_ += dt;

	if (isResultRequested_)
	{
		return;
	}

	// 少し待ってからは、ボタンを押せば先へ進める
	InputManager& ins = InputManager::GetInstance();

	const bool skip =
		clearTimer_ >= CLEAR_SKIP_TIME &&
		(ins.IsTrgDown(KEY_INPUT_SPACE) ||
			ins.IsTrgDown(KEY_INPUT_RETURN) ||
			PadInput::IsTrg(PadInput::BTN::CIRCLE));

	// 何も押さなくても、時間がたてばタイトルへ戻る
	if (clearTimer_ >= CLEAR_WAIT_TIME || skip)
	{
		isResultRequested_ = true;
		SceneManager::GetInstance().ChangeScene(SceneManager::SCENE_ID::TITLE);
	}
}

void GameScene::DrawClear(void)
{
	// 0.0 ～ 1.0 (最初の0.5秒で出てくる)
	float rate = clearTimer_ / 0.5f;

	if (rate > 1.0f)
	{
		rate = 1.0f;
	}

	// 背景を暗くする
	MenuUI::DrawDim(static_cast<int>(150.0f * rate), GetColor(0, 0, 0));

	// STAGE CLEAR 拡大演出 (0.5倍 -> 1.0倍)
	const float scale = 0.5f + 0.5f * rate;
	const int fontSize = static_cast<int>(48.0f * scale);

	const int fontHandle = CreateFontToHandle(nullptr, fontSize, 3);

	const char* text = "STAGE CLEAR!";

	const int textWidth = GetDrawStringWidthToHandle(text, -1, fontHandle);

	const int drawX = Application::SCREEN_SIZE_X / 2 - textWidth / 2;
	const int drawY = Application::SCREEN_SIZE_Y / 2 - fontSize / 2;

	// 文字もフェードイン
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(255.0f * rate));

	DrawStringToHandle(drawX, drawY, text, GetColor(255, 230, 80), fontHandle);

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 255);

	DeleteFontToHandle(fontHandle);

	// ボタンで進めるようになったら案内を出す
	if (clearTimer_ >= CLEAR_SKIP_TIME)
	{
		const char* hint = "SPACE / A : TITLE";

		const int hintWidth = GetDrawStringWidth(hint, -1);

		DrawString(
			Application::SCREEN_SIZE_X / 2 - hintWidth / 2,
			Application::SCREEN_SIZE_Y / 2 + 70,
			hint,
			GetColor(255, 255, 255));
	}

	// 終わりが近づいたら、画面が白くなっていく
	const float fadeStart = CLEAR_WAIT_TIME - CLEAR_FADE_TIME;

	if (clearTimer_ > fadeStart)
	{
		float fade = (clearTimer_ - fadeStart) / CLEAR_FADE_TIME;

		if (fade > 1.0f)
		{
			fade = 1.0f;
		}

		MenuUI::DrawDim(static_cast<int>(255.0f * fade * fade), GetColor(255, 255, 255));
	}
}

//-----------------------------------------------------------------------------
// ポーズ
//-----------------------------------------------------------------------------

void GameScene::StartPause(void)
{
	gameState_ = GAME_STATE::PAUSE;
	isMenuDecided_ = false;

	// Effekseerの再生も止める(SceneManager::Draw 側で IsEffectFrozen を見る)
	AppFlow::SetEffectFreeze(true);

	OpenPauseMenu(0);
}

void GameScene::EndPause(void)
{
	gameState_ = GAME_STATE::PLAY;
	confirmAction_ = MENU_ACTION::NONE;
	menu_->Close();

	AppFlow::SetEffectFreeze(false);

	// ボタンを押したままだと、再開した最初のフレームで「押した瞬間」と
	// 判定されてしまうので、ここで1回更新して「押している状態」にしておく
	PadInput::Update();
}

void GameScene::OpenPauseMenu(int cursor)
{
	confirmAction_ = MENU_ACTION::NONE;
	menu_->Open({ "再開", "リトライ", "タイトルへ戻る", "ゲーム終了" }, cursor);
}

void GameScene::UpdatePause(void)
{
	InputManager& ins = InputManager::GetInstance();

	// プレイヤーが更新されない間は、パッド入力を自分で更新する
	PadInput::Update();

	if (isMenuDecided_)
	{
		return;
	}

	const int result = menu_->Update(SceneManager::GetInstance().GetDeltaTime());

	// 「はい / いいえ」の確認中
	if (confirmAction_ != MENU_ACTION::NONE)
	{
		if (result == 0)
		{
			ExecuteAction(confirmAction_);
		}
		else if (result == 1 || result == MenuUI::RESULT_CANCEL)
		{
			// 元のメニューへ戻る(さっき選んでいた項目にカーソルを置く)
			OpenPauseMenu(static_cast<int>(confirmAction_));
		}
		return;
	}

	// ポーズボタンをもう一度押しても再開できる
	const bool resumeKey =
		ins.IsTrgDown(KEY_INPUT_P) ||
		PadInput::IsTrg(PadInput::BTN::OPTIONS);

	if (resumeKey || result == MenuUI::RESULT_CANCEL || result == 0)
	{
		EndPause();
		return;
	}

	// リトライ / タイトル / 終了は、間違えて押しても大丈夫なように確認を挟む
	MENU_ACTION action = MENU_ACTION::NONE;
	const char* caption = "";

	if (result == 1)
	{
		action = MENU_ACTION::RETRY;
		caption = "最初からやり直しますか?";
	}
	else if (result == 2)
	{
		action = MENU_ACTION::TITLE;
		caption = "タイトルに戻りますか?";
	}
	else if (result == 3)
	{
		action = MENU_ACTION::QUIT;
		caption = "ゲームを終了しますか?";
	}

	if (action != MENU_ACTION::NONE)
	{
		confirmAction_ = action;
		menu_->Open({ "はい", "いいえ" }, 1);
		menu_->SetCaption(caption);
	}
}

void GameScene::ExecuteAction(MENU_ACTION action)
{
	confirmAction_ = MENU_ACTION::NONE;

	switch (action)
	{
	case MENU_ACTION::RETRY:
		// シーンを作り直す = 最初からやり直し
		isMenuDecided_ = true;
		SceneManager::GetInstance().ChangeScene(SceneManager::SCENE_ID::GAME);
		break;

	case MENU_ACTION::TITLE:
		isMenuDecided_ = true;
		SceneManager::GetInstance().ChangeScene(SceneManager::SCENE_ID::TITLE);
		break;

	case MENU_ACTION::QUIT:
		// メインループが AppFlow::IsQuitRequested() を見て終わる
		AppFlow::RequestQuit();
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// ゲームオーバー
//-----------------------------------------------------------------------------

void GameScene::StartGameOver(void)
{
	gameState_ = GAME_STATE::GAME_OVER;
	gameOverTimer_ = 0.0f;
	isResultRequested_ = false;
	menu_->Close();

	// エフェクトも止めて、暗くなっていく間は静止させる
	AppFlow::SetEffectFreeze(true);
}

void GameScene::UpdateGameOver(void)
{
	gameOverTimer_ += SceneManager::GetInstance().GetDeltaTime();

	// 暗くなりきったら、ゲームオーバー画面へ移る
	if (gameOverTimer_ >= GAME_OVER_WAIT_TIME && !isResultRequested_)
	{
		isResultRequested_ = true;
		SceneManager::GetInstance().ChangeScene(SceneManager::SCENE_ID::GAME_OVER);
	}
}

//-----------------------------------------------------------------------------
// ポーズ / ゲームオーバーの画面(一番手前に描く)
//-----------------------------------------------------------------------------

void GameScene::DrawMenuOverlay(void)
{
	if (!menu_)
	{
		return;
	}

	if (gameState_ == GAME_STATE::PAUSE)
	{
		MenuUI::DrawDim(150, GetColor(0, 0, 20));

		// 確認中は見出しを出さず、質問だけにする
		if (confirmAction_ == MENU_ACTION::NONE)
		{
			menu_->DrawHeading("PAUSE", 150, GetColor(255, 255, 255), 255);
		}

		menu_->Draw(300);
	}
	else if (gameState_ == GAME_STATE::GAME_OVER)
	{
		// 画面がだんだん暗い赤になる(文字はゲームオーバー画面で出す)
		float dimRate = gameOverTimer_ / GAME_OVER_WAIT_TIME;

		if (dimRate > 1.0f)
		{
			dimRate = 1.0f;
		}

		MenuUI::DrawDim(static_cast<int>(230.0f * dimRate), GetColor(25, 0, 0));
	}
}
