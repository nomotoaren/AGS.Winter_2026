#include <DxLib.h>
#include "../Application.h"
#include "Resource.h"
#include "ResourceManager.h"

ResourceManager* ResourceManager::instance_ = nullptr;

void ResourceManager::CreateInstance(void)
{
	if (instance_ == nullptr)
	{
		instance_ = new ResourceManager();
	}
	instance_->Init();
}

ResourceManager& ResourceManager::GetInstance(void)
{
	return *instance_;
}

void ResourceManager::Init(void)
{

	// 推奨しませんが、どうしても使いたい方は
	using RES = Resource;
	using RES_T = RES::TYPE;
	static std::string PATH_IMG = Application::PATH_IMAGE;
	static std::string PATH_MDL = Application::PATH_MODEL;
	static std::string PATH_EFF = Application::PATH_EFFECT;

	std::unique_ptr<Resource> res;

	// タイトル画像
	res = std::make_unique<RES>(RES_T::IMG, PATH_IMG + "Title.png");
	resourcesMap_.emplace(SRC::TITLE, std::move(res));

	// PushSpace
	res = std::make_unique<RES>(RES_T::IMG, PATH_IMG + "PushSpace.png");
	resourcesMap_.emplace(SRC::PUSH_SPACE, std::move(res));

	// 吹き出し
	res = std::make_unique<RES>(RES_T::IMG, PATH_IMG + "SpeechBalloon.png");
	resourcesMap_.emplace(SRC::SPEECH_BALLOON, std::move(res));
	
	// プレイヤー
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Player/無題.mv1");
	resourcesMap_.emplace(SRC::PLAYER, std::move(res));

	// プレイヤー影
	res = std::make_unique<RES>(RES_T::IMG, PATH_IMG + "Shadow.png");
	resourcesMap_.emplace(SRC::PLAYER_SHADOW, std::move(res));

	// スカイドーム
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "SkyDome/SkyDome.mv1");
	resourcesMap_.emplace(SRC::SKY_DOME, std::move(res));

	// 最初の惑星
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Planet/MainPlanet.mv1");
	resourcesMap_.emplace(SRC::MAIN_PLANET, std::move(res));

	// 落とし穴の惑星
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Planet/FallPlanet.mv1");
	resourcesMap_.emplace(SRC::FALL_PLANET, std::move(res));

	// 平坦な惑星01
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Planet/FlatPlanet01.mv1");
	resourcesMap_.emplace(SRC::FLAT_PLANET_01, std::move(res));

	// 平坦な惑星02
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Planet/FlatPlanet02.mv1");
	resourcesMap_.emplace(SRC::FLAT_PLANET_02, std::move(res));

	// 最後の惑星
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Planet/LastPlanet.mv1");
	resourcesMap_.emplace(SRC::LAST_PLANET, std::move(res));

	// 特別な惑星
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Planet/RoadPlanet.mv1");
	resourcesMap_.emplace(SRC::SPECIAL_PLANET, std::move(res));

	// 足煙
	res = std::make_unique<RES>(RES_T::EFFEKSEER, PATH_EFF + "Smoke/Smoke.efkefc");
	resourcesMap_.emplace(SRC::FOOT_SMOKE, std::move(res));

	// ワープスターモデル
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Star/star.mv1");
	resourcesMap_.emplace(SRC::WARP_STAR, std::move(res));

	// ワープスター用回転エフェクト
	res = std::make_unique<RES>(RES_T::EFFEKSEER, PATH_EFF + "StarDust/StarDust.efkefc");
	resourcesMap_.emplace(SRC::WARP_STAR_ROT_EFF, std::move(res));

	// ワープの軌跡線
	res = std::make_unique<RES>(RES_T::EFFEKSEER, PATH_EFF + "Warp/WarpOrbit.efkefc");
	resourcesMap_.emplace(SRC::WARP_ORBIT, std::move(res));

	// ブラックホール
	res = std::make_unique<RES>(RES_T::EFFEKSEER, PATH_EFF + "BlackHole/BlackHole.efkefc");
	resourcesMap_.emplace(SRC::BLACK_HOLE, std::move(res));

	// ゴール
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "GoalStar/GoalStar.mv1");
	resourcesMap_.emplace(SRC::GOAL_STAR, std::move(res));

	// Clear
	res = std::make_unique<RES>(RES_T::IMG, PATH_IMG + "Congratulations.png");
	resourcesMap_.emplace(SRC::CLEAR, std::move(res));

	// タンク
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Tank/Body.mv1");
	resourcesMap_.emplace(SRC::TANK_BODY, std::move(res));
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Tank/Wheel.mv1");
	resourcesMap_.emplace(SRC::TANK_WHEEL, std::move(res));
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Tank/Barrel.mv1");
	resourcesMap_.emplace(SRC::TANK_BARREL, std::move(res));

	// 月
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Moon/Moon.mv1");
	resourcesMap_.emplace(SRC::MOON, std::move(res));

	// 斧
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Metal/Axe.mv1");
	resourcesMap_.emplace(SRC::AXE, std::move(res));

	// 盾
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Metal/Shield.mv1");
	resourcesMap_.emplace(SRC::SHIELD, std::move(res));

	// 水
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Water/WaterWaveCube.mv1");
	resourcesMap_.emplace(SRC::WATER, std::move(res));

	// ドア
	res = std::make_unique<RES>(RES_T::MODEL, PATH_MDL + "Object/wooden_box.mv1");
	resourcesMap_.emplace(SRC::DOOR, std::move(res));

	res = std::make_unique<RES>(RES_T::MODEL,PATH_MDL + "WAZA/charge.mv1");
	resourcesMap_.emplace(SRC::CHARGE,std::move(res));

	res = std::make_unique<RES>(RES_T::MODEL,PATH_MDL + "WAZA/kamehameha.mv1");
	resourcesMap_.emplace(SRC::KAMEHAMEHA,std::move(res));
}

void ResourceManager::Release(void)
{
	for (auto& p : loadedMap_)
	{
		p.second.Release();
	}

	loadedMap_.clear();
}

void ResourceManager::Destroy(void)
{
	Release();
	resourcesMap_.clear();
	delete instance_;
}

const Resource& ResourceManager::Load(SRC src)
{
	Resource& res = _Load(src);
	if (res.type_ == Resource::TYPE::NONE)
	{
		return dummy_;
	}
	return res;
}

int ResourceManager::LoadModelDuplicate(SRC src)
{
	Resource& res = _Load(src);
	if (res.type_ == Resource::TYPE::NONE)
	{
		return -1;
	}

	int duId = MV1DuplicateModel(res.handleId_);
	res.duplicateModelIds_.push_back(duId);

	return duId;
}

ResourceManager::ResourceManager(void)
{
}

Resource& ResourceManager::_Load(SRC src)
{

	// ロード済みチェック
	const auto& lPair = loadedMap_.find(src);
	if (lPair != loadedMap_.end())
	{
		return lPair->second;
	}

	// リソース登録チェック
	const auto& rPair = resourcesMap_.find(src);
	if (rPair == resourcesMap_.end())
	{
		// 登録されていない
		return dummy_;
	}

	// ロード処理
	rPair->second->Load();

	// 念のためコピーコンストラクタ
	loadedMap_.emplace(src, *rPair->second);

	return *rPair->second;

}
