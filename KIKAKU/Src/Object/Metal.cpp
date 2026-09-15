#include "../Manager/ResourceManager.h"
#include "Common/Transform.h"
#include "Player.h"
#include "Metal.h"
#include "../Renderer/ModelMaterial.h"
#include "../Renderer/ModelRenderer.h"
#include "../Utility/AsoUtility.h"
#include "../Manager/Camera.h"
#include "../Manager/SceneManager.h"

Metal::Metal(
	Player& player, const Transform& transform) : player_(player)
{

	transform_ = transform;

	state_ = STATE::NONE;

	// 状態管理
	stateChanges_.emplace(STATE::IDLE, std::bind(&Metal::ChangeStateIdle, this));
	stateChanges_.emplace(STATE::RESERVE, std::bind(&Metal::ChangeStateReserve, this));
	stateChanges_.emplace(STATE::MOVE, std::bind(&Metal::ChangeStateMove, this));

}

Metal::~Metal(void)
{
}

void Metal::Init(void)
{
	transform_.quaRot = Quaternion();

	// モデルの基本情報
	// 月
	transform_.SetModel(
		resMng_.LoadModelDuplicate(
			ResourceManager::SRC::MOON)
	);

	transform_.Update();

	// マテリアルとレンダラーを作成
	modelMaterial_ = std::make_unique<ModelMaterial>(
		"MetalModelVS.cso", 0,
		"MetalModelPS.cso", 4
	);

	auto dir = GetLightDirection();
	modelMaterial_->AddConstBufPS({ dir.x, dir.y, dir.z, 0.0f });

	auto cameraPos = SceneManager::GetInstance().GetCamera().GetPos();
	modelMaterial_->SetConstBufPS(1, { cameraPos.x, cameraPos.y, cameraPos.z, 10.0f });

	modelMaterial_->AddConstBufPS({ 1.0f, 1.0f, 1.0f, 1.0f });

	modelMaterial_->AddConstBufPS({ 0.0f, 0.0f, 0.0f, 1.0f });

	modelRenderer_ = std::make_unique<ModelRenderer>(transform_.modelId, *modelMaterial_);

	ChangeState(STATE::IDLE);
}

void Metal::Update(void)
{
	// 更新ステップ
	stateUpdate_();

	auto dir = GetLightDirection();
	modelMaterial_->SetConstBufPS(0, { dir.x, dir.y, dir.z, 0.0f });

	auto cameraPos = SceneManager::GetInstance().GetCamera().GetPos();
	modelMaterial_->SetConstBufPS(1, { cameraPos.x, cameraPos.y, cameraPos.z, 10.0f });
}

void Metal::Draw(void)
{
	//MV1DrawModel(transform_.modelId);

	modelRenderer_->Draw();
}

void Metal::ChangeState(STATE state)
{

	// 状態変更
	state_ = state;

	// 各状態遷移の初期処理
	stateChanges_[state_]();

}

void Metal::ChangeStateNone(void)
{
}

void Metal::ChangeStateIdle(void)
{
	stateUpdate_ = std::bind(&Metal::UpdateIdle, this);
}

void Metal::ChangeStateReserve(void)
{
	stateUpdate_ = std::bind(&Metal::UpdateReserve, this);
}

void Metal::ChangeStateMove(void)
{
	stateUpdate_ = std::bind(&Metal::UpdateMove, this);
}

void Metal::UpdateNone(void)
{
}

void Metal::UpdateIdle(void)
{
}

void Metal::UpdateReserve(void)
{
}

void Metal::UpdateMove(void)
{
}