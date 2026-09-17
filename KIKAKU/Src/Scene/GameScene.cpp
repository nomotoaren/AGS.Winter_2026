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
#include "../Object/Planet.h"
#include "../Renderer/PixelMaterial.h"
#include "../Renderer/PixelRenderer.h"
#include "../Renderer/ModelMaterial.h"
#include "../Renderer/ModelRenderer.h"
#include "GameScene.h"
#include <cmath>
#include <cfloat>

GameScene::GameScene(void)
{
	player_ = nullptr;
	skyDome_ = nullptr;
	stage_ = nullptr;
	kamehameDamageTimer_ = 0.0f;
	wasKamehame_ = false;
	wasKamehameBeam_ = false;
	gameState_ = GAME_STATE::PLAY;
	clearTimer_ = 0.0f;
	stageNo_ = STAGE_NO::STAGE_1;
	attackUIHandle_ = -1;
	isHitStop_ = false;
	hitStopTimer_ = 0.0f;
	lockOnTarget_ = nullptr;
	isLockOn_ = false;
}

GameScene::~GameScene(void)
{
	DeleteGraph(postEffectScreen_);
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

	MakeStage3();

	gameState_ = GAME_STATE::PLAY;
	clearTimer_ = 0.0f;
	stageStartTimer_ = 0.0f;

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
		
		// ゲーム更新
		skyDome_->Update();

		stage_->Update();

		// ロックオン切り替え
		if (ins.IsTrgDown(KEY_INPUT_H))
		{
			// ロックオン解除
			if (isLockOn_)
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
			// ロックオン開始
			else
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
					isLockOn_ = true;
					lockOnTarget_ = nearestEnemy;

					player_->SetLockOn(true);

					mainCamera.SetLockOnTarget(
						&lockOnTarget_->GetTransform()
					);

					mainCamera.ChangeMode(
						Camera::MODE::LOCK_ON
					);
				}
			}
		}

		// ロックオン中
		if (isLockOn_)
		{
			if (lockOnTarget_ == nullptr ||
				lockOnTarget_->IsDead())
			{
				isLockOn_ = false;
				lockOnTarget_ = nullptr;

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

				player_->SetAttackTarget(
					targetPos
				);

				player_->LookAtTarget(
					targetPos
				);
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

			enemy->Update();
		}

		// Enemyの攻撃判定
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

				EffekseerEffect::GetInstance()->
					PlayHitEffect(
						effectPos,
						0.0f
					);

				isHitStop_ = true;

				if (player_->GetCombo() == 8)
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

				player_->SetAttackHit();

				break;

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

				enemy->Damage(1);

				blast->Hit();

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

	DrawFormatString(
		20,
		210,
		GetColor(255, 255, 255),
		"Player HP : %d",
		player_->GetHp()
	);

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

void GameScene::ChangeGameStage(STAGE_NO stageNo)
{
	stageNo_ = stageNo;

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
}

void GameScene::MakeStage2(void)
{
}

void GameScene::MakeStage3(void)
{
	std::unique_ptr<MeleeEnemy> enemy =
		std::make_unique<MeleeEnemy>(
			*player_
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
