#include "../Manager/ResourceManager.h"
#include "Common/Transform.h"
#include "Player.h"
#include "Water.h"
#include "../Renderer/ModelMaterial.h"
#include "../Renderer/ModelRenderer.h"
#include "../Utility/AsoUtility.h"
#include "../Manager/Camera.h"
#include "../Manager/SceneManager.h"

Water::Water(
	Player& player, const Transform& transform) : player_(player)
{

	transform_ = transform;

	state_ = STATE::NONE;

	// 状態管理
	stateChanges_.emplace(STATE::IDLE, std::bind(&Water::ChangeStateIdle, this));
	stateChanges_.emplace(STATE::RESERVE, std::bind(&Water::ChangeStateReserve, this));
	stateChanges_.emplace(STATE::MOVE, std::bind(&Water::ChangeStateMove, this));
}

Water::~Water(void)
{
}

void Water::Init(void)
{
	transform_.quaRot = Quaternion();

	// モデルの基本情報
	// 水
	transform_.SetModel(
		resMng_.LoadModelDuplicate(
			ResourceManager::SRC::WATER)
	);

	transform_.Update();

	// マテリアルとレンダラーを作成
	modelMaterial_ = std::make_unique<ModelMaterial>(
		"WaterVS.cso", 1,
		"WaterPS.cso", 2
	);

	modelMaterial_->AddConstBufPS({ 1.0f, 0.9f, 0.1f, 0.8f });
	VECTOR worldLightDirection = GetLightDirection();
	modelMaterial_->AddConstBufPS({
		worldLightDirection.x, worldLightDirection.y, worldLightDirection.z, 0.0f
		});
	modelMaterial_->AddConstBufVS({ 0.0f, 10.0f, 0.1f, 2.0f });
	modelRenderer_ = std::make_unique<ModelRenderer>(transform_.modelId, *modelMaterial_);	

	ChangeState(STATE::IDLE);
}

void Water::Update(void)
{
	// 更新ステップ
	stateUpdate_();

	modelMaterial_->SetConstBufVS(0, { SceneManager::GetInstance().GetTotalTime(), 4.0f, 4.0f, 0.0f });

	VECTOR worldLightDirection = GetLightDirection();
	modelMaterial_->SetConstBufPS(0, {
		worldLightDirection.x,
		worldLightDirection.y,
		worldLightDirection.z,
		SceneManager::GetInstance().GetTotalTime()
	});

	float totalTime = SceneManager::GetInstance().GetTotalTime();
	modelMaterial_->SetConstBufVS(0, { totalTime, 10.0f, 0.1f, 2.0f });
}

void Water::Draw(void)
{
	//MV1DrawModel(transform_.modelId);

	modelRenderer_->Draw();
}

void Water::ChangeState(STATE state)
{
	// 状態変更
	state_ = state;

	// 各状態遷移の初期処理
	stateChanges_[state_]();
}

void Water::ChangeStateNone(void)
{
}

void Water::ChangeStateIdle(void)
{
	stateUpdate_ = std::bind(&Water::UpdateIdle, this);
}

void Water::ChangeStateReserve(void)
{
	stateUpdate_ = std::bind(&Water::UpdateReserve, this);
}

void Water::ChangeStateMove(void)
{
	stateUpdate_ = std::bind(&Water::UpdateMove, this);
}

void Water::UpdateNone(void)
{
}

void Water::UpdateIdle(void)
{
}

void Water::UpdateReserve(void)
{
}

void Water::UpdateMove(void)
{
}