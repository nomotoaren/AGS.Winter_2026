#pragma once
#include <memory>
#include <chrono>
#include <functional>

// メインカメラの取得
#define mainCamera SceneManager::GetInstance().GetCamera()

class SceneBase;
class Fader;
class Camera;

class SceneManager
{

public:

	// シーン管理用
	enum class SCENE_ID
	{
		NONE,
		TITLE,
		STAGE_SELECT,
		GAME,
		GAME_OVER,		
		GAME_CLEAR		
	};

	// インスタンスの生成
	static void CreateInstance(void);

	// インスタンスの取得
	static SceneManager& GetInstance(void);

	void Init(void);
	void Init3D(void);
	void Update(void);
	void Draw(void);

	// リソースの破棄
	void Destroy(void) const;

	// 状態遷移
	void ChangeScene(SCENE_ID nextId);

	// シーンIDの取得
	SCENE_ID GetSceneID(void) const;

	// デルタタイムの取得
	float GetDeltaTime(void) const;

	// SceneManager に足す
	float GetFrameScale(void) const { return GetDeltaTime() * 144.0f; }

	// 時間の流れる速さ
	void SetTimeScale(float scale) { timeScale_ = scale; }

	// カメラの取得
	Camera& GetCamera(void);

	int GetMainScreen(void) const;

	float GetTotalTime(void) const;

	// エフェクトより手前に描くもの(HUDなど)。シーンが切り替わると消える
	void SetOverlayDraw(std::function<void(void)> func) { overlayDraw_ = std::move(func); }

private:

	// 静的インスタンス
	static SceneManager* instance_;

	SCENE_ID sceneId_;
	SCENE_ID waitSceneId_;


	// 各種シーン
	std::unique_ptr<SceneBase> scene_;

	// フェード
	std::unique_ptr<Fader> fader_;

	// カメラ
	std::unique_ptr<Camera> camera_;

	// 最前面に描く処理
	std::function<void(void)> overlayDraw_;

	// シーン遷移中判定
	bool isSceneChanging_;

	// デルタタイム
	std::chrono::system_clock::time_point preTime_;
	float deltaTime_;

	float timeScale_ = 1.0f;

	// ゲーム実行時間
	float totalTime_;

	// メインスクリーン
	int mainScreen_;

	// デフォルトコンストラクタをprivateにして、
	// 外部から生成できない様にする
	SceneManager(void);
	// コピーコンストラクタも同様
	SceneManager(const SceneManager& manager) = default;
	// デストラクタも同様
	~SceneManager(void) = default;

	// デルタタイムをリセットする
	void ResetDeltaTime(void);

	// シーン遷移
	void DoChangeScene(SCENE_ID sceneId);

	// フェード
	void Fade(void);

};