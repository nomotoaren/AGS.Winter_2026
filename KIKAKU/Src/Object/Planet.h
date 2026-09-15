#pragma once
#include "../Common/Quaternion.h"
#include "ActorBase.h"
#include "Stage.h"

class ModelMaterial;
class ModelRenderer;

class Planet : public ActorBase
{

public:

	// 重力の強さ
	static constexpr float DEFAULT_GRAVITY_POW = 25.0f;

	// 重力がかかる範囲
	static constexpr float DEFAULT_GRAVITY_RADIUS = 5000.0f;

	// ゲームオーバー範囲
	static constexpr float DEFAULT_DEAD_LENGTH = 1000.0f;

	// 重力種別
	enum class TYPE
	{
		GROUND,
		SPHERE,
		TRANS_ROT,
		TRANS_CAMERA_FIXED,
		ROAD,
	};

	// コンストラクタ
	Planet(const Stage::NAME& name, const TYPE& type, const Transform& transform);

	// デストラクタ
	~Planet(void);

	void Init(void) override;
	void Update(void) override;
	void Draw(void) override;
	
	// 惑星種別
	const TYPE& GetType(void) const;

	// 惑星名
	const Stage::NAME& GetName(void) const;

	// 位置の設定
	void SetPosition(const VECTOR& pos);

	// 回転の設定
	void SetRotation(const Quaternion& rot);

	// 重力の強さの取得・設定
	float GetGravityPow(void) const;
	void SetGravityPow(float pow);
	
	// 重力範囲の取得・設定
	float GetGravityRadius(void) const;
	void SetGravityRadius(float radius);

	// 重力範囲内
	bool InRangeGravity(const VECTOR& pos) const;

	// ゲームオーバー範囲内
	bool InRangeDead(const VECTOR& pos) const;

	// ゲームオーバー範囲
	void SetDeadLength(float len);

private:

	// オリジナルシェーダ用（メインステージのみ使用）
	std::unique_ptr<ModelMaterial> modelMaterial_;
	std::unique_ptr<ModelRenderer> modelRenderer_;

	// 惑星種別
	TYPE type_;

	// 惑星名
	Stage::NAME name_;

	// 重力の強さ
	float gravityPow_;

	// 重力範囲
	float gravityRadius_;

	// ゲームーオーバー距離
	float deadLength_;

};
