#pragma once
#include <vector>
#include <memory>
#include "SceneBase.h"
class Stage;
class SkyDome;
class Player;
class EnemyBase;
class GhostPlayer;
class PixelMaterial;
class PixelRenderer;
class ModelMaterial;
class ModelRenderer;
class FloorSwitch;
class Door;
class Goal;
class TimeStopController;

class GameScene : public SceneBase
{

public:

	static constexpr float STAGE_START_UI_TIME = 2.0f;

	enum class GAME_STATE
	{
		PLAY,
		CLEAR,
		GAME_CLEAR
	};

	enum class STAGE_NO
	{
		STAGE_1,
		STAGE_2,
		STAGE_3
	};

	// ポストエフェクトモード
	enum class MODE
	{
		MAIN,
		MONO,
		SCAN,
		LENS,
		VINE,
		MAX
	};

	enum class SHIFT_STATE
	{
		IDLE,       // 何もしていない
		RECORDING,  // 記録中
		PLAYING     // Ghost再生中
	};

	// コンストラクタ
	GameScene(void);

	// デストラクタ
	~GameScene(void);

	void Init(void) override;
	void Update(void) override;
	void Draw(void) override;

private:

	int attackUIHandle_;

	// ステージ
	std::unique_ptr<Stage> stage_;

	// スカイドーム
	std::unique_ptr<SkyDome> skyDome_;

	// プレイヤー
	std::unique_ptr<Player> player_;

	// 敵
	std::vector<std::unique_ptr<EnemyBase>> enemies_;

	// 過去のプレイヤー
	std::unique_ptr<GhostPlayer> ghostPlayer_;
	SHIFT_STATE shiftState_;

	// 床スイッチ
	// STAGE1用
	std::unique_ptr<FloorSwitch> floorSwitch_;

	// STAGE2用
	std::unique_ptr<FloorSwitch> floorSwitch1_;
	std::unique_ptr<FloorSwitch> floorSwitch2_;

	// 扉
	std::unique_ptr<Door> door_;
	std::unique_ptr<Door> door1_;
	std::unique_ptr<Door> door2_;

	// ゴール
	std::unique_ptr<Goal> goal_;

	// 時
	std::unique_ptr<TimeStopController> timeStop_;

	// ポストエフェクトモード
	MODE mode_;

	// ポストエフェクト用スクリーン
	int postEffectScreen_;
	
	// ポストエフェクト用(モノクロ)
	std::unique_ptr<PixelMaterial> monoMaterial_;
	std::unique_ptr<PixelRenderer> monoRenderer_;

	// ポストエフェクト用(走査線)
	std::unique_ptr<PixelMaterial> scanMaterial_;
	std::unique_ptr<PixelRenderer> scanRenderer_;

	// ポストエフェクト用(ビネット)
	std::unique_ptr<PixelMaterial> vineMaterial_;
	std::unique_ptr<PixelRenderer> vineRenderer_;

	// ポストエフェクト用(レンズの歪み)
	std::unique_ptr<PixelMaterial> lensMaterial_;
	std::unique_ptr<PixelRenderer> lensRenderer_;

	// 頂点シェーダー用
	std::unique_ptr<ModelMaterial> modelMaterial_;
	std::unique_ptr<ModelRenderer> vertextRenderer_;

	int timeStopScreen_;

	// ------------------------------
	// SHIFT関連---------------------
	// ------------------------------
	// SHIFT UI
	void DrawShiftUI(void);

	// SHIFT UI画像
	int shiftIdleImage_;
	int shiftRecordingImage_;
	int shiftPlayingImage_;

	int drawImage;
	//------------------------------

	// ゲームの状態
	GAME_STATE gameState_;
	float clearTimer_;

	// 現在のステージ番号
	STAGE_NO stageNo_;

	float stageStartTimer_;

	float kamehameDamageTimer_;
	bool wasKamehame_;
	bool wasKamehameBeam_;

	// ヒットストップ
	bool isHitStop_;
	float hitStopTimer_;

	// ステージ作成
	void MakeStage1(void);
	void MakeStage2(void);
	void MakeStage3(void);
	void MakeStage4(void);
	void ChangeGameStage(STAGE_NO stageNo);

	void DrawStageStartUI(void);
};
