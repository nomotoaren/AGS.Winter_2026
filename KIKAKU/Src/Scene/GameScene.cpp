#include <DxLib.h>
#include "../Application.h"
#include "../Utility/AsoUtility.h"
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
#include "../Renderer/ModelMaterial.h"
#include "../Renderer/ModelRenderer.h"
#include "GameScene.h"
#include "ClashSystem.h"
#include <cmath>
#include <cfloat>
#include <vector>
#include <unordered_map>

namespace
{
	// プレイヤーと敵が重ならない最小の水平距離
	// (攻撃中に保つ距離 AttackDistance=90 より小さくしておく)
	constexpr float BODY_DISTANCE = 65.0f;
	// この高さ以上離れていたら重なりとみなさない(上や下にいるときは通す)
	constexpr float BODY_HEIGHT_RANGE = 130.0f;
	// 1フレームで押し出す最大量(急に飛ばないようにする)
	constexpr float BODY_MAX_PUSH = 25.0f;
	// 押し出しの配分(プレイヤー側 / 敵側)
	constexpr float BODY_PLAYER_RATE = 0.65f;

	// 攻撃が届く高さの差(これを超えると当たらない)
	constexpr float ATTACK_HEIGHT_RANGE = 150.0f;
	// 水平距離がこれ以下のときは向き判定をしない(真上・真下の敵用)
	constexpr float ATTACK_DOT_MIN_DISTANCE = 20.0f;

	// ブレイクした瞬間の「全体停止」の残り時間
	// (isHitStop_ は同じフレームの別のヒットで上書きされるので、専用の変数を使う)
	float g_breakFreeze = 0.0f;

	// かめはめ波が敵に当たったか(ビームが終わったら追撃のチャンスを出す)
	bool g_kamehameHit = false;
}

GameScene::GameScene(void)
	:
	player_(nullptr),
	skyDome_(nullptr),
	stage_(nullptr),
	kamehameDamageTimer_(0.0f),
	wasKamehame_(false),
	wasKamehameBeam_(false),
	gameState_(GAME_STATE::PLAY),
	clearTimer_(0.0f),
	attackUIHandle_(-1),
	isHitStop_(false),
	hitStopTimer_(0.0f),
	lockOnTarget_(nullptr),
	isLockOn_(false),
	mode_(MODE::MAIN),
	postEffectScreen_(-1),
	menu_(nullptr),
	confirmAction_(MENU_ACTION::NONE),
	isMenuDecided_(false),
	gameOverTimer_(0.0f),
	clash_(nullptr),
	clearDelayTimer_(0.0f),
	playTime_(0.0f),
	isResultRequested_(false)
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
				player_->DrawHud();
			}

			// ブレイク演出(自前の時計で動く)
			BreakEffect::Draw();

			// ポーズ / ゲームオーバーのメニュー(一番手前)
			DrawMenuOverlay();
		});
	clash_ = std::make_unique<ClashSystem>();
	g_breakFreeze = 0.0f;
	g_kamehameHit = false;

	// 前のプレイで残ったブレイク演出を終わらせる(リトライ対策)
	BreakEffect::Update(100.0f);

	// ポーズ / ゲームオーバー用
	menu_ = std::make_unique<MenuUI>();
	confirmAction_ = MENU_ACTION::NONE;
	isMenuDecided_ = false;
	gameOverTimer_ = 0.0f;
	clearDelayTimer_ = 0.0f;
	playTime_ = 0.0f;
	isResultRequested_ = false;
	AppFlow::SetEffectFreeze(false);

	// プレイヤー
	player_ = std::make_unique<Player>();
	player_->Init();

	// ステージ
	stage_ = std::make_unique<Stage>(*player_);
	stage_->Init();

	// ステージの初期設定
	stage_->ChangeStage(Stage::NAME::MAIN_PLANET);

	// スカイドーム
	skyDome_ = std::make_unique<SkyDome>(player_->GetTransform());
	skyDome_->Init();

	mainCamera.SetFollow(&player_->GetTransform());
	mainCamera.ChangeMode(Camera::MODE::FOLLOW);

	// ポストエフェクト用スクリーン
	postEffectScreen_ = MakeScreen(
		Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y, true);

	// ポストエフェクト用(モノトーン)
	monoMaterial_ = std::make_unique<PixelMaterial>("Monotone.cso", 1);
	monoMaterial_->AddConstBuf({ 1.0f, 1.0f, 1.0f, 1.0f });
	monoMaterial_->AddTextureBuf(SceneManager::GetInstance().GetMainScreen());
	monoRenderer_ = std::make_unique<PixelRenderer>(*monoMaterial_);
	monoRenderer_->MakeSquereVertex(
		Vector2(0, 0),
		Vector2(Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y)
	);

	// ポストエフェクト用(走査線)
	scanMaterial_ = std::make_unique<PixelMaterial>("ScanLine.cso", 2);
	scanMaterial_->AddConstBuf({ 1.0f, 1.0f, 1.0f, 1.0f });
	scanMaterial_->AddConstBuf({ 0.0f, 0.0f, 0.0f, 0.0f });
	scanMaterial_->AddTextureBuf(SceneManager::GetInstance().GetMainScreen());
	scanRenderer_ = std::make_unique<PixelRenderer>(*scanMaterial_);
	scanRenderer_->MakeSquereVertex(
		Vector2(0, 0),
		Vector2(Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y)
	);

	// ポストエフェクト用(ビネット)
	vineMaterial_ = std::make_unique<PixelMaterial>("Vignette.cso", 1);
	vineMaterial_->AddConstBuf({ 3.5f, 0.0f, 0.0f, 0.0f });
	vineMaterial_->AddTextureBuf(SceneManager::GetInstance().GetMainScreen());
	vineRenderer_ = std::make_unique<PixelRenderer>(*vineMaterial_);
	vineRenderer_->MakeSquereVertex(
		Vector2(0, 0),
		Vector2(Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y)
	);

	// ポストエフェクト用(レンズの歪み)
	lensMaterial_ = std::make_unique<PixelMaterial>("LensDistortion.cso", 1);
	lensMaterial_->AddConstBuf({ 3.5f, 0.0f, 0.0f, 0.0f });
	lensMaterial_->AddTextureBuf(SceneManager::GetInstance().GetMainScreen());
	lensRenderer_ = std::make_unique<PixelRenderer>(*lensMaterial_);
	lensRenderer_->MakeSquereVertex(
		Vector2(0, 0),
		Vector2(Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y)
	);

	// 初期モード
	mode_ = MODE::MAIN;

	MakeEnemies();

	gameState_ = GAME_STATE::PLAY;
	clearTimer_ = 0.0f;

	SetGlobalAmbientLight(
		GetColorF(
			1.0f,
			1.0f,
			1.0f,
			1.0f
		)
	);

	attackUIHandle_ =
		LoadGraph(
			"Data/Image/AttackUI.png"
		);
}

void GameScene::Update(void)
{
	InputManager& ins = InputManager::GetInstance();

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

	if (gameState_ == GAME_STATE::PLAY)
	{
		// HPが0になったらゲームオーバー
		if (player_->IsDead())
		{
			StartGameOver();
			return;
		}

		// 敵を全部倒したら、少し間をおいてクリア
		if (IsAllEnemiesDead())
		{
			clearDelayTimer_ += dt;

			if (clearDelayTimer_ >= CLEAR_DELAY_TIME)
			{
				StartClear();
				return;
			}
		}
		else
		{
			clearDelayTimer_ = 0.0f;
		}

		// プレイ時間(クリア画面に表示する)
		playTime_ += dt;

		// ポーズボタン(ESC / P / パッドのOPTIONS)
		// ヒートアップ発動中はカメラ演出が進むので、ポーズできないようにする
		const bool pauseTrg =
			ins.IsTrgDown(KEY_INPUT_ESCAPE) ||
			ins.IsTrgDown(KEY_INPUT_P) ||
			PadInput::IsTrg(PadInput::BTN::OPTIONS);

		if (pauseTrg && !player_->IsHeatActivating())
		{
			StartPause();
			return;
		}
	}

	// ブレイク演出は専用の時計で進める(下の全体停止の影響を受けない)
	BreakEffect::Update(dt);

	// ブレイクした瞬間の全体停止
	if (g_breakFreeze > 0.0f)
	{
		g_breakFreeze -= dt;
		return;
	}

	if (isHitStop_)
	{
		hitStopTimer_ -=
			SceneManager::GetInstance().GetDeltaTime();

		if (hitStopTimer_ <= 0.0f)
		{
			hitStopTimer_ = 0.0f;
			isHitStop_ = false;
		}
		else
		{
			return;
		}
	}

	switch (gameState_)
	{
	case GAME_STATE::PLAY:
	{

		// ゲーム更新
		skyDome_->Update();

		stage_->Update();

		// 常時ロックオン
		if (lockOnTarget_ == nullptr ||
			lockOnTarget_->IsDead())
		{
			EnemyBase* nearestEnemy = nullptr;
			float nearestDistance = FLT_MAX;

			for (auto& enemy : enemies_)
			{
				if (!enemy)
				{
					continue;
				}

				if (enemy->IsDead())
				{
					continue;
				}

				VECTOR toEnemy =
					VSub(
						enemy->GetTransform().pos,
						player_->GetTransform().pos
					);

				float distance =
					VSize(toEnemy);

				if (distance < nearestDistance)
				{
					nearestDistance = distance;
					nearestEnemy = enemy.get();
				}
			}

			if (nearestEnemy != nullptr)
			{
				lockOnTarget_ = nearestEnemy;
				isLockOn_ = true;

				player_->SetLockOn(true);

				mainCamera.SetLockOnTarget(
					&lockOnTarget_->GetTransform()
				);

				mainCamera.ChangeMode(
					Camera::MODE::LOCK_ON
				);
			}
		}

		// ロックオン中
		if (isLockOn_)
		{
			// 高速移動中はカメラを落ち着かせる(向きの追従をゆっくり、位置の追従を速く)
			mainCamera.SetBoostChase(player_->IsBoostChasing());

			if (lockOnTarget_ == nullptr ||
				lockOnTarget_->IsDead())
			{
				isLockOn_ = false;
				lockOnTarget_ = nullptr;

				player_->SetLockOn(false);
				player_->ClearAttackTarget();

				mainCamera.SetLockOnTarget(nullptr);
				mainCamera.ChangeMode(
					Camera::MODE::FOLLOW
				);
			}
			else
			{
				VECTOR targetPos =
					lockOnTarget_->GetTransform().pos;

				// かめはめ波中は敵を追尾しない
				if (!player_->IsKamehame())
				{
					player_->SetAttackTarget(
						targetPos
					);

					if (player_->GetCombo() != 8)
					{
						player_->LookAtTarget(
							targetPos
						);
					}
				}
			}
		}
		else
		{
			EnemyBase* nearestEnemy = nullptr;
			float nearestDistance = 300.0f;

			for (auto& enemy : enemies_)
			{
				if (!enemy)
				{
					continue;

				}

				if (enemy->IsDead())
				{
					continue;
				}

				VECTOR toEnemy =
					VSub(
						enemy->GetTransform().pos,
						player_->GetTransform().pos
					);

				float distance =
					VSize(toEnemy);

				if (distance < nearestDistance)
				{
					nearestDistance = distance;
					nearestEnemy = enemy.get();
				}
			}

			if (nearestEnemy != nullptr)
			{
				player_->SetAttackTarget(
					nearestEnemy->GetTransform().pos
				);
			}
			else
			{
				player_->ClearAttackTarget();
			}
		}

		player_->Update();

		// バニッシュアタック: ガード中に×で、敵の斜め後ろへ瞬間移動し、少し間をおいて叩き落とす
		{
			static EnemyBase* vanishTarget = nullptr;
			static float vanishTimer = 0.0f;
			static VECTOR vanishDir = { 0.0f, 0.0f, 1.0f };

			if (player_->ConsumeVanish())
			{
				auto* target = lockOnTarget_;

				// ロックオンしていなければ、一番近い敵
				if (target == nullptr || target->IsDead())
				{
					target = nullptr;
					float best = PlayerConfig::Vanish::MAX_RANGE;

					for (auto& enemy : enemies_)
					{
						if (!enemy || enemy->IsDead())
						{
							continue;
						}

						const float d = VSize(VSub(enemy->GetTransform().pos, player_->GetTransform().pos));

						if (d < best)
						{
							best = d;
							target = enemy.get();
						}
					}
				}

				if (target != nullptr)
				{
					const VECTOR enemyPos = target->GetTransform().pos;

					// 敵の向きの後ろ側(水平)。少し横にずらして、カメラが大きく回らないようにする
					VECTOR back = target->GetTransform().quaRot.GetForward();
					back.y = 0.0f;

					if (VSize(back) > 0.001f)
					{
						back = VNorm(back);
					}
					else
					{
						back = VGet(0.0f, 0.0f, 1.0f);
					}

					// 真後ろから左右どちらかへずらした2つの候補のうち、今のプレイヤーに近い方を選ぶ
					const float rad = PlayerConfig::Vanish::SIDE_ANGLE_DEG * 3.14159265f / 180.0f;
					const VECTOR playerNow = player_->GetTransform().pos;
					VECTOR warpPos = enemyPos;
					float bestDist = 1.0e9f;

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

						const float d = VSize(VSub(cand, playerNow));

						if (d < bestDist)
						{
							bestDist = d;
							warpPos = cand;
						}
					}

					player_->VanishTo(warpPos, enemyPos);

					vanishTarget = target;
					vanishTimer = PlayerConfig::Vanish::HIT_DELAY;
					vanishDir = back;
				}
			}

			// 少し間をおいてから叩き落とす(消えて、現れて、叩く、が目で追えるように)
			if (vanishTarget != nullptr)
			{
				vanishTimer -= SceneManager::GetInstance().GetDeltaTime();

				if (vanishTarget->IsDead())
				{
					vanishTarget = nullptr;
				}
				else if (vanishTimer <= 0.0f)
				{
					EnemyBase* target = vanishTarget;
					vanishTarget = nullptr;

					const VECTOR enemyPos = target->GetTransform().pos;
					const VECTOR back = vanishDir;

					const int dmg =
						player_->GetAttackDamage(Player::ATTACK_DAMAGE) * PlayerConfig::Vanish::DAMAGE_RATE;

					target->Damage(dmg);
					target->AddStun(PlayerConfig::Vanish::STUN);
					player_->AddDamagePopup(VAdd(enemyPos, VGet(0.0f, 130.0f, 0.0f)), dmg);

					MeleeEnemy* meleeEnemy = dynamic_cast<MeleeEnemy*>(target);

					if (meleeEnemy != nullptr)
					{
						meleeEnemy->LookAtPlayer();
						meleeEnemy->StartSlamDown();
					}

					const VECTOR slamDir = VNorm(VGet(back.x, -0.8f, back.z));
					target->AddKnockBack(slamDir, PlayerConfig::Vanish::KNOCK);

					EffekseerEffect::GetInstance()->
						PlayHitEffect(VAdd(enemyPos, VGet(0.0f, 80.0f, 0.0f)), 0.0f);

					isHitStop_ = true;
					hitStopTimer_ = PlayerConfig::Vanish::HIT_STOP;
					mainCamera.StartShake(PlayerConfig::Vanish::SHAKE_TIME, PlayerConfig::Vanish::SHAKE_POWER);

					// (追撃はブレイクしたときだけ)
				}
			}
		}

		// 追撃: ふっ飛ばした敵の元へ瞬間移動して、追い打ちする
		{
			static EnemyBase* pursuitTarget = nullptr;
			static float pursuitTimer = 0.0f;
			static VECTOR pursuitDir = { 0.0f, 0.0f, 1.0f };

			if (player_->ConsumePursuit())
			{
				auto* target = lockOnTarget_;

				if (target == nullptr || target->IsDead())
				{
					target = nullptr;
					float best = FLT_MAX;

					for (auto& enemy : enemies_)
					{
						if (!enemy || enemy->IsDead())
						{
							continue;
						}

						const float d = VSize(VSub(enemy->GetTransform().pos, player_->GetTransform().pos));

						if (d < best)
						{
							best = d;
							target = enemy.get();
						}
					}
				}

				if (target != nullptr)
				{
					// 瞬間移動はせず、敵のところまで速く飛んでいく(下で毎コマ動かす)
					pursuitTarget = target;
					pursuitTimer = PlayerConfig::Pursuit::HIT_DELAY;
					pursuitDir = VGet(0.0f, 0.0f, 1.0f);
				}
			}

			if (pursuitTarget != nullptr)
			{
				pursuitTimer -= SceneManager::GetInstance().GetDeltaTime();

				if (pursuitTarget->IsDead())
				{
					pursuitTarget = nullptr;
				}
				else
				{
					// 敵の手前まで、決まった速さで近づく
					const VECTOR enemyNow = pursuitTarget->GetTransform().pos;
					const VECTOR playerNow = player_->GetTransform().pos;

					VECTOR flat = VSub(enemyNow, playerNow);
					flat.y = 0.0f;
					flat = (VSize(flat) > 0.001f) ? VNorm(flat) : pursuitDir;
					pursuitDir = flat;

					VECTOR stop = VSub(enemyNow, VScale(flat, PlayerConfig::Pursuit::DISTANCE));
					stop.y = enemyNow.y + PlayerConfig::Pursuit::HEIGHT;

					if (stop.y < PlayerConfig::Pursuit::MIN_HEIGHT)
					{
						stop.y = PlayerConfig::Pursuit::MIN_HEIGHT;
					}

					const VECTOR toStop = VSub(stop, playerNow);
					const float remain = VSize(toStop);
					const float step = PlayerConfig::Pursuit::DASH_SPEED * SceneManager::GetInstance().GetDeltaTime();

					const bool arrived = (remain <= step) || (pursuitTimer <= 0.0f);

					if (arrived)
					{
						player_->DashStepTo(stop, enemyNow);
					}
					else
					{
						player_->DashStepTo(VAdd(playerNow, VScale(toStop, step / remain)), enemyNow);
					}

					if (arrived)
					{
						EnemyBase* target = pursuitTarget;
						pursuitTarget = nullptr;

						const VECTOR enemyPos = target->GetTransform().pos;
						const VECTOR dir = pursuitDir;

						const int dmg =
							player_->GetAttackDamage(Player::ATTACK_DAMAGE) * PlayerConfig::Pursuit::DAMAGE_RATE;

						target->Damage(dmg);
						target->AddStun(PlayerConfig::Pursuit::STUN);
						player_->AddDamagePopup(VAdd(enemyPos, VGet(0.0f, 130.0f, 0.0f)), dmg);

						MeleeEnemy* meleeEnemy = dynamic_cast<MeleeEnemy*>(target);

						if (meleeEnemy != nullptr)
						{
							meleeEnemy->LookAtPlayer();
						}

						// また遠くへふっ飛ばす
						target->AddKnockBack(VNorm(VGet(dir.x, 0.35f, dir.z)), PlayerConfig::Pursuit::KNOCK);

						EffekseerEffect::GetInstance()->
							PlayHitEffect(VAdd(enemyPos, VGet(0.0f, 80.0f, 0.0f)), 0.0f);

						isHitStop_ = true;
						hitStopTimer_ = PlayerConfig::Pursuit::HIT_STOP;
						mainCamera.StartShake(PlayerConfig::Pursuit::SHAKE_TIME, PlayerConfig::Pursuit::SHAKE_POWER);

						// 追撃はブレイク後の最初の一発だけ(続けての追撃はなし)
					}
				}
			}
		}

		// 相殺勝負: かめはめ波が敵の大きな気弾・ビームにぶつかったら、連打で押し合う
		if (clash_->Update(dt, *player_, enemies_) == ClashSystem::RESULT::WIN)
		{
			// 押し勝った瞬間に、少し止める
			isHitStop_ = true;
			hitStopTimer_ = ClashSystem::WIN_HIT_STOP;
		}

		// ガードバースト
		if (player_->IsGuardBurstTrigger())
		{
			VECTOR playerPos =
				player_->GetTransform().pos;

			// バースト時のカメラ揺れ
			mainCamera.StartShake(
				0.12f,
				6.0f
			);

			for (auto& enemy : enemies_)
			{
				if (!enemy)
				{
					continue;
				}

				if (enemy->IsDead())
				{
					continue;
				}

				VECTOR enemyPos =
					enemy->GetTransform().pos;

				VECTOR dir =
					VSub(
						enemyPos,
						playerPos
					);

				float distance =
					VSize(dir);

				// バーストの範囲
				if (distance > 250.0f)
				{
					continue;
				}

				if (distance <= 0.001f)
				{
					continue;
				}

				dir =
					VNorm(dir);

				MeleeEnemy* meleeEnemy =
					dynamic_cast<MeleeEnemy*>(
						enemy.get()
						);

				if (meleeEnemy != nullptr)
				{
					meleeEnemy->GuardBurst();
				}

				enemy->AddKnockBack(
					dir,
					40.0f
				);
			}
		}
		// かめはめ波中もカメラは切り替えない(ロックオン / 追従のまま)
		wasKamehame_ = player_->IsKamehame();
		//// かめはめ波カメラ
		//bool isKamehame =
		//	player_->IsKamehame();

		//bool isKamehameBeam =
		//	player_->IsKamehameBeam();

		////if (isKamehame &&
		////	!wasKamehame_)
		////{
		////	mainCamera.ChangeMode(
		////		Camera::MODE::KAMEHAME
		////	);
		////}

		////if (isKamehameBeam &&
		////	!wasKamehameBeam_)
		////{
		////	mainCamera.ChangeMode(
		////		Camera::MODE::KAMEHAME_SHOT
		////	);
		////}

		//if (!isKamehame &&
		//	wasKamehame_)
		//{
		//	if (isLockOn_ &&
		//		lockOnTarget_ != nullptr)
		//	{
		//		mainCamera.SetLockOnTarget(
		//			&lockOnTarget_->GetTransform()
		//		);

		//		mainCamera.ChangeMode(
		//			Camera::MODE::LOCK_ON
		//		);
		//	}
		//	else
		//	{
		//		mainCamera.ChangeMode(
		//			Camera::MODE::FOLLOW
		//		);
		//	}
		//}

		//wasKamehame_ =
		//	isKamehame;

		// ジャスト回避が成立したら、しばらく敵だけをスローモーションにする
		//   (敵を数フレームに1回だけ更新する。プレイヤーは普通に動けるので、反撃できる)
		static float justSlowTimer = 0.0f;
		static int justSlowFrame = 0;

		if (player_->ConsumeJustDodge())
		{
			justSlowTimer = PlayerConfig::JustDodge::SLOW_TIME;
			justSlowFrame = 0;
		}

		// ブレイク(スタン)した瞬間: カカロットのように全体を止めて演出を見せる
		{
			static std::unordered_map<EnemyBase*, bool> prevStun;

			for (auto& enemy : enemies_)
			{
				if (!enemy || enemy->IsDead())
				{
					continue;
				}

				const bool now = enemy->IsStunned();
				bool& prev = prevStun[enemy.get()];

				if (now && !prev)
				{
					// 全体を止めて、ブレイク演出(自前の時計)を再生する
					g_breakFreeze = PlayerConfig::Break::FREEZE_TIME;

					const VECTOR hitPos = VAdd(enemy->GetTransform().pos, VGet(0.0f, 80.0f, 0.0f));
					BreakEffect::Play(hitPos, PlayerConfig::Break::FREEZE_TIME);
					EffekseerEffect::GetInstance()->PlayHitEffect(hitPos, 0.0f);
				}

				prev = now;
			}
		}

		// ヒートアップ発動の演出カメラ(光の柱 → はじける → 拳と顔のアップ → 引き)
		{
			namespace hu = PlayerConfig::HeatUp;

			static bool wasActivating = false;
			static bool shookBurst = false;
			static bool shookClose = false;

			const bool activating = player_->IsHeatActivating();

			if (activating)
			{
				if (!wasActivating)
				{
					shookBurst = false;
					shookClose = false;

					// 発動の瞬間に、敵の飛び道具・技を消す(止まった弾が残らないように)
					for (auto& e : enemies_)
					{
						if (!e || e->IsDead())
						{
							continue;
						}

						MeleeEnemy* me = dynamic_cast<MeleeEnemy*>(e.get());
						if (me != nullptr)
						{
							me->CancelAttacksForHeatUp();
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
						// 引いて、金色をまとった全身を見せる(カット → 少しずつ引く)
						{ hu::ACTIVATE_CLOSE_END, {  40.0f, 110.0f, 180.0f }, { 0.0f, 100.0f, 0.0f }, 55.0f, true },
						{ hu::ACTIVATE_TIME,      {  40.0f, 110.0f, 340.0f }, { 0.0f, 100.0f, 0.0f }, 60.0f, false },
					};

					mainCamera.StartCinematic(keys);
				}

				mainCamera.UpdateCinematicPivot(player_->GetTransform().pos, player_->GetForward());

				const float t = player_->GetHeatActivateElapsed();

				if (!shookBurst && t >= hu::ACTIVATE_FLASH)
				{
					shookBurst = true;
					mainCamera.StartShake(0.4f, 7.0f);
				}

				if (!shookClose && t >= hu::ACTIVATE_BURST_END)
				{
					shookClose = true;
					mainCamera.StartShake(0.7f, 3.0f);
				}
			}
			else if (wasActivating)
			{
				mainCamera.EndCinematic();
			}

			wasActivating = activating;
		}

		bool updateEnemy = true;

		if (justSlowTimer > 0.0f)
		{
			justSlowTimer -= SceneManager::GetInstance().GetDeltaTime();
			justSlowFrame++;

			updateEnemy = (justSlowFrame % PlayerConfig::JustDodge::SLOW_DIV) == 0;
		}

		// ヒートアップ発動の演出中は、敵も止まる
		if (player_->IsHeatActivating())
		{
			updateEnemy = false;
		}

		// Enemy更新
		for (auto& enemy : enemies_)
		{
			if (!enemy)
			{
				continue;
			}

			if (enemy->IsDead())
			{
				continue;
			}

			if (updateEnemy)
			{
				enemy->Update();
			}
		}

		// プレイヤーと敵が重ならないように押し出す
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

			// 敵 → プレイヤー の水平方向
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

				dir = (VSize(forward) > 0.001f)
					? VScale(VNorm(forward), -1.0f)
					: VGet(0.0f, 0.0f, -1.0f);
			}

			float overlap = BODY_DISTANCE - dist;

			if (overlap > BODY_MAX_PUSH)
			{
				overlap = BODY_MAX_PUSH;
			}

			player_->PushOut(VScale(dir, overlap * BODY_PLAYER_RATE));
			enemy->PushOut(VScale(dir, -overlap * (1.0f - BODY_PLAYER_RATE)));
		}

		// Enemyの攻撃判定
		//for (auto& enemyBase : enemies_)
		//{
		//	if (!enemyBase)
		//	{
		//		continue;
		//	}

		//	if (enemyBase->IsDead())
		//	{
		//		continue;
		//	}

		//	MeleeEnemy* enemy =
		//		dynamic_cast<MeleeEnemy*>(
		//			enemyBase.get()
		//			);

		//	if (enemy == nullptr)
		//	{
		//		continue;
		//	}

		//	if (!enemy->IsAttackHitTiming())
		//	{
		//		continue;
		//	}

		//	if (enemy->HasAttackHit())
		//	{
		//		continue;
		//	}

		//	VECTOR enemyPos =
		//		enemy->GetTransform().pos;
		//	// Playerを狙っている
		//	VECTOR playerPos =
		//		player_->GetTransform().pos;

		//	VECTOR toPlayer =
		//		VSub(
		//			playerPos,
		//			enemyPos
		//		);

		//	float distance =
		//		VSize(toPlayer);

		//	if (distance <= ActorBase::ATTACK_RANGE)
		//	{
		//		if (!player_->IsDodging())
		//		{
		//			VECTOR hitDir =
		//				VSub(
		//					player_->GetTransform().pos,
		//					enemy->GetTransform().pos
		//				);

		//			if (VSize(hitDir) > 0.001f)
		//			{
		//				hitDir = VNorm(hitDir);

		//				if (player_->IsGuard())
		//				{
		//					// ガード耐久値を減らす
		//					if (enemy->GetAttackCombo() == 4)
		//					{
		//						player_->GuardDamage(80.0f);
		//					}
		//					else
		//					{
		//						player_->GuardDamage(20.0f);
		//					}

		//					// ガード中はダメージなしで少しだけ押される
		//					float guardKnockPower = 1.5f;

		//					if (enemy->GetAttackCombo() == 4)
		//					{
		//						guardKnockPower = 5.0f;
		//					}

		//					player_->AddKnockBack(
		//						hitDir,
		//						guardKnockPower
		//					);

		//					// ガードエフェクト
		//					VECTOR effectPos =
		//						player_->GetTransform().pos;

		//					effectPos.y += 80.0f;

		//					EffekseerEffect::GetInstance()->
		//						PlayHitEffect(
		//							effectPos,
		//							0.0f
		//						);

		//					// ガードした瞬間に少し止める
		//					isHitStop_ = true;
		//					hitStopTimer_ = 0.03f;
		//				}
		//				else
		//				{
		//					player_->LookAtDamageEnemy(
		//						enemy->GetTransform().pos
		//					);

		//					player_->Damage(
		//						MeleeEnemy::ATTACK_DAMAGE
		//					);

		//					float knockPower = 5.0f;

		//					if (enemy->GetAttackCombo() == 4)
		//					{
		//						knockPower = 30.0f;
		//					}

		//					player_->AddKnockBack(
		//						hitDir,
		//						knockPower
		//					);
		//				}
		//			}
		//		}
		//		enemy->SetAttackHit();
		//	}
		//}

		// Playerの攻撃判定
		if (player_->IsAttackHitTiming() &&
			!player_->HasAttackHit())
		{
			const int combo = player_->GetCombo();

			// 形態に応じたダメージ(変身すると倍率が変わる)
			const int attackDamage =
				player_->GetAttackDamage(Player::ATTACK_DAMAGE);

			// 攻撃範囲(水平距離)と高さの許容範囲
			float attackRange = ActorBase::ATTACK_RANGE;
			float attackHeight = ATTACK_HEIGHT_RANGE;

			// 8段目は上から叩き落とすので広め
			if (combo == 8)
			{
				attackRange = 280.0f;
				attackHeight = 300.0f;
			}

			const VECTOR playerPos =
				player_->GetTransform().pos;

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

				const VECTOR enemyPos =
					enemy->GetTransform().pos;

				const VECTOR toEnemy =
					VSub(enemyPos, playerPos);

				// 水平距離と高さの差を別々に見る
				// (3D距離だと、敵が上下にいるだけで届かなくなるため)
				VECTOR horizontal = toEnemy;
				horizontal.y = 0.0f;

				const float horizontalDist = VSize(horizontal);
				const float verticalDist = std::fabs(toEnemy.y);

				if (horizontalDist > attackRange ||
					verticalDist > attackHeight)
				{
					continue;
				}

				// 向き判定(真上・真下など水平距離がほぼ0のときは判定しない)
				if (hasForward &&
					horizontalDist > ATTACK_DOT_MIN_DISTANCE)
				{
					const float dot =
						VDot(forward, VScale(horizontal, 1.0f / horizontalDist));

					if (dot < ActorBase::ATTACK_DOT)
					{
						continue;
					}
				}

				MeleeEnemy* meleeEnemy =
					dynamic_cast<MeleeEnemy*>(enemy.get());

				// 敵がガード中(8段目はガードを崩すので貫通)
				if (meleeEnemy != nullptr &&
					meleeEnemy->IsGuard() &&
					combo != 8)
				{
					// ガード耐久値を減らす(攻撃力が高いほど削れる)
					meleeEnemy->GuardDamage(
						20.0f * player_->GetAttackRate()
					);

					VECTOR effectPos = enemyPos;
					effectPos.y += 80.0f;

					EffekseerEffect::GetInstance()->
						PlayHitEffect(effectPos, 0.0f);

					isHitStop_ = true;
					hitStopTimer_ = 0.03f;

					player_->SetAttackHit();

					break;
				}

				// HIT
				if (meleeEnemy != nullptr)
				{
					meleeEnemy->LookAtPlayer();
				}

				enemy->Damage(attackDamage);
				enemy->AddStun(combo == 8 ? 40.0f : 8.0f);	// フィニッシュは大きくスタン値を溜める
				player_->AddDamagePopup(VAdd(enemyPos, VGet(0.0f, 130.0f, 0.0f)), attackDamage);

				float knockPower = 0.0f;

				switch (combo)
				{
				case 1: knockPower = 5.0f;   break;
				case 2: knockPower = 7.0f;   break;
				case 3: knockPower = 5.0f;   break;
				case 4: knockPower = 7.0f;   break;
				case 5: knockPower = 9.0f;   break;
				case 6: knockPower = 7.0f;   break;
				case 7: knockPower = 4.0f;   break;
				case 8: knockPower = 100.0f; break;
				}

				VECTOR hitDir = toEnemy;
				if (VSize(hitDir) > 0.001f)
				{
					hitDir = VNorm(hitDir);
				}

				// 8段目は下に叩き落とす
				if (combo == 8)
				{
					VECTOR slamDir = { forward.x, -0.8f, forward.z };
					hitDir = VNorm(slamDir);

					if (meleeEnemy != nullptr)
					{
						meleeEnemy->StartSlamDown();
					}

					// 8段目が当たったら高速追撃できる
					player_->SetCanChase(true);
					// (追撃はブレイクしたときだけ)	// ふっ飛ばしたので、追撃できる
				}

				enemy->AddKnockBack(hitDir, knockPower);

				VECTOR effectPos = enemyPos;
				effectPos.y += 80.0f;

				EffekseerEffect::GetInstance()->
					PlayHitEffect(effectPos, 0.0f);

				isHitStop_ = true;

				if (combo == 8)
				{
					hitStopTimer_ = 0.10f;
					mainCamera.StartShake(0.15f, 8.0f);
				}
				else
				{
					hitStopTimer_ = 0.05f;
					mainCamera.StartShake(0.08f, 3.0f);
				}

				player_->SetAttackHit();

				// 1攻撃で1体だけ
				break;
			}
		}

		// 気弾の攻撃判定
		for (auto& blast : player_->GetKiBlasts())
		{
			if (blast->IsDead())
			{
				continue;
			}

			VECTOR blastPos =
				blast->GetPos();

			for (auto& enemy : enemies_)
			{
				if (!enemy)
				{
					continue;
				}

				if (enemy->IsDead())
				{
					continue;
				}

				VECTOR enemyPos =
					enemy->GetTransform().pos;

				enemyPos.y += 70.0f;

				float distance =
					VSize(
						VSub(
							blastPos,
							enemyPos
						)
					);

				if (distance > 45.0f)
				{
					continue;
				}

				{
					const int blastDamage = player_->GetAttackDamage(1);
					enemy->Damage(blastDamage);
					enemy->AddStun(10.0f);
					player_->AddDamagePopup(VAdd(enemyPos, VGet(0.0f, 40.0f, 0.0f)), blastDamage);
				}

				blast->Hit();

				break;
			}
		}

		// かめはめ波ダメージタイマー
		// フィニッシュ気弾
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

				const VECTOR enemyPos = enemy->GetTransform().pos;
				const VECTOR center = VAdd(enemyPos, VGet(0.0f, 70.0f, 0.0f));

				if (VSize(VSub(finish->GetPos(), center)) > finish->GetRadius() + 45.0f)
				{
					continue;
				}

				const int dmg = player_->GetAttackDamage(PlayerConfig::Finish::DAMAGE);

				enemy->Damage(dmg);
				enemy->AddStun(PlayerConfig::Finish::STUN);
				player_->AddDamagePopup(VAdd(enemyPos, VGet(0.0f, 130.0f, 0.0f)), dmg);

				enemy->AddKnockBack(finish->GetDir(), PlayerConfig::Finish::KNOCK);

				EffekseerEffect::GetInstance()->PlayHitEffect(center, 0.0f);

				isHitStop_ = true;
				hitStopTimer_ = 0.08f;
				mainCamera.StartShake(0.12f, 6.0f);

				finish->Kill();

				// ふっ飛ばしたので、追撃できる
				// (追撃はブレイクしたときだけ)
				break;
			}
		}

		if (player_->IsKamehameBeam())
		{
			kamehameDamageTimer_ +=
				SceneManager::GetInstance().GetDeltaTime();
		}
		else
		{
			kamehameDamageTimer_ = 0.0f;

			// かめはめ波が当たったまま終わったら、追撃できる
			if (g_kamehameHit)
			{
				g_kamehameHit = false;
				// (追撃はブレイクしたときだけ)
			}

			for (auto& enemy : enemies_)
			{
				if (!enemy)
				{
					continue;
				}

				MeleeEnemy* meleeEnemy =
					dynamic_cast<MeleeEnemy*>(
						enemy.get()
						);

				if (meleeEnemy != nullptr)
				{
					meleeEnemy->SetKamehameHit(false);
				}
			}
		}

		// かめはめ波の攻撃判定
		if (player_->IsKamehameBeam() && !clash_->IsActive())
		{
			VECTOR beamStart =
				player_->GetKamehameStartPos();

			VECTOR beamEnd =
				player_->GetKamehameEndPos();

			VECTOR beamVec =
				VSub(
					beamEnd,
					beamStart
				);

			float beamLengthSq =
				VDot(
					beamVec,
					beamVec
				);

			if (beamLengthSq > 0.001f)
			{
				for (auto& enemy : enemies_)
				{
					if (!enemy)
					{
						continue;
					}

					if (enemy->IsDead())
					{
						continue;
					}

					MeleeEnemy* meleeEnemy =
						dynamic_cast<MeleeEnemy*>(
							enemy.get()
							);

					// 最初は「ビームに当たっていない」にする
					if (meleeEnemy != nullptr)
					{
						meleeEnemy->SetKamehameHit(false);
					}

					VECTOR enemyPos =
						enemy->GetTransform().pos;

					// 敵の中心
					enemyPos.y += 70.0f;

					VECTOR startToEnemy =
						VSub(
							enemyPos,
							beamStart
						);

					float t =
						VDot(
							startToEnemy,
							beamVec
						) /
						beamLengthSq;

					if (t < 0.0f)
					{
						t = 0.0f;
					}

					if (t > 1.0f)
					{
						t = 1.0f;
					}

					VECTOR closestPos =
						VAdd(
							beamStart,
							VScale(
								beamVec,
								t
							)
						);

					VECTOR diff =
						VSub(
							enemyPos,
							closestPos
						);

					float distance =
						VSize(diff);

					static constexpr float ENEMY_HIT_RADIUS =
						45.0f;

					float hitRadius =
						player_->GetKamehameRadius() +
						ENEMY_HIT_RADIUS;

					// ビームに当たっていない
					if (distance > hitRadius)
					{
						continue;
					}

					// ビームに当たっている
					if (meleeEnemy != nullptr)
					{
						VECTOR pushDir =
							player_->GetForward();

						if (VSize(pushDir) > 0.001f)
						{
							pushDir =
								VNorm(pushDir);
						}

						meleeEnemy->SetKamehameHit(
							true,
							pushDir
						);
					}

					// ダメージだけ0.15秒ごと
					if (kamehameDamageTimer_ >= 0.15f)
					{
						{
							const int beamDamage = player_->GetAttackDamage(3);
							enemy->Damage(beamDamage);
							enemy->AddStun(4.0f);
							player_->AddDamagePopup(VAdd(enemyPos, VGet(0.0f, 60.0f, 0.0f)), beamDamage);
							g_kamehameHit = true;
						}

						kamehameDamageTimer_ = 0.0f;
					}
				}
			}
		}
		break;
	}
	case GAME_STATE::CLEAR:

		UpdateClear(dt);

		break;

	default:
		break;
	}
}

void GameScene::Draw(void)
{


	// 背景
	skyDome_->Draw();
	stage_->Draw();

	player_->Draw();

	for (auto& enemy : enemies_)
	{
		if (!enemy)
		{
			continue;
		}

		if (enemy->IsDead())
		{
			continue;
		}

		enemy->Draw();
	}

	// 相殺勝負: ぶつかっている光
	clash_->Draw(*player_);

	// ステージクリア演出
	if (gameState_ == GAME_STATE::CLEAR)
	{
		DrawClear();
	}

	int mainScreen = SceneManager::GetInstance().GetMainScreen();

	// ポストエフェクト(モノクロ)
	//-----------------------------------------
	if (mode_ < MODE::MONO) { return; }

	SetDrawScreen(postEffectScreen_);

	// 画面を初期化
	ClearDrawScreen();

	monoRenderer_->Draw();

	// メインに戻す
	SetDrawScreen(mainScreen);
	DrawGraph(0, 0, postEffectScreen_, false);
	//-----------------------------------------

	// ポストエフェクト(走査線)
	//-----------------------------------------
	if (mode_ < MODE::SCAN) { return; }

	SetDrawScreen(postEffectScreen_);

	// 画面を初期化
	ClearDrawScreen();

	scanMaterial_->SetConstBuf(
		1,
		{ SceneManager::GetInstance().GetTotalTime(), 0.0f, 0.0f, 0.0f }
	);
	scanRenderer_->Draw();

	// メインに戻す
	SetDrawScreen(mainScreen);
	DrawGraph(0, 0, postEffectScreen_, false);
	//-----------------------------------------


	// ポストエフェクト(レンズの歪み)
	//-----------------------------------------
	if (mode_ < MODE::LENS) { return; }

	SetDrawScreen(postEffectScreen_);

	// 画面を初期化
	ClearDrawScreen();

	lensRenderer_->Draw();

	// メインに戻す
	SetDrawScreen(mainScreen);
	DrawGraph(0, 0, postEffectScreen_, false);
	//-----------------------------------------


	// ポストエフェクト(ビネット)
	//-----------------------------------------
	if (mode_ < MODE::VINE) { return; }

	SetDrawScreen(postEffectScreen_);

	// 画面を初期化
	ClearDrawScreen();

	vineRenderer_->Draw();

	// メインに戻す
	SetDrawScreen(mainScreen);
	DrawGraph(0, 0, postEffectScreen_, false);
	//-----------------------------------------
}

void GameScene::MakeEnemies(void)
{
	std::unique_ptr<MeleeEnemy> enemy =
		std::make_unique<MeleeEnemy>(
			*player_
		);

	enemy->Init();

	enemies_.push_back(
		std::move(enemy)
	);
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
		SceneManager::GetInstance().ChangeScene(
			SceneManager::SCENE_ID::TITLE
		);
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

	// STAGE CLEAR 拡大演出 (0.5倍 → 1.0倍)
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
			GetColor(255, 255, 255)
		);
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
// ポーズ / ゲームオーバー
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
		SceneManager::GetInstance().ChangeScene(
			SceneManager::SCENE_ID::GAME_OVER
		);
	}
}

void GameScene::StartClear(void)
{
	gameState_ = GAME_STATE::CLEAR;
	PlayRecord::ClearTime() = playTime_;

	clearTimer_ = 0.0f;
	isResultRequested_ = false;
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