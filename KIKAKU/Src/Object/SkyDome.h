#pragma once
#include <map>
#include <functional>
#include "Common/Transform.h"
#include "ActorBase.h"

class SkyDome : public ActorBase
{

public:

	static constexpr float SCALE = 100.0f;
	static constexpr VECTOR SCALES = { SCALE, SCALE, SCALE };

	// 状態
	enum class STATE
	{
		NONE,
		STAY,
		FOLLOW
	};

	// コンストラクタ
	SkyDome(const Transform& syncTransform);

	// デストラクタ
	~SkyDome(void);

	void Init(void) override;
	void Update(void) override;
	void Draw(void) override;

private:

	// 自機の情報
	const Transform& syncTransform_;

	// 状態
	STATE state_;
	// 状態管理(状態遷移時初期処理)
	std::map<STATE, std::function<void(void)>> stateChanges_;
	// 状態管理(更新ステップ)
	std::function<void(void)> stateUpdate_;

	// 状態遷移
	void ChangeState(STATE state);
	void ChangeStateNone(void);
	void ChangeStateStay(void);
	void ChangeStateFollow(void);

	// 更新ステップ
	void UpdateNone(void);
	void UpdateStay(void);
	void UpdateFollow(void);

};
