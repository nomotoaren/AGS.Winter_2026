#include <DxLib.h>
#include <cmath>
#include <cfloat>
#include <vector>
#include <cstdio>
#include "../../Application.h"
#include "../../Utility/AsoUtility.h"
#include "../../Utility/DamageCalc.h"
#include "../../Manager/SceneManager.h"
#include "../../Manager/Camera/Camera.h"
#include "../../Manager/InputManager.h"
#include "../../Manager/PadInput.h"
#include "../../Manager/EffekseerEffect.h"
#include "../../Manager/AppFlow.h"
#include "../../Manager/PlayRecord.h"
#include "../../Manager/GameProgress.h"
#include "../../Manager/StageData.h"
#include "../../Common/MenuUI.h"
#include "../../Object/Common/Capsule.h"
#include "../../Object/Common/Collider.h"
#include "../../Object/SkyDome.h"
#include "../../Object/Stage.h"
#include "../../Object/Player/Player.h"
#include "../../Object/Enemy/EnemyBase.h"
#include "../../Object/Enemy/BreakEffect.h"
#include "../../Object/Planet.h"
#include "../../Object/Enemy/definition/EnemyFactory.h"
#include "../../Renderer/PixelMaterial.h"
#include "../../Renderer/PixelRenderer.h"
#include "GameScene.h"
#include "ClashSystem.h"
#include "GameScene_Internal.h"

using namespace GameSceneInternal;

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
	const StageData::Info& info = StageData::Get(GameProgress::Get().selectedStage);

	for (int i = 0; i < info.spawnCount; i++)
	{
		const StageData::Spawn& s = info.spawns[i];
		const VECTOR pos = VGet(s.x, s.y, s.z);

		enemies_.push_back(EnemyFactory::Create(s.type, *player_, s.hp, &pos));
	}
}

//-----------------------------------------------------------------------------
// 更新
//-----------------------------------------------------------------------------

void GameScene::Update(void)
{
	const float dt = SceneManager::GetInstance().GetDeltaTime();

	if (gameState_ != GAME_STATE::PLAY)
	{
		SceneManager::GetInstance().SetTimeScale(1.0f);
	}

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
	// 技パレットを開いている間(Tab 長押し / パッドのパレット)はスローモーション
	{
		const bool palette =
			InputManager::GetInstance().IsNew(KEY_INPUT_TAB) || PadInput::IsSpecialPalette();

		SceneManager::GetInstance().SetTimeScale(palette ? PALETTE_SLOW_RATE : 1.0f);
	}

	skyDome_->Update();
	stage_->Update();

	UpdateLockOn();

	player_->Update();
	// 連続エネルギー弾: 枠を動かした分だけカメラも向きを変える。構え中は少し寄る
	mainCamera.SetAimTarget(player_->GetBarrage().GetAimPoint(), player_->IsBarrageActive());

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