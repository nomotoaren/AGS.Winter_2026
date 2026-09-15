#pragma once
#include <memory>
#include <map>
#include "Common/Transform.h"
class ResourceManager;
class WarpStar;
class Planet;
class Player;
class Metal;
class Water;

class Stage
{

public:

	// ステージの切り替え間隔
	static constexpr float TIME_STAGE_CHANGE = 1.0f;

	// ステージ名
	enum class NAME
	{
		MAIN_PLANET,
		FALL_PLANET,
		FLAT_PLANET_BASE,
		FLAT_PLANET_ROT01,
		FLAT_PLANET_ROT02,
		FLAT_PLANET_ROT03,
		FLAT_PLANET_ROT04,
		FLAT_PLANET_FIXED01,
		FLAT_PLANET_FIXED02,
		PLANET10,
		LAST_STAGE,
		SPECIAL_STAGE
	};

	// コンストラクタ
	Stage(Player& player);

	// デストラクタ
	~Stage(void);

	void Init(void);
	void Update(void);
	void Draw(void);

	// ステージ変更
	void ChangeStage(NAME type);

	// 対象ステージを取得
	std::weak_ptr<Planet> GetPlanet(NAME type);

private:

	// シングルトン参照
	ResourceManager& resMng_;

	Player& player_;
	// ステージアクティブになっている惑星の情報
	NAME activeName_;
	std::weak_ptr<Planet> activePlanet_;

	// 惑星
	std::map<NAME, std::shared_ptr<Planet>> planets_;

	// ワープスター
	std::vector<std::unique_ptr<WarpStar>> warpStars_;

	// マテル
	std::vector<std::unique_ptr<Metal>> matels_;

	// ウォーター
	std::vector<std::unique_ptr<Water>> waters_;

	// 空のPlanet
	std::shared_ptr<Planet> nullPlanet = nullptr;

	float step_;

	// 最初の惑星
	void MakeMainStage(void);

	// ワープスター
	void MakeWarpStar(void);

	// マテル
	void MakeMatel(void);

	// ウォーター
	void MakeWater(void);

};
