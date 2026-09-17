#include <vector>
#include <map>
#include <DxLib.h>
#include "../Utility/AsoUtility.h"
#include "../Manager/SceneManager.h"
#include "../Manager/ResourceManager.h"
#include "Player.h"
#include "Planet.h"
#include "Metal.h"
#include "Common/Collider.h"
#include "Common/Transform.h"
#include "Stage.h"

Stage::Stage(Player& player)
	: resMng_(ResourceManager::GetInstance()), player_(player)
{
	activeName_ = NAME::MAIN_PLANET;
	step_ = 0.0f;
}

Stage::~Stage(void)
{
	// 惑星
	planets_.clear();

	// Metal
	matels_.clear();
}

void Stage::Init(void)
{
	MakeMainStage();
	MakeMatel();

	step_ = -1.0f;
}

void Stage::Update(void)
{

	//// ワープスター
	//for (const auto& s : warpStars_)
	//{
	//	s->Update();
	//}

	// 惑星
	for (const auto& s : planets_)
	{
		s.second->Update();
	}

	//// マテル
	//for (const auto& m : matels_)
	//{
	//	m->Update();
	//}

	//// ウォーター
	//for (const auto& w : waters_)
	//{
	//	w->Update();
	//}
}

void Stage::Draw(void)
{

	//// ワープスター
	//for (const auto& s : warpStars_)
	//{
	//	s->Draw();
	//}

	// 惑星
	for (const auto& s : planets_)
	{
		s.second->Draw();
	}

	//// マテル
	//for (const auto& m : matels_)
	//{
	//	m->Draw();
	//}

	//// ウォーター
	//for (const auto& w : waters_)
	//{
	//	w->Draw();
	//}
}

void Stage::ChangeStage(NAME type)
{

	activeName_ = type;

	// 対象のステージを取得する
	activePlanet_ = GetPlanet(activeName_);

	// ステージの当たり判定をプレイヤーに設定
	player_.ClearCollider();
	player_.AddCollider(activePlanet_.lock()->GetTransform().collider);

	step_ = TIME_STAGE_CHANGE;

}

std::weak_ptr<Planet> Stage::GetPlanet(NAME type)
{
	if (planets_.count(type) == 0)
	{
		return nullPlanet;
	}

	return planets_[type];
}

void Stage::MakeMainStage(void)
{

	// 最初の惑星
	//------------------------------------------------------------------------------
	Transform planetTrans;
	planetTrans.SetModel(
		resMng_.LoadModelDuplicate(ResourceManager::SRC::MAIN_PLANET));
	planetTrans.scl = AsoUtility::VECTOR_ONE;
	planetTrans.quaRot = Quaternion();
	planetTrans.pos = { 0.0f, -100.0f, 0.0f };

	// 当たり判定(コライダ)作成
	planetTrans.MakeCollider(Collider::TYPE::STAGE);

	planetTrans.Update();

	NAME name = NAME::MAIN_PLANET;
	std::shared_ptr<Planet> planet =
		std::make_shared<Planet>(
			name, Planet::TYPE::GROUND, planetTrans);
	planet->Init();
	planets_.emplace(name, std::move(planet));
	//------------------------------------------------------------------------------
}

void Stage::MakeMatel(void)
{
	Transform trans;
	std::unique_ptr<Metal> metal;
	
	// マテル
	//------------------------------------------------------------------------------
	trans.pos = { 0.0f, 0.0f,0.0f };
	trans.scl = { 1.0f, 1.0f,1.0f };
	trans.quaRot = Quaternion::Euler(
		AsoUtility::Deg2RadF(0.0f),
		AsoUtility::Deg2RadF(0.0f),
		AsoUtility::Deg2RadF(0.0f)
	);

	metal = std::make_unique<Metal>(player_, trans);
	metal->Init();
	matels_.push_back(std::move(metal));
	//------------------------------------------------------------------------------
}
