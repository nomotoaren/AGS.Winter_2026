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
#include "../../Renderer/PixelMaterial.h"
#include "../../Renderer/PixelRenderer.h"
#include "GameScene.h"
#include "ClashSystem.h"
#include "GameScene_Internal.h"

using namespace GameSceneInternal;

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

	// 枠の中心(3D)と、横に半径だけずらした点を画面の座標にして、円の大きさを決める
	const VECTOR c = player_->GetBarrage().GetAimPoint();
	const VECTOR s0 = ConvWorldPosToScreenPos(c);
	const VECTOR s1 = ConvWorldPosToScreenPos(
		VAdd(c, VScale(GetCameraRightVector(), PlayerBarrage::RingRadius())));

	// カメラの後ろ側にあるときは描かない
	if (s0.z <= 0.0f || s0.z >= 1.0f)
	{
		return;
	}

	const float r = std::fabs(s1.x - s0.x);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 50);
	DrawCircleAA(s0.x, s0.y, r, 64, GetColor(255, 255, 255), TRUE);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 220);
	DrawCircleAA(s0.x, s0.y, r, 64, GetColor(255, 255, 255), FALSE, 3.0f);

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

//-----------------------------------------------------------------------------
// ステージクリア
//-----------------------------------------------------------------------------

void GameScene::StartClear(void)
{
	gameState_ = GAME_STATE::CLEAR;
	PlayRecord::ClearTime() = playTime_;

	// 経験値をもらって、クリアを記録して保存する
	const int stage = GameProgress::Get().selectedStage;

	gainedExp_ = StageData::Get(stage).expReward;
	clearLevelBefore_ = GameProgress::Get().level;

	GameProgress::OnStageCleared(stage, gainedExp_);
	GameProgress::Save();

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
		SceneManager::GetInstance().ChangeScene(SceneManager::SCENE_ID::STAGE_SELECT);
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

	// もらった経験値と、レベルアップ
	{
		const int level = GameProgress::Get().level;

		char buf[96];

		snprintf(buf, sizeof(buf), "EXP +%d", gainedExp_);

		DrawString(
			Application::SCREEN_SIZE_X / 2 - GetDrawStringWidth(buf, -1) / 2,
			Application::SCREEN_SIZE_Y / 2 + 45,
			buf,
			GetColor(255, 255, 255));

		if (level > clearLevelBefore_)
		{
			snprintf(buf, sizeof(buf), "LEVEL UP!   Lv %d -> Lv %d", clearLevelBefore_, level);

			DrawString(
				Application::SCREEN_SIZE_X / 2 - GetDrawStringWidth(buf, -1) / 2,
				Application::SCREEN_SIZE_Y / 2 + 75,
				buf,
				GetColor(255, 230, 80));
		}
	}

	// ボタンで進めるようになったら案内を出す
	if (clearTimer_ >= CLEAR_SKIP_TIME)
	{
		const char* hint = "SPACE / A : STAGE SELECT";

		const int hintWidth = GetDrawStringWidth(hint, -1);

		DrawString(
			Application::SCREEN_SIZE_X / 2 - hintWidth / 2,
			Application::SCREEN_SIZE_Y / 2 + 115,
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
