#include <DxLib.h>
#include "../Application.h"
#include "../Manager/SceneManager.h"
#include "../Manager/Camera/Camera.h"
#include "../Manager/PadInput.h"
#include "../Common/MenuUI.h"
#include "ResultSceneBase.h"

ResultSceneBase::ResultSceneBase(void)
	:
	menu_(nullptr),
	timer_(0.0f),
	isDecided_(false)
{
}

ResultSceneBase::~ResultSceneBase(void)
{
}

void ResultSceneBase::Init(void)
{
	// 前のシーンのプレイヤーを追いかけていたカメラを、定点カメラに戻す
	// (戻さないと、消えたプレイヤーを参照してしまう)
	mainCamera.ChangeMode(Camera::MODE::FIXED_POINT);

	menu_ = std::make_unique<MenuUI>();
	timer_ = 0.0f;
	isDecided_ = false;
}

void ResultSceneBase::Update(void)
{
	// プレイヤーが居ないシーンなので、パッド入力は自分で更新する
	PadInput::Update();

	const float dt = SceneManager::GetInstance().GetDeltaTime();
	timer_ += dt;

	// 少し待ってから、メニューを出す
	if (!menu_->IsOpen())
	{
		if (timer_ >= MENU_OPEN_TIME)
		{
			menu_->Open(GetMenuItems(), 0);
			menu_->SetCaption(GetCaption());
		}
		return;
	}

	if (isDecided_)
	{
		return;
	}

	const int result = menu_->Update(dt);
	if (result >= 0 && OnSelect(result))
	{
		isDecided_ = true;
	}
}

void ResultSceneBase::Draw(void)
{
	DrawBackground();

	// 見出しは、ゆっくり濃くなる
	float rate = timer_ / HEADING_FADE_TIME;
	if (rate > 1.0f)
	{
		rate = 1.0f;
	}

	menu_->DrawHeading(
		GetHeadingText(),
		static_cast<int>(Application::SCREEN_SIZE_Y * HEADING_Y_RATE),
		GetHeadingColor(),
		static_cast<int>(255.0f * rate));

	menu_->Draw(static_cast<int>(Application::SCREEN_SIZE_Y * MENU_Y_RATE));
}

std::string ResultSceneBase::GetCaption(void) const
{
	return "";
}