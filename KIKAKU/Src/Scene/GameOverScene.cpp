#include <DxLib.h>
#include <cmath>
#include "../Application.h"
#include "../Manager/SceneManager.h"
#include "../Manager/AppFlow.h"
#include "GameOverScene.h"

namespace
{
	// メニューの項目番号
	constexpr int kMenuRetry = 0;
	constexpr int kMenuTitle = 1;
	constexpr int kMenuQuit = 2;

	// 背景のグラデーション(上 → 下)の色と、帯の数
	constexpr int kBandCount = 48;
	constexpr int kTopR = 10;
	constexpr int kTopG = 0;
	constexpr int kTopB = 5;
	constexpr int kBottomR = 75;
	constexpr int kBottomG = 5;
	constexpr int kBottomB = 12;
}

GameOverScene::GameOverScene(void)
{
}

GameOverScene::~GameOverScene(void)
{
}

void GameOverScene::DrawBackground(void) const
{
	const int sw = Application::SCREEN_SIZE_X;
	const int sh = Application::SCREEN_SIZE_Y;

	// 上が黒、下が暗い赤のグラデーション(横長の帯を重ねて作る)
	for (int i = 0; i < kBandCount; i++)
	{
		const float t = static_cast<float>(i) / (kBandCount - 1);
		const int r = kTopR + static_cast<int>((kBottomR - kTopR) * t);
		const int g = kTopG + static_cast<int>((kBottomG - kTopG) * t);
		const int b = kTopB + static_cast<int>((kBottomB - kTopB) * t);

		const int y0 = sh * i / kBandCount;
		const int y1 = sh * (i + 1) / kBandCount + 1;
		DrawBox(0, y0, sw, y1, GetColor(r, g, b), TRUE);
	}

	// 画面の下から、赤い光がゆっくり脈打つ
	const float pulse = 0.5f + 0.5f * sinf(GetTime() * 1.6f);
	SetDrawBlendMode(DX_BLENDMODE_ADD, 30 + static_cast<int>(30.0f * pulse));
	DrawOval(sw / 2, sh, sw / 2, sh / 3, GetColor(200, 20, 30), TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

std::string GameOverScene::GetHeadingText(void) const
{
	return "GAME OVER";
}

unsigned int GameOverScene::GetHeadingColor(void) const
{
	return GetColor(230, 40, 40);
}

std::vector<std::string> GameOverScene::GetMenuItems(void) const
{
	return { "リトライ", "タイトルへ戻る", "ゲーム終了" };
}

std::string GameOverScene::GetCaption(void) const
{
	return "やられてしまった...";
}

bool GameOverScene::OnSelect(int index)
{
	switch (index)
	{
	case kMenuRetry:
		SceneManager::GetInstance().ChangeScene(SceneManager::SCENE_ID::GAME);
		return true;

	case kMenuTitle:
		SceneManager::GetInstance().ChangeScene(SceneManager::SCENE_ID::TITLE);
		return true;

	case kMenuQuit:
		// メインループが AppFlow::IsQuitRequested() を見て終わる
		AppFlow::RequestQuit();
		return false;

	default:
		return false;
	}
}