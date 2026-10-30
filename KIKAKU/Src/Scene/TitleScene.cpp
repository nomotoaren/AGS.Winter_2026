#include <string>
#include <DxLib.h>
#include "../Application.h"
#include "../Utility/AsoUtility.h"
#include "../Manager/SceneManager.h"
#include "../Manager/ResourceManager.h"
#include "../Manager/InputManager.h"
#include "../Manager/PadInput.h"
#include "../Manager/AppFlow.h"
#include "../Manager/Camera/Camera.h"
#include "../Common/MenuUI.h"
#include "../Object/Common/AnimationController.h"
#include "../Object/SkyDome.h"
#include "TitleScene.h"

TitleScene::TitleScene(void)
{
	imgTitle_ = -1;
	skyDome_ = nullptr;
	animationController_ = nullptr;
	menu_ = nullptr;
	isDecided_ = false;
}

TitleScene::~TitleScene(void)
{
}

void TitleScene::Init(void)
{

	// 画像読み込み
	imgTitle_ = resMng_.Load(ResourceManager::SRC::TITLE).handleId_;

	// 背景
	spaceDomeTran_.pos = AsoUtility::VECTOR_ZERO;
	skyDome_ = std::make_unique<SkyDome>(spaceDomeTran_);
	skyDome_->Init();

	float size;

	// メイン惑星
	planet_.SetModel(resMng_.LoadModelDuplicate(ResourceManager::SRC::FALL_PLANET));
	planet_.pos = AsoUtility::VECTOR_ZERO;
	planet_.scl = AsoUtility::VECTOR_ONE;
	planet_.Update();

	// 回転する惑星
	movePlanet_.SetModel(resMng_.LoadModelDuplicate(ResourceManager::SRC::LAST_PLANET));
	movePlanet_.pos = { -250.0f, -100.0f, -100.0f };
	size = 0.7f;
	movePlanet_.scl = { size, size, size };
	movePlanet_.quaRotLocal = Quaternion::Euler(
		AsoUtility::Deg2RadF(90.0f), 0.0f, 0.0f);
	movePlanet_.Update();

	// キャラ
	charactor_.SetModel(resMng_.LoadModelDuplicate(ResourceManager::SRC::PLAYER));
	charactor_.pos = { -250.0f, -32.0f, -105.0f };
	size = 0.4f;
	charactor_.scl = { size, size, size };
	charactor_.quaRot = Quaternion::Euler(
		0.0f, AsoUtility::Deg2RadF(90.0f), 0.0f);
	charactor_.Update();

	// アニメーションの設定
	std::string path = Application::PATH_MODEL + "Player/";
	animationController_ = std::make_unique<AnimationController>(charactor_.modelId);
	animationController_->Add(0, path + "Run.mv1", 20.0f);
	animationController_->Play(0);

	// 定点カメラ
	mainCamera.ChangeMode(Camera::MODE::FIXED_POINT);

	// タイトルメニュー
	menu_ = std::make_unique<MenuUI>();
	menu_->Open({ "ゲームスタート", "ゲーム終了" }, 0);

	isDecided_ = false;

}

void TitleScene::Update(void)
{

	// パッド入力はプレイヤーが居ないこのシーンでは自分で更新する
	PadInput::Update();

	// メニュー操作
	if (!isDecided_)
	{
		const int result = menu_->Update(SceneManager::GetInstance().GetDeltaTime());

		if (result == 0)
		{
			// ゲームスタート(SPACE / ENTER / ○ でも始まる)
			isDecided_ = true;
			SceneManager::GetInstance().ChangeScene(SceneManager::SCENE_ID::STAGE_SELECT);
		}
		else if (result == 1)
		{
			// ゲーム終了(メインループが AppFlow::IsQuitRequested() を見て終わる)
			AppFlow::RequestQuit();
		}
	}

	// 惑星の回転
	movePlanet_.quaRot = movePlanet_.quaRot.Mult(
		Quaternion::Euler(0.0f, 0.0f, AsoUtility::Deg2RadF(-1.0f)));
	movePlanet_.Update();

	// キャラアニメーション
	animationController_->Update();

	skyDome_->Update();

}

void TitleScene::Draw(void)
{

	skyDome_->Draw();

	MV1DrawModel(planet_.modelId);
	MV1DrawModel(movePlanet_.modelId);
	MV1DrawModel(charactor_.modelId);

	DrawRotaGraph(Application::SCREEN_SIZE_X / 2, 250, 1.0, 0.0, imgTitle_, true);

	// メニュー(旧「PUSH SPACE」画像の位置)
	menu_->Draw(440);

}