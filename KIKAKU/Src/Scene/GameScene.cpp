#include <DxLib.h>
#include "../Application.h"
#include "../Utility/AsoUtility.h"
#include "../Manager/SceneManager.h"
#include "../Manager/Camera.h"
#include "../Manager/InputManager.h"
#include "../Manager/EffekseerEffect.h"
#include "../Object/Common/Capsule.h"
#include "../Object/Common/Collider.h"
#include "../Object/SkyDome.h"
#include "../Object/Stage.h"
#include "../Object/Player.h"
#include "../Object/Enemy/EnemyBase.h"
#include "../Object/Enemy/MeleeEnemy.h"
#include "../Object/GhostPlayer.h"
#include "../Object/Planet.h"
#include "../Object/FloorSwitch.h"
#include "../Object/Door.h"
#include "../Object/Goal.h"
#include "../Renderer/PixelMaterial.h"
#include "../Renderer/PixelRenderer.h"
#include "../Renderer/ModelMaterial.h"
#include "../Renderer/ModelRenderer.h"
#include "../Controller/TimeStopController.h"
#include "GameScene.h"
#include <cmath>

GameScene::GameScene(void)
{
	player_ = nullptr;
	ghostPlayer_ = nullptr;
	skyDome_ = nullptr;
	stage_ = nullptr;
	floorSwitch_ = nullptr;
	floorSwitch1_ = nullptr;
	floorSwitch2_ = nullptr;
	door_ = nullptr;
	goal_ = nullptr;
	door1_ = nullptr;
	door2_ = nullptr;
	kamehameDamageTimer_ = 0.0f;
	wasKamehame_ = false;
	wasKamehameBeam_ = false;
	shiftState_ = SHIFT_STATE::IDLE;
	gameState_ = GAME_STATE::PLAY;
	clearTimer_ = 0.0f;
	stageNo_ = STAGE_NO::STAGE_1;
	attackUIHandle_ = -1;
	isHitStop_ = false;
	hitStopTimer_ = 0.0f;
}

GameScene::~GameScene(void)
{
	DeleteGraph(postEffectScreen_);

	if (drawImage != -1)
	{
		DeleteGraph(drawImage);
		drawImage = -1;
	}
}

void GameScene::Init(void)
{

	// プレイヤー
	player_ = std::make_unique<Player>();
	player_->Init();

	// ステージ
	stage_ = std::make_unique<Stage>(*player_);
	stage_->Init();

	// ステージの初期設定
	stage_->ChangeStage(Stage::NAME::MAIN_PLANET);

	// 過去のプレイヤー
	ghostPlayer_ = std::make_unique<GhostPlayer>();
	ghostPlayer_->Init();

	// スカイドーム
	skyDome_ = std::make_unique<SkyDome>(player_->GetTransform());
	skyDome_->Init();

	timeStop_ =
		std::make_unique<TimeStopController>();

	timeStop_->Init();

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
	
	shiftIdleImage_ = LoadGraph("Data/Image/blue.png");
	shiftRecordingImage_ = LoadGraph("Data/Image/red.png");
	shiftPlayingImage_ = LoadGraph("Data/Image/purple.png");

	// 頂点シェーダー用

	// 初期モード
	mode_ = MODE::MAIN;

	MakeStage3();

	gameState_ = GAME_STATE::PLAY;
	shiftState_ = SHIFT_STATE::IDLE;
	clearTimer_ = 0.0f;
	stageStartTimer_ = 0.0f;

	attackUIHandle_ =
		LoadGraph(
			"Data/Image/AttackUI.png"
		);
}

void GameScene::Update(void)
{
	InputManager& ins = InputManager::GetInstance();

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

		if (stageStartTimer_ < STAGE_START_UI_TIME)
		{
			stageStartTimer_ +=
				SceneManager::GetInstance().GetDeltaTime();
		}
		
		// SHIFT処理
		if (ins.IsTrgDown(KEY_INPUT_LSHIFT))
		{
			switch (shiftState_)
			{
			case SHIFT_STATE::IDLE:

				// 1回目のSHIFT
				// 記録開始
				player_->StartRecord();

				shiftState_ = SHIFT_STATE::RECORDING;

				break;


			case SHIFT_STATE::RECORDING:

				// 2回目のSHIFT
				// 記録終了
				player_->StopRecord();

				// Ghost再生開始
				ghostPlayer_->Start(
					player_->GetRecords()
				);

				shiftState_ = SHIFT_STATE::PLAYING;

				break;


			case SHIFT_STATE::PLAYING:

				// Ghost再生中はSHIFTを使えない
				break;
			}
		}

		// ゲーム更新
		skyDome_->Update();

		stage_->Update();

		player_->ClearAttackTarget();

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

			toEnemy.y = 0.0f;

			float distance =
				VSize(toEnemy);

			if (distance < nearestDistance)
			{
				nearestDistance = distance;

				player_->SetAttackTarget(
					enemy->GetTransform().pos
				);
			}
		}

		player_->Update();

		// かめはめ波カメラ
		bool isKamehame =
			player_->IsKamehame();

		bool isKamehameBeam =
			player_->IsKamehameBeam();

		if (isKamehame &&
			!wasKamehame_)
		{
			mainCamera.ChangeMode(
				Camera::MODE::KAMEHAME
			);
		}

		if (isKamehameBeam &&
			!wasKamehameBeam_)
		{
			mainCamera.ChangeMode(
				Camera::MODE::KAMEHAME_SHOT
			);
		}

		if (!isKamehame &&
			wasKamehame_)
		{
			mainCamera.ChangeMode(
				Camera::MODE::FOLLOW
			);
		}

		wasKamehame_ =
			isKamehame;

		if (!timeStop_->IsStopping())
		{
			ghostPlayer_->Update();
			ghostPlayer_->UpdateAutoAttack(
				enemies_
			);
		}

		timeStop_->Update();

		// RECORDING状態の間は世界を停止
		if (timeStop_->IsJustFinished())
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

				// 蓄積ダメージ取得
				int damage =
					enemy->GetPendingDamage();

				// ダメージが無ければ何もしない
				if (damage <= 0)
				{
					continue;
				}

				// ノックバック方向
				VECTOR playerPos =
					player_->GetTransform().pos;

				VECTOR enemyPos =
					enemy->GetTransform().pos;

				VECTOR knockDir =
					VSub(
						enemyPos,
						playerPos
					);

				knockDir.y = 0.0f;

				// ダメージ量でノックバックを強くする
				float knockPower =
					2.0f +
					static_cast<float>(damage) * 2.0f;

				// 吹っ飛びすぎ防止
				const float MAX_KNOCK_POWER = 12.0f;

				if (knockPower > MAX_KNOCK_POWER)
				{
					knockPower =
						MAX_KNOCK_POWER;
				}

				// 蓄積ダメージ発動！
				enemy->ApplyPendingDamage();

				// ノックバック
				enemy->AddKnockBack(
					knockDir,
					knockPower
				);
			}
		}

		// Enemy更新
		if (!timeStop_->IsStopping())
		{
			for (auto& enemy : enemies_)
			{
				if (!enemy)
				{
					continue;
				}

				enemy->Update();
			}
		}

		// Enemyの攻撃判定
		if (!timeStop_->IsStopping())
		{
			for (auto& enemyBase : enemies_)
			{
				if (!enemyBase)
				{
					continue;
				}

				if (enemyBase->IsDead())
				{
					continue;
				}

				MeleeEnemy* enemy =
					dynamic_cast<MeleeEnemy*>(
						enemyBase.get()
						);

				if (enemy == nullptr)
				{
					continue;
				}

				if (!enemy->IsAttackHitTiming())
				{
					continue;
				}

				if (enemy->HasAttackHit())
				{
					continue;
				}

				VECTOR enemyPos =
					enemy->GetTransform().pos;

				// Ghostを狙っている
				if (enemy->IsTargetGhost())
				{
					if (!ghostPlayer_->IsTargetable())
					{
						continue;
					}

					VECTOR ghostPos =
						ghostPlayer_->GetTransform().pos;

					VECTOR toGhost =
						VSub(
							ghostPos,
							enemyPos
						);

					float distance =
						VSize(toGhost);

					if (distance <= ActorBase::ATTACK_RANGE)
					{
						ghostPlayer_->Damage(
							MeleeEnemy::ATTACK_DAMAGE
						);

						enemy->SetAttackHit();
					}

					continue;
				}

				// Playerを狙っている
				VECTOR playerPos =
					player_->GetTransform().pos;

				VECTOR toPlayer =
					VSub(
						playerPos,
						enemyPos
					);

				float distance =
					VSize(toPlayer);

				if (distance <= ActorBase::ATTACK_RANGE)
				{
					player_->Damage(
						MeleeEnemy::ATTACK_DAMAGE
					);

					enemy->SetAttackHit();
				}
			}
		}

		// Playerの攻撃判定
		if (player_->IsAttackHitTiming() &&
			!player_->HasAttackHit())
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

				VECTOR playerPos =
					player_->GetTransform().pos;

				VECTOR enemyPos =
					enemy->GetTransform().pos;

				VECTOR toEnemy =
					VSub(
						enemyPos,
						playerPos
					);

				// 高さは無視
				toEnemy.y = 0.0f;

				float distance =
					VSize(toEnemy);

				// 距離判定
				if (distance > ActorBase::ATTACK_RANGE)
				{
					continue;
				}

				if (distance <= 0.001f)
				{
					continue;
				}

				// 向き判定
				toEnemy =
					VNorm(toEnemy);

				VECTOR forward =
					player_->GetForward();

				forward.y = 0.0f;

				if (VSize(forward) <= 0.001f)
				{
					continue;
				}

				forward =
					VNorm(forward);


				float dot =
					VDot(
						forward,
						toEnemy
					);

				if (dot < ActorBase::ATTACK_DOT)
				{
					continue;
				}

				// HIT
				if (timeStop_->IsStopping())
				{
					// 時間停止中は蓄積
					enemy->AddPendingDamage(
						Player::ATTACK_DAMAGE
					);
				}
				else
				{
					// SYNC受付中か
					if (enemy->IsSyncReady())
					{
						static constexpr int SYNC_BONUS_DAMAGE = 2;
						static constexpr float SYNC_KNOCKBACK = 8.0f;

						enemy->Damage(
							Player::ATTACK_DAMAGE +
							SYNC_BONUS_DAMAGE
						);

						enemy->AddKnockBack(
							toEnemy,
							SYNC_KNOCKBACK
						);

						// SYNC終了
						enemy->EndSyncWindow();
					}
					else
					{
						enemy->Damage(
							Player::ATTACK_DAMAGE
						);

						float knockPower = 3.0f;

						switch (player_->GetCombo())
						{
						case 1:
							knockPower = 8.0f;
							break;
						case 2:
							knockPower = 10.0f;
							break;
						case 3:
							knockPower = 8.0f;
							break;
						case 4:
							knockPower = 5.0f;
							break;
						case 5:
							knockPower = 5.5f;
							break;
						case 6:
							knockPower = 7.0f;
							break;
						case 7:
							knockPower = 7.0f;
							break;
						case 8:
							knockPower = 30.0f;
							player_->SetCanChase(true);
							break;
						}

						VECTOR hitDir =
							player_->GetForward();

						hitDir.y = 0.0f;

						enemy->AddKnockBack(
							hitDir,
							knockPower
						);

						VECTOR effectPos =
							enemy->GetTransform().pos;

						effectPos.y += 80.0f;

						EffekseerEffect::GetInstance()->PlayHitEffect(
							effectPos,
							0.0f
						);

						isHitStop_ = true;

						if (player_->GetCombo() == 4)
						{
							hitStopTimer_ = 0.10f;

							mainCamera.StartShake(
								0.15f,
								8.0f
							);
						}
						else
						{
							hitStopTimer_ = 0.05f;

							mainCamera.StartShake(
								0.08f,
								3.0f
							);
						}
					}
				}

				player_->SetAttackHit();
				// 1攻撃で1体だけ
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
		}
		
		// かめはめ波の攻撃判定
		if (player_->IsKamehameBeam() &&
			kamehameDamageTimer_ >= 0.15f)
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

					VECTOR enemyPos =
						enemy->GetTransform().pos;

					// 敵の中心を少し上にする
					enemyPos.y += 70.0f;

					// ビーム始点 → 敵
					VECTOR startToEnemy =
						VSub(
							enemyPos,
							beamStart
						);

					// 線分上のどの位置が敵に一番近いか
					float t =
						VDot(
							startToEnemy,
							beamVec
						) /
						beamLengthSq;

					// 0～1に制限
					if (t < 0.0f)
					{
						t = 0.0f;
					}

					if (t > 1.0f)
					{
						t = 1.0f;
					}

					// ビーム上で敵に最も近い位置
					VECTOR closestPos =
						VAdd(
							beamStart,
							VScale(
								beamVec,
								t
							)
						);

					// 敵とビームの距離
					VECTOR diff =
						VSub(
							enemyPos,
							closestPos
						);

					float distance =
						VSize(diff);

					// 敵側の当たり半径
					static constexpr float ENEMY_HIT_RADIUS =
						45.0f;

					float hitRadius =
						player_->GetKamehameRadius() +
						ENEMY_HIT_RADIUS;

					if (distance > hitRadius)
					{
						continue;
					}

					// HIT
					enemy->Damage(3);

					// ノックバック
					VECTOR knockDir =
						player_->GetForward();

					knockDir.y = 0.0f;

					if (VSize(knockDir) > 0.001f)
					{
						knockDir =
							VNorm(knockDir);

						enemy->AddKnockBack(
							knockDir,
							10.0f
						);
					}

					kamehameDamageTimer_ = 0.0f;
				}
			}
		}

		// Ghostの攻撃判定
		if (!timeStop_->IsStopping())
		{
			if (ghostPlayer_->IsActive() &&
				ghostPlayer_->IsAttackHitTiming() &&
				!ghostPlayer_->HasAttackHit())
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
					// Ghostの位置
					VECTOR ghostPos =
						ghostPlayer_->GetTransform().pos;
					// Enemyの位置
					VECTOR enemyPos =
						enemy->GetTransform().pos;
					// Ghost → Enemy
					VECTOR toEnemy =
						VSub(
							enemyPos,
							ghostPos
						);

					toEnemy.y = 0.0f;

					// 距離
					float distance =
						VSize(toEnemy);

					if (distance > ActorBase::ATTACK_RANGE)
					{
						continue;
					}
					// 方向
					if (distance <= 0.001f)
					{
						continue;
					}

					toEnemy =
						VNorm(toEnemy);

					VECTOR forward =
						ghostPlayer_->GetForward();

					forward.y = 0.0f;

					if (VSize(forward) <= 0.001f)
					{
						continue;
					}

					forward =
						VNorm(forward);

					float dot =
						VDot(
							forward,
							toEnemy
						);

					if (dot < ActorBase::ATTACK_DOT)
					{
						continue;
					}

					// HIT
					enemy->Damage(
						GhostPlayer::ATTACK_DAMAGE
					);

					enemy->StartSyncWindow();

					ghostPlayer_->SetAttackHit();

					break;
				}
			}
		}

		if (floorSwitch_)
		{
			floorSwitch_->Update();
		}

		if (floorSwitch1_)
		{
			floorSwitch1_->Update();
		}

		if (floorSwitch2_)
		{
			floorSwitch2_->Update();
		}

		if (door_)
		{
			door_->Update();
		}

		if (door1_)
		{
			door1_->Update();
		}

		if (door2_)
		{
			door2_->Update();
		}

		// Ghost再生終了
		if (shiftState_ == SHIFT_STATE::PLAYING)
		{
			if (!ghostPlayer_->IsPlaying())
			{
				shiftState_ = SHIFT_STATE::IDLE;
			}
		}

		// ゴール判定
		if (goal_)
		{
			goal_->Update();

			if (goal_->IsClear())
			{
				// 記録中なら終了
				if (shiftState_ == SHIFT_STATE::RECORDING)
				{
					player_->StopRecord();
				}

				shiftState_ = SHIFT_STATE::IDLE;
				clearTimer_ = 0.0f;

				// STAGE3ならゲームクリア
				if (stageNo_ == STAGE_NO::STAGE_3)
				{
					gameState_ = GAME_STATE::GAME_CLEAR;
				}
				else
				{
					gameState_ = GAME_STATE::CLEAR;
				}
			}
		}

		break;
	}
	case GAME_STATE::CLEAR:

	clearTimer_ +=
		SceneManager::GetInstance().GetDeltaTime();

	// 1秒後から入力受付
	if (clearTimer_ >= 1.0f)
	{
		if (ins.IsTrgDown(KEY_INPUT_SPACE))
		{
			// STAGE1 → STAGE2
			if (stageNo_ == STAGE_NO::STAGE_1)
			{
				ChangeGameStage(
					STAGE_NO::STAGE_2
				);
			}

			// STAGE2 → STAGE3
			else if (stageNo_ == STAGE_NO::STAGE_2)
			{
				ChangeGameStage(
					STAGE_NO::STAGE_3
				);
			}
		}
	}
		break;

	case GAME_STATE::GAME_CLEAR:

		// GAME CLEAR演出用に同じタイマーを使う
		clearTimer_ +=
			SceneManager::GetInstance().GetDeltaTime();

		// 2秒後からSPACE受付
		if (clearTimer_ >= 2.0f)
		{
			if (ins.IsTrgDown(KEY_INPUT_SPACE))
			{
				SceneManager::GetInstance().ChangeScene(
					SceneManager::SCENE_ID::TITLE
				);
			}
		}

		break;
	}
}

void GameScene::Draw(void)
{

	// 背景
	skyDome_->Draw();
	stage_->Draw();

	if (floorSwitch_)
	{
		floorSwitch_->Draw();
	}

	if (floorSwitch1_)
	{
		floorSwitch1_->Draw();
	}

	if (floorSwitch2_)
	{
		floorSwitch2_->Draw();
	}

	if (door_)
	{
		door_->Draw();
	}

	// STAGE3 Door1
	if (door1_)
	{
		door1_->Draw();
	}

	// STAGE3 Door2
	if (door2_)
	{
		door2_->Draw();
	}

	if (goal_)
	{
		goal_->Draw();
	}

	player_->Draw();

	ghostPlayer_->Draw();

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

	timeStop_->DrawEffect(
		postEffectScreen_
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

		// SYNC中じゃなければUIだけ描画しない
		if (!enemy->IsSyncReady())
		{
			continue;
		}

		VECTOR uiPos =
			enemy->GetTransform().pos;

		// 敵の頭上
		uiPos.y += 180.0f;

		VECTOR screenPos =
			ConvWorldPosToScreenPos(
				uiPos
			);

		if (screenPos.z < 0.0f ||
			screenPos.z > 1.0f)
		{
			continue;
		}

		float time =
			SceneManager::GetInstance().GetTotalTime();

		// 0.0 ～ 1.0
		float pulse =
			(sinf(time * 8.0f) + 1.0f) * 0.5f;

		// 0.22 ～ 0.26倍
		float scale =
			0.22f +
			pulse * 0.04f;

		DrawRotaGraph(
			static_cast<int>(screenPos.x),
			static_cast<int>(screenPos.y),
			scale,
			0.0,
			attackUIHandle_,
			true
		);
	}

	DrawFormatString(
		20,
		210,
		GetColor(255, 255, 255),
		"Player HP : %d",
		player_->GetHp()
	);

	if (ghostPlayer_->IsActive())
	{
		DrawFormatString(
			20,
			235,
			GetColor(150, 220, 255),
			"Ghost HP : %d",
			ghostPlayer_->GetHp()
		);
	}

	// SHIFT UI
	if (gameState_ == GAME_STATE::PLAY)
	{
		DrawShiftUI();
		DrawStageStartUI();
	}

	// STAGE CLEAR演出
	if (gameState_ == GAME_STATE::CLEAR)
	{
		// 0.0 ～ 1.0
		float rate = clearTimer_ / 0.5f;

		if (rate > 1.0f)
		{
			rate = 1.0f;
		}

		// 背景を暗くする
		int bgAlpha =
			static_cast<int>(150.0f * rate);

		SetDrawBlendMode(
			DX_BLENDMODE_ALPHA,
			bgAlpha
		);

		DrawBox(
			0,
			0,
			Application::SCREEN_SIZE_X,
			Application::SCREEN_SIZE_Y,
			GetColor(0, 0, 0),
			TRUE
		);

		SetDrawBlendMode(
			DX_BLENDMODE_NOBLEND,
			255
		);


		// STAGE CLEAR 拡大演出
		// 0.5倍 → 1.0倍
		float scale =
			0.5f + 0.5f * rate;

		int fontSize =
			static_cast<int>(48.0f * scale);

		int fontHandle =
			CreateFontToHandle(
				nullptr,
				fontSize,
				3
			);

		const char* text = "STAGE CLEAR!";

		int textWidth =
			GetDrawStringWidthToHandle(
				text,
				-1,
				fontHandle
			);

		int drawX =
			Application::SCREEN_SIZE_X / 2
			- textWidth / 2;

		int drawY =
			Application::SCREEN_SIZE_Y / 2
			- fontSize / 2;

		// 文字もフェードイン
		int textAlpha =
			static_cast<int>(255.0f * rate);

		SetDrawBlendMode(
			DX_BLENDMODE_ALPHA,
			textAlpha
		);

		DrawStringToHandle(
			drawX,
			drawY,
			text,
			GetColor(255, 230, 80),
			fontHandle
		);

		SetDrawBlendMode(
			DX_BLENDMODE_NOBLEND,
			255
		);

		DeleteFontToHandle(fontHandle);

		// NEXT STAGE
		// 1秒経過してから表示
		if (clearTimer_ >= 1.0f)
		{
			const char* nextText =
				"SPACE : NEXT STAGE";

			int nextWidth =
				GetDrawStringWidth(
					nextText,
					-1
				);

			DrawString(
				Application::SCREEN_SIZE_X / 2
				- nextWidth / 2,

				Application::SCREEN_SIZE_Y / 2
				+ 70,

				nextText,

				GetColor(255, 255, 255)
			);
		}
	}

	if (gameState_ == GAME_STATE::GAME_CLEAR)
	{
		float rate =
			clearTimer_ / 1.0f;

		if (rate > 1.0f)
		{
			rate = 1.0f;
		}

		// 背景暗転
		int bgAlpha =
			static_cast<int>(
				180.0f * rate
				);

		SetDrawBlendMode(
			DX_BLENDMODE_ALPHA,
			bgAlpha
		);

		DrawBox(
			0,
			0,
			Application::SCREEN_SIZE_X,
			Application::SCREEN_SIZE_Y,
			GetColor(0, 0, 0),
			TRUE
		);

		SetDrawBlendMode(
			DX_BLENDMODE_NOBLEND,
			255
		);

		// GAME CLEAR
		float scale =
			0.5f + 0.5f * rate;

		int fontSize =
			static_cast<int>(
				64.0f * scale
				);

		int fontHandle =
			CreateFontToHandle(
				nullptr,
				fontSize,
				3
			);

		const char* text =
			"GAME CLEAR";

		int width =
			GetDrawStringWidthToHandle(
				text,
				-1,
				fontHandle
			);

		SetDrawBlendMode(
			DX_BLENDMODE_ALPHA,
			static_cast<int>(255.0f * rate)
		);

		DrawStringToHandle(
			Application::SCREEN_SIZE_X / 2
			- width / 2,

			Application::SCREEN_SIZE_Y / 2
			- 80,

			text,

			GetColor(100, 220, 255),

			fontHandle
		);

		SetDrawBlendMode(
			DX_BLENDMODE_NOBLEND,
			255
		);

		DeleteFontToHandle(
			fontHandle
		);

		// サブタイトル
		const char* subText =
			"ALL STAGES COMPLETED";

		int subWidth =
			GetDrawStringWidth(
				subText,
				-1
			);

		DrawString(
			Application::SCREEN_SIZE_X / 2
			- subWidth / 2,

			Application::SCREEN_SIZE_Y / 2
			+ 10,

			subText,

			GetColor(255, 255, 255)
		);

		// タイトルへ戻る
		if (clearTimer_ >= 2.0f)
		{
			const char* backText =
				"SPACE : TITLE";

			int backWidth =
				GetDrawStringWidth(
					backText,
					-1
				);

			DrawString(
				Application::SCREEN_SIZE_X / 2
				- backWidth / 2,

				Application::SCREEN_SIZE_Y / 2
				+ 90,

				backText,

				GetColor(180, 180, 180)
			);
		}
	}

	// ヘルプ
	DrawFormatString(840, 20, 0x000000, "移動　　：WASD");
	DrawFormatString(840, 40, 0x000000, "カメラ　：矢印キー");
	DrawFormatString(840, 60, 0x000000, "ダッシュ：右Shift");
	DrawFormatString(840, 80, 0x000000, "ジャンプ：＼(バクスラ)");

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

void GameScene::DrawShiftUI(void)
{
	const int centerX = Application::SCREEN_SIZE_X - 90;
	const int centerY = Application::SCREEN_SIZE_Y - 110;
	const int radius = 55;

	unsigned int colorOuter = GetColor(255, 255, 255);
	unsigned int colorInner = GetColor(40, 40, 50);

	const char* stateText = "SHIFT";

	switch (shiftState_)
	{
	case SHIFT_STATE::IDLE:
		drawImage = shiftIdleImage_;
		break;

	case SHIFT_STATE::RECORDING:
		drawImage = shiftRecordingImage_;
		break;

	case SHIFT_STATE::PLAYING:
		drawImage = shiftPlayingImage_;
		break;
	}

	// SHIFTアイコン画像
	if (drawImage != -1)
	{
		const int iconSize = 120;

		DrawExtendGraph(
			centerX - iconSize / 2,
			centerY - iconSize / 2,
			centerX + iconSize / 2,
			centerY + iconSize / 2,
			drawImage,
			true
		);
	}

	// キー表示
	DrawString(
		centerX - 30,
		centerY + 70,
		"[SHIFT]",
		GetColor(255, 255, 255)
	);

	if (shiftState_ == SHIFT_STATE::RECORDING)
	{
		float rate = player_->GetRecordRate();

		const int barX = centerX - 60;
		const int barY = centerY + 95;

		const int barWidth = 120;
		const int barHeight = 8;

		// 背景
		DrawBox(
			barX,
			barY,
			barX + barWidth,
			barY + barHeight,
			GetColor(70, 70, 70),
			true
		);

		// 記録量
		DrawBox(
			barX,
			barY,
			barX + static_cast<int>(barWidth * rate),
			barY + barHeight,
			GetColor(255, 80, 80),
			true
		);

		// 円形の記録ゲージ
		const int gaugeRadius = radius + 8;
		const int segmentCount = 60;

		// 現在表示する線の数
		const int drawCount =
			static_cast<int>(segmentCount * rate);

		for (int i = 0; i < drawCount; i++)
		{
			// -90度から開始する
			const float angle =
				-DX_PI_F / 2.0f +
				(DX_TWO_PI_F * i / segmentCount);

			const float nextAngle =
				-DX_PI_F / 2.0f +
				(DX_TWO_PI_F * (i + 1) / segmentCount);

			const int x1 =
				centerX +
				static_cast<int>(cosf(angle) * gaugeRadius);

			const int y1 =
				centerY +
				static_cast<int>(sinf(angle) * gaugeRadius);

			const int x2 =
				centerX +
				static_cast<int>(cosf(nextAngle) * gaugeRadius);

			const int y2 =
				centerY +
				static_cast<int>(sinf(nextAngle) * gaugeRadius);

			DrawLine(
				x1,
				y1,
				x2,
				y2,
				GetColor(255, 80, 80),
				5
			);
		}
	}

	// 記録中の文字
	if (shiftState_ == SHIFT_STATE::RECORDING)
	{
		DrawString(
			centerX - 35,
			centerY - 85,
			"RECORDING",
			GetColor(255, 100, 100)
		);
	}

	// Ghost再生中の文字
	if (shiftState_ == SHIFT_STATE::PLAYING)
	{
		DrawString(
			centerX - 35,
			centerY - 85,
			"REPLAY",
			GetColor(180, 120, 255)
		);
	}
}

void GameScene::ChangeGameStage(STAGE_NO stageNo)
{
	stageNo_ = stageNo;

	// 今のステージを破棄
	floorSwitch_.reset();

	floorSwitch1_.reset();
	floorSwitch2_.reset();

	door_.reset();
	door1_.reset();
	door2_.reset();

	goal_.reset();

	enemies_.clear();

	// Playerに登録しているColliderを一旦削除
	player_->ClearCollider();

	// ステージ本体のColliderを戻す
	auto planet =
		stage_->GetPlanet(
			Stage::NAME::MAIN_PLANET
		).lock();

	if (planet)
	{
		player_->AddCollider(
			planet->GetTransform().collider
		);
	}

	// 新しいステージ作成
	switch (stageNo_)
	{
	case STAGE_NO::STAGE_1:

		MakeStage1();

		break;


	case STAGE_NO::STAGE_2:

		MakeStage2();

		break;

	case STAGE_NO::STAGE_3:

		MakeStage3();
		break;
	}

	// 状態リセット
	gameState_ = GAME_STATE::PLAY;

	shiftState_ = SHIFT_STATE::IDLE;

	clearTimer_ = 0.0f;

	// ステージ開始時のタイマーリセット
	stageStartTimer_ = 0.0f;
}

void GameScene::DrawStageStartUI(void)
{
	// タイマーが経過したら描画終了
	if(stageStartTimer_ >= STAGE_START_UI_TIME)
	{
		return;
	}

	// 表示する文字
	const char* stageText = "";
	const char* subText = "";

	switch(stageNo_)
	{
	case STAGE_NO::STAGE_1:
		stageText = "STAGE 1";
		subText = "FIRST SHIFT";
		break;
	case STAGE_NO::STAGE_2:
		stageText = "STAGE 2";
		subText = "DOUBLE SWITCH";
		break;
	case STAGE_NO::STAGE_3:
		stageText = "STAGE 3";
		subText = "GHOST PUZZLE";
		break;
	default:
		break;
	}

	// フェード
	int alpha = 255;

	// 最初の0.5秒の領域展開
	if(stageStartTimer_ < 0.5f)
	{
		float rate =
			stageStartTimer_ / 0.5f;

		alpha =
			static_cast<int>(
				255.0f * rate
				);
	}

	// 最後の0.5秒の領域収縮
	else if (stageStartTimer_ > STAGE_START_UI_TIME - 0.5f)
	{
		float rate =
			(STAGE_START_UI_TIME - stageStartTimer_)
			/ 0.5f;

		alpha =
			static_cast<int>(
				255.0f * rate
				);
	}

	// 拡大率
	float scale = 1.0f;

	// 最初の0.5秒だけ
	if (stageStartTimer_ < 0.5f)
	{
		float rate =
			stageStartTimer_ / 0.5f;

		scale =
			0.7f + 0.3f * rate;
	}

	// 画面中央
	const int centerX =
		Application::SCREEN_SIZE_X / 2;

	const int centerY =
		Application::SCREEN_SIZE_Y / 2 - 100;

	// 半透明描画
	SetDrawBlendMode(
		DX_BLENDMODE_ALPHA,
		alpha
	);

	// 上のライン
	DrawLine(
		centerX - 180,
		centerY - 65,

		centerX + 180,
		centerY - 65,

		GetColor(100, 220, 255),

		2
	);

	// 下のライン
	DrawLine(
		centerX - 180,
		centerY + 65,

		centerX + 180,
		centerY + 65,

		GetColor(100, 220, 255),

		2
	);

	// STAGE文字
	int fontSize =
		static_cast<int>(
			48.0f * scale
			);

	// フォント作成
	int fontHandle =
		CreateFontToHandle(
			nullptr,
			fontSize,
			3
		);

	// 文字の横幅を取得
	int stageWidth =
		GetDrawStringWidthToHandle(
			stageText,
			-1,
			fontHandle
		);

	// 中央に描画
	DrawStringToHandle(
		centerX - stageWidth / 2,
		centerY - 35,

		stageText,

		GetColor(255, 255, 255),

		fontHandle
	);

	// フォント解放
	DeleteFontToHandle(
		fontHandle
	);

	// サブタイトル
	int subWidth =
		GetDrawStringWidth(
			subText,
			-1
		);

	DrawString(
		centerX - subWidth / 2,
		centerY + 30,

		subText,

		GetColor(100, 220, 255)
	);

	// ブレンドモードを元に戻す
	SetDrawBlendMode(
		DX_BLENDMODE_NOBLEND,
		255
	);
}

void GameScene::MakeStage1(void)
{
	// 床スイッチ
	floorSwitch_ =
		std::make_unique<FloorSwitch>(
			*player_,
			*ghostPlayer_
		);

	floorSwitch_->Init();

	// Door
	door_ =
		std::make_unique<Door>(
			*floorSwitch_,
			Door::OPEN_TYPE::HOLD
		);

	door_->Init();

	// DoorのColliderをPlayerへ登録
	player_->AddCollider(
		door_->GetCollider()
	);

	// Goal
	goal_ =
		std::make_unique<Goal>(*player_);

	goal_->Init();
}

void GameScene::MakeStage2(void)
{
	// スイッチ1
	floorSwitch1_ =
		std::make_unique<FloorSwitch>(
			*player_,
			*ghostPlayer_
		);

	floorSwitch1_->Init();

	floorSwitch1_->SetPosition(
		{ -200.0f, -28.0f, 0.0f }
	);

	// スイッチ2
	floorSwitch2_ =
		std::make_unique<FloorSwitch>(
			*player_,
			*ghostPlayer_
		);

	floorSwitch2_->Init();

	floorSwitch2_->SetPosition(
		{ 200.0f, -28.0f, 0.0f }
	);

	// Door
	door_ =
		std::make_unique<Door>(
			*floorSwitch1_,
			*floorSwitch2_,
			Door::OPEN_TYPE::UNLOCK
		);

	door_->Init();

	// Doorの当たり判定をPlayerに登録
	player_->AddCollider(
		door_->GetCollider()
	);

	// Goal
	goal_ =
		std::make_unique<Goal>(
			*player_
		);

	goal_->Init();

	goal_->SetPosition(
		{ 400.0f, -28.0f, 20.0f }
	);
}

void GameScene::MakeStage3(void)
{
	// Switch A
	floorSwitch1_ =
		std::make_unique<FloorSwitch>(
			*player_,
			*ghostPlayer_
		);

	floorSwitch1_->Init();

	floorSwitch1_->SetPosition(
		{ -200.0f, -28.0f, 0.0f }
	);


	// Switch B
	floorSwitch2_ =
		std::make_unique<FloorSwitch>(
			*player_,
			*ghostPlayer_
		);

	floorSwitch2_->Init();

	floorSwitch2_->SetPosition(
		{ 200.0f, -28.0f, 250.0f }
	);


	// Door1
	// Switch Aを踏んでいる間だけ開く
	door1_ =
		std::make_unique<Door>(
			*floorSwitch1_,
			Door::OPEN_TYPE::HOLD
		);

	door1_->Init();

	door1_->SetPosition(
		{ 0.0f, -30.0f, 150.0f }
	);

	// Door2
	// Switch A + Bで永久解除
	door2_ =
		std::make_unique<Door>(
			*floorSwitch1_,
			*floorSwitch2_,
			Door::OPEN_TYPE::UNLOCK
		);

	door2_->Init();

	door2_->SetPosition(
		{ 0.0f, -30.0f, 400.0f }
	);

	player_->AddCollider(
		door2_->GetCollider()
	);


	// Goal
	goal_ =
		std::make_unique<Goal>(
			*player_
		);

	goal_->Init();

	goal_->SetPosition(
		{ 500.0f, -28.0f, 500.0f }
	);

	// 敵テスト
	std::unique_ptr<MeleeEnemy> enemy =
		std::make_unique<MeleeEnemy>(
			*player_,
			*ghostPlayer_
		);

	enemy->Init();

	enemy->SetPosition(
		{ 300.0f, -30.0f, 200.0f }
	);

	enemies_.push_back(
		std::move(enemy)
	);
}

void GameScene::MakeStage4(void)
{
}
