#pragma once
#include <memory>
#include <map>
#include <functional>
#include "Common/Transform.h"
#include "ActorBase.h"
class Player;
class ModelMaterial;
class ModelRenderer;

class Water : public ActorBase
{

public:

	// 準備時間
	static constexpr float TIME_WARP_RESERVE = 2.0f;

	// 半径
	static constexpr float RADIUS = 80.0f;

	// エフェクトの位置
	static constexpr VECTOR EFFECT_RELATIVE_POS = { 0.0f, 70.0f, 0.0f };

	// ワープ前の準備移動相対座標
	static constexpr VECTOR WARP_RELATIVE_POS = { 0.0f, -80.0f, 10.0f };

	// 回転速度
	static constexpr float SPEED_ROT_IDLE = 3.0f;
	static constexpr float SPEED_ROT_RESERVE = 15.0f;

	// エフェクト生成間隔
	static constexpr float TERM_EFFECT = 0.08f;

	// 状態
	enum class STATE
	{
		NONE,
		IDLE,
		RESERVE,
		MOVE
	};

	// コンストラクタ
	Water(
		Player& player, const Transform& transform);

	// デストラクタ
	~Water(void);

	void Init(void) override;
	void Update(void) override;
	void Draw(void) override;

private:

	// プレイヤー
	Player& player_;

	// モデルマテリアル
	std::unique_ptr<ModelMaterial> modelMaterial_;

	// モデルレンダラー
	std::unique_ptr<ModelRenderer> modelRenderer_;

	// 状態管理
	STATE state_;
	// 状態管理(状態遷移時初期処理)
	std::map<STATE, std::function<void(void)>> stateChanges_;
	// 状態管理(更新ステップ)
	std::function<void(void)> stateUpdate_;

	// 状態遷移
	void ChangeState(STATE state);
	void ChangeStateNone(void);
	void ChangeStateIdle(void);
	void ChangeStateReserve(void);
	void ChangeStateMove(void);

	// 更新ステップ
	void UpdateNone(void);
	void UpdateIdle(void);
	void UpdateReserve(void);
	void UpdateMove(void);
};
