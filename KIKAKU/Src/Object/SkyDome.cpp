#include <DxLib.h>
#include <cmath>
#include "../Application.h"
#include "../Manager/SceneManager.h"
#include "../Renderer/PixelMaterial.h"
#include "../Renderer/PixelRenderer.h"
#include "SkyDome.h"

SkyDome::SkyDome(const Transform& syncTransform)
	:
	syncTransform_(syncTransform),
	time_(0.0f)
{
}

SkyDome::~SkyDome(void)
{
}

void SkyDome::Init(void)
{

	// 空のシェーダー
	material_ = std::make_unique<PixelMaterial>("SkyPS.cso", CONST_BUF_NUM);
	for (int i = 0; i < CONST_BUF_NUM; i++)
	{
		material_->AddConstBuf(FLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f });
	}

	// 画面いっぱいの四角
	renderer_ = std::make_unique<PixelRenderer>(*material_);
	renderer_->MakeSquereVertex(
		Vector2(0, 0),
		Vector2(Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y));

	time_ = 0.0f;

}

void SkyDome::Update(void)
{
	// 雲を流す
	time_ += SceneManager::GetInstance().GetDeltaTime() * CLOUD_SPEED;
}

void SkyDome::Draw(void)
{

	if (!renderer_)
	{
		return;
	}

	// 今のカメラの向き
	const VECTOR front = VNorm(GetCameraFrontVector());
	const VECTOR up0 = GetCameraUpVector();
	const VECTOR right = VNorm(VCross(up0, front));
	const VECTOR up = VCross(front, right);

	const float tanHalfFov = std::tan(GetCameraFov() * 0.5f);
	const float aspect =
		static_cast<float>(Application::SCREEN_SIZE_X) /
		static_cast<float>(Application::SCREEN_SIZE_Y);

	const VECTOR sun = VNorm(SUN_DIR);

	material_->SetConstBuf(0, FLOAT4{ right.x, right.y, right.z, 0.0f });
	material_->SetConstBuf(1, FLOAT4{ up.x, up.y, up.z, 0.0f });
	material_->SetConstBuf(2, FLOAT4{ front.x, front.y, front.z, 0.0f });
	material_->SetConstBuf(3, FLOAT4{ time_, tanHalfFov, aspect, CLOUD_COVER });
	material_->SetConstBuf(4, FLOAT4{ sun.x, sun.y, sun.z, 0.0f });
	material_->SetConstBuf(5, FLOAT4{
		static_cast<float>(Application::SCREEN_SIZE_X),
		static_cast<float>(Application::SCREEN_SIZE_Y), 0.0f, 0.0f });

	// 空は一番奥なので、他の3D描画より先に呼ぶ
	renderer_->Draw();

}