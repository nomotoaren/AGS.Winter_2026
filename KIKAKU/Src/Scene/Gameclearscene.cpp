#include <DxLib.h>
#include <cmath>
#include <cstdio>
#include "../Application.h"
#include "../Manager/SceneManager.h"
#include "../Manager/AppFlow.h"
#include "../Manager/PlayRecord.h"
#include "GameClearScene.h"

namespace
{
	// メニューの項目番号
	constexpr int kMenuRetry = 0;
	constexpr int kMenuTitle = 1;
	constexpr int kMenuQuit = 2;

	// 背景のグラデーション(上 → 下)の色と、帯の数
	constexpr int kBandCount = 48;
	constexpr int kTopR = 5;
	constexpr int kTopG = 10;
	constexpr int kTopB = 40;
	constexpr int kBottomR = 35;
	constexpr int kBottomG = 70;
	constexpr int kBottomB = 130;

	// 舞い上がる光の粒
	constexpr int kSparkleCount = 70;
	constexpr float kSparkleSpeedMin = 30.0f;	// 上へ昇る速さ(px/秒)
	constexpr float kSparkleSpeedMax = 120.0f;

	// 整数から 0.0 〜 1.0 の値を作る(同じ n なら同じ値)
	float Hash01(int n)
	{
		unsigned int x = static_cast<unsigned int>(n);
		x = (x << 13) ^ x;
		x = (x * (x * x * 15731u + 789221u) + 1376312589u) & 0x7fffffffu;
		return static_cast<float>(x) / 2147483648.0f;
	}
}

GameClearScene::GameClearScene(void)
{
}

GameClearScene::~GameClearScene(void)
{
}

void GameClearScene::DrawBackground(void) const
{
	const int sw = Application::SCREEN_SIZE_X;
	const int sh = Application::SCREEN_SIZE_Y;
	const float t = GetTime();

	// 上が濃い青、下が明るい青のグラデーション(横長の帯を重ねて作る)
	for (int i = 0; i < kBandCount; i++)
	{
		const float rate = static_cast<float>(i) / (kBandCount - 1);
		const int r = kTopR + static_cast<int>((kBottomR - kTopR) * rate);
		const int g = kTopG + static_cast<int>((kBottomG - kTopG) * rate);
		const int b = kTopB + static_cast<int>((kBottomB - kTopB) * rate);

		const int y0 = sh * i / kBandCount;
		const int y1 = sh * (i + 1) / kBandCount + 1;
		DrawBox(0, y0, sw, y1, GetColor(r, g, b), TRUE);
	}

	// 見出しの後ろの、金色のやわらかい光
	const float glow = 0.5f + 0.5f * sinf(t * 1.5f);
	SetDrawBlendMode(DX_BLENDMODE_ADD, 25 + static_cast<int>(20.0f * glow));
	DrawOval(sw / 2, static_cast<int>(sh * 0.28f), sw / 3, sh / 6, GetColor(255, 200, 80), TRUE);

	// 舞い上がる光の粒(位置は時間から計算するので、状態を持たなくてよい)
	SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
	for (int i = 0; i < kSparkleCount; i++)
	{
		const float x = Hash01(i * 3) * sw;
		const float speed = kSparkleSpeedMin + (kSparkleSpeedMax - kSparkleSpeedMin) * Hash01(i * 3 + 1);
		const float phase = Hash01(i * 3 + 2) * sh;

		// 下から上へ昇り、上に出たら下へ戻る
		const float y = sh - fmodf(speed * t + phase, static_cast<float>(sh));

		// きらきら点滅する
		const float twinkle = 0.5f + 0.5f * sinf(t * 3.0f + i);
		const int bright = 80 + static_cast<int>(120.0f * twinkle);
		const float size = 1.5f + 2.5f * Hash01(i * 5 + 7);

		DrawCircleAA(x, y, size, 12, GetColor(bright, bright * 85 / 100, bright / 3), TRUE);
	}
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

std::string GameClearScene::GetHeadingText(void) const
{
	return "GAME CLEAR";
}

unsigned int GameClearScene::GetHeadingColor(void) const
{
	return GetColor(255, 215, 90);
}

std::vector<std::string> GameClearScene::GetMenuItems(void) const
{
	return { "もう一度遊ぶ", "タイトルへ戻る", "ゲーム終了" };
}

std::string GameClearScene::GetCaption(void) const
{
	// クリアタイムを「分:秒」にする
	const int total = static_cast<int>(PlayRecord::ClearTime());
	const int minutes = total / 60;
	const int seconds = total % 60;

	char buf[64];
	std::snprintf(buf, sizeof(buf), "クリアタイム  %02d:%02d", minutes, seconds);
	return buf;
}

bool GameClearScene::OnSelect(int index)
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