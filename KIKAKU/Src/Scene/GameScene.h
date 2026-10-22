#pragma once
#include <vector>
#include <memory>
#include "SceneBase.h"
class Stage;
class SkyDome;
class Player;
class EnemyBase;
class PixelMaterial;
class PixelRenderer;
class ModelMaterial;
class ModelRenderer;
class MenuUI;

class GameScene : public SceneBase
{

public:

	static constexpr float STAGE_START_UI_TIME = 2.0f;

	// ゲームオーバー画面で、メニューを出し始めるまでの時間
	static constexpr float GAME_OVER_MENU_TIME = 1.8f;

	enum class GAME_STATE
	{
		PLAY,
		CLEAR,
		GAME_CLEAR,
		PAUSE,		// ポーズ中
		GAME_OVER	// プレイヤーのHPが0になった
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

	// メニューで決定した内容
	enum class MENU_ACTION
	{
		NONE,
		RETRY,		// 最初からやり直す
		TITLE,		// タイトルへ戻る
		QUIT		// ゲーム終了
	};

	int attackUIHandle_;

	// ステージ
	std::unique_ptr<Stage> stage_;

	// スカイドーム
	std::unique_ptr<SkyDome> skyDome_;

	// プレイヤー
	std::unique_ptr<Player> player_;

	// 敵
	std::vector<std::unique_ptr<EnemyBase>> enemies_;

	// ポストエフェクトモード
	MODE mode_;

	// ポストエフェクト用スクリーン
	int postEffectScreen_;

	// ポストエフェクト用(モノトーン)
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

	// ロックオン
	EnemyBase* lockOnTarget_;
	bool isLockOn_;

	// ポーズ / ゲームオーバー用メニュー
	std::unique_ptr<MenuUI> menu_;

	// 「はい / いいえ」の確認中の内容(NONE なら通常のメニュー)
	MENU_ACTION confirmAction_;

	// 決定済みか(フェード中に何度も決定しないため)
	bool isMenuDecided_;

	// ゲームオーバーになってからの経過時間
	float gameOverTimer_;

	// ステージ作成
	void MakeStage1(void);
	void MakeStage2(void);
	void MakeStage3(void);
	void MakeStage4(void);
	void ChangeGameStage(STAGE_NO stageNo);

	void DrawStageStartUI(void);

	// ポーズ / ゲームオーバー
	void StartPause(void);
	void EndPause(void);
	void StartGameOver(void);
	void UpdatePause(void);
	void UpdateGameOver(void);
	void OpenPauseMenu(int cursor);
	void ExecuteAction(MENU_ACTION action);
	void DrawMenuOverlay(void);

};