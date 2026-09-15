#pragma once
#include "Common/Transform.h"
class ResourceManager;
class SceneManager;

class ActorBase
{

public:
	// 共通の攻撃範囲
	static constexpr float ATTACK_RANGE = 120.0f;

	// 共通の攻撃方向判定
	static constexpr float ATTACK_DOT = 0.5f;

	// コンストラクタ
	ActorBase(void);

	// デストラクタ
	virtual ~ActorBase(void);

	virtual void Init(void) = 0;
	virtual void Update(void) = 0;
	virtual void Draw(void) = 0;

	const Transform& GetTransform(void) const;

protected:

	// シングルトン参照
	ResourceManager& resMng_;
	SceneManager& scnMng_;

	// モデル制御の基本情報
	Transform transform_;

};
