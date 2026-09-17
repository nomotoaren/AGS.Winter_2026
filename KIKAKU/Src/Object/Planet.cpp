#include <DxLib.h>
#include "../Utility/AsoUtility.h"
#include "../Manager/SceneManager.h"
#include "Common/Transform.h"
#include "Planet.h"
#include "../Renderer/ModelMaterial.h"
#include "../Renderer/ModelRenderer.h"

Planet::Planet(const Stage::NAME& name, const TYPE& type, const Transform& transform)
{
	name_ = name;
	type_ = type;
	transform_ = transform;

	gravityPow_ = 0.0f;
	gravityRadius_ = 0.0f;
	deadLength_ = 0.0f;
}

Planet::~Planet(void)
{
}

void Planet::Init(void)
{
	gravityPow_ = DEFAULT_GRAVITY_POW;
	gravityRadius_ = DEFAULT_GRAVITY_RADIUS;
	deadLength_ = DEFAULT_DEAD_LENGTH;

	// シェーダファイル名
	modelMaterial_ = std::make_unique<ModelMaterial>(
		"StdModelVS.cso", 1,
		"stagePS.cso", 1
	);
	// ディレクショナルライトの方向を取得
	auto dir = GetLightDirection();

	// 頂点シェーダ用の定数バッファを登録
	modelMaterial_->AddConstBufPS({ dir.x, dir.y, dir.z, 0.0f });

	modelRenderer_ = std::make_unique<ModelRenderer>(transform_.modelId, *modelMaterial_);
}

void Planet::Update(void)
{
}

void Planet::Draw(void)
{
	if (modelRenderer_)
	{
		modelRenderer_->Draw();
	}
	else
	{
		MV1DrawModel(transform_.modelId);
	}
}

void Planet::SetPosition(const VECTOR& pos)
{
	transform_.pos = pos;
	transform_.Update();
}

void Planet::SetRotation(const Quaternion& rot)
{
	transform_.quaRot = rot;
	transform_.Update();
}

float Planet::GetGravityPow(void) const
{
	return gravityPow_;
}

void Planet::SetGravityPow(float pow)
{
	gravityPow_ = pow;
}

float Planet::GetGravityRadius(void) const
{
	return gravityRadius_;
}

void Planet::SetGravityRadius(float radius)
{
	gravityRadius_ = radius;
}

const Planet::TYPE& Planet::GetType(void) const
{
	return type_;
}

bool Planet::InRangeGravity(const VECTOR& pos) const
{
	return false;
}

bool Planet::InRangeDead(const VECTOR& pos) const
{
	return false;
}

void Planet::SetDeadLength(float len)
{
	deadLength_ = len;
}

const Stage::NAME& Planet::GetName(void) const
{
	return name_;
}