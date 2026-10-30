#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <DxLib.h>
#include "../SceneBase.h"

class Stage;
class SkyDome;
class Player;
class EnemyBase;
class PixelMaterial;
class PixelRenderer;
class MenuUI;
class ClashSystem;

class GameScene : public SceneBase
{

public:

	// 敵を全部倒してから、ステージクリアになるまでの間(秒)
	static constexpr float CLEAR_DELAY_TIME = 1.0f;

	// ステージクリアになってから、ステージ選択へ戻るまでの時間(秒)
	static constexpr float CLEAR_WAIT_TIME = 6.0f;

	// ステージクリアになってから、ボタンで先へ進めるようになるまでの時間(秒)
	static constexpr float CLEAR_SKIP_TIME = 1.0f;

	// タイトルへ戻る直前に、画面が白くなっていく時間(秒)
	static constexpr float CLEAR_FADE_TIME = 1.0f;

	// HPが0になってから、ゲームオーバー画面へ移るまでの時間(秒)
	static constexpr float GAME_OVER_WAIT_TIME = 1.5f;

	enum class GAME_STATE
	{
		PLAY,
		CLEAR,		// ステージクリア(待ってタイトルへ)
		PAUSE,		// ポーズ中
		GAME_OVER	// プレイヤーのHPが0になった
	};

	// ポストエフェクトのモード
	enum class MODE
	{
		MAIN,
		MONO,
		SCAN,
		LENS,
		VINE,
		MAX
	};

	GameScene(void);
	~GameScene(void);

	void Init(void) override;
	void Update(void) override;
	void Draw(void) override;

private:

	// メニューで決定した内容
	enum class MENU_ACTION
	{
		NONE,
		RETRY,		// 最初からやり直し
		TITLE,		// タイトルへ戻る
		QUIT		// ゲーム終了
	};

	//------------------------------------------------------------
	// 持ち物
	//------------------------------------------------------------

	std::unique_ptr<Stage> stage_;
	std::unique_ptr<SkyDome> skyDome_;
	std::unique_ptr<Player> player_;
	std::vector<std::unique_ptr<EnemyBase>> enemies_;

	// 相殺勝負(かめはめ波と敵の大技のぶつかり合い)
	std::unique_ptr<ClashSystem> clash_;

	// ポーズ / ゲームオーバー用のメニュー
	std::unique_ptr<MenuUI> menu_;

	//------------------------------------------------------------
	// ポストエフェクト
	//------------------------------------------------------------

	MODE mode_;
	int postEffectScreen_;

	std::unique_ptr<PixelMaterial> monoMaterial_;
	std::unique_ptr<PixelRenderer> monoRenderer_;

	std::unique_ptr<PixelMaterial> scanMaterial_;
	std::unique_ptr<PixelRenderer> scanRenderer_;

	std::unique_ptr<PixelMaterial> vineMaterial_;
	std::unique_ptr<PixelRenderer> vineRenderer_;

	std::unique_ptr<PixelMaterial> lensMaterial_;
	std::unique_ptr<PixelRenderer> lensRenderer_;

	//------------------------------------------------------------
	// ゲームの状態
	//------------------------------------------------------------

	GAME_STATE gameState_;
	float playTime_;			// プレイ時間(クリア画面に表示する)
	float clearDelayTimer_;		// 敵を全部倒してからの経過時間
	float clearTimer_;			// クリアになってからの経過時間
	float gameOverTimer_;		// ゲームオーバーになってからの経過時間
	bool isResultRequested_;	// 結果画面への切り替えを始めたか(何度も呼ばない)

	// クリアでもらった経験値と、クリア前のレベル(クリア画面の表示用)
	int gainedExp_ = 0;
	int clearLevelBefore_ = 1;

	// メニュー
	MENU_ACTION confirmAction_;	// 「はい / いいえ」の確認中の内容(NONE なら通常のメニュー)
	bool isMenuDecided_;		// 決定済みか(フェード中に何度も決定しない)

	// ヒットストップ
	bool isHitStop_;
	float hitStopTimer_;

	// ブレイクした瞬間の「全体停止」の残り時間
	// (isHitStop_ は同じフレームの別のヒットで上書きされるので、専用の変数を使う)
	float breakFreeze_;

	// ロックオン
	EnemyBase* lockOnTarget_;
	bool isLockOn_;

	//------------------------------------------------------------
	// 技の途中経過(以前は関数内の static だったもの)
	//------------------------------------------------------------

	// バニッシュアタック
	EnemyBase* vanishTarget_;
	float vanishTimer_;
	VECTOR vanishDir_;

	// 追撃
	EnemyBase* pursuitTarget_;
	float pursuitTimer_;
	VECTOR pursuitDir_;

	// ジャスト回避後の、敵だけのスローモーション
	float justSlowTimer_;
	int justSlowFrame_;

	// ブレイク(スタン)した瞬間を見つけるための、前フレームのスタン状態
	std::unordered_map<EnemyBase*, bool> prevStun_;

	// ヒートアップ発動の演出カメラ
	bool heatWasActivating_;
	bool heatShookBurst_;
	bool heatShookClose_;

	// かめはめ波のダメージ間隔
	float kamehameDamageTimer_;

	//------------------------------------------------------------
	// 初期化
	//------------------------------------------------------------

	void MakeEnemies(void);
	void InitPostEffects(void);
	void ResetState(void);

	//------------------------------------------------------------
	// 毎フレームの更新(PLAY 中)
	//------------------------------------------------------------

	void UpdatePlay(float dt);

	// ポーズ・ゲームオーバー・クリアへ移るか調べる。移ったら true(その場で Update を打ち切る)
	bool CheckStateChange(float dt);

	// ヒットストップ中なら true(その間はゲームを止める)
	bool UpdateHitStop(float dt);

	// ロックオン
	void UpdateLockOn(void);
	void StartLockOn(EnemyBase* target);
	void EndLockOn(void);
	void UpdateLockOnTracking(void);
	void UpdateFreeTarget(void);

	// 技
	void UpdateVanish(float dt);
	void StartVanish(void);
	void UpdatePursuit(float dt);
	void StartPursuit(void);
	void UpdateClash(float dt);
	void UpdateGuardBurst(void);

	// 演出
	void UpdateJustDodge(void);
	void UpdateBreakFreeze(void);
	void UpdateHeatUpCamera(void);

	// 敵と体
	void UpdateEnemies(float dt);
	void PushOutBodies(void);

	// 当たり判定
	void UpdateMeleeHit(void);
	void UpdateKiBlastHit(void);
	void UpdateBarrageHit(void);
	void UpdateFinishBlastHit(void);
	void UpdateKamehameHit(float dt);

	//------------------------------------------------------------
	// 共通の部品
	//------------------------------------------------------------

	// ロックオン中の敵(いなければ、範囲内で一番近い敵)。いなければ nullptr
	EnemyBase* PickTarget(float maxRange) const;

	// プレイヤーに一番近い生きている敵(maxRange 以内)。いなければ nullptr
	EnemyBase* FindNearestEnemy(float maxRange) const;

	// ダメージを与えて、ダメージ表示を出す
	void DealDamage(EnemyBase& enemy, int damage, float stun);

	// ヒットストップとカメラの揺れ
	void StartHitStop(float time);
	void ApplyHitImpact(float stopTime, float shakeTime, float shakePower);

	bool IsAllEnemiesDead(void) const;

	//------------------------------------------------------------
	// クリア / ポーズ / ゲームオーバー
	//------------------------------------------------------------

	void StartClear(void);
	void UpdateClear(float dt);
	void DrawClear(void);

	void StartPause(void);
	void EndPause(void);
	void UpdatePause(void);
	void OpenPauseMenu(int cursor);
	void ExecuteAction(MENU_ACTION action);

	void StartGameOver(void);
	void UpdateGameOver(void);

	void DrawMenuOverlay(void);

	//------------------------------------------------------------
	// 描画
	//------------------------------------------------------------

	// ポストエフェクトを1枚かける
	void DrawPostPass(PixelRenderer& renderer);

	// 連続エネルギー弾の狙いの枠
	void DrawBarrageReticle(void);
};