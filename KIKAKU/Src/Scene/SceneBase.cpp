#include "../Manager/ResourceManager.h"
#include "SceneBase.h"

SceneBase::SceneBase(void) : resMng_(ResourceManager::GetInstance())
{
}

SceneBase::~SceneBase()
{
}

void SceneBase::Init(void)
{
}

void SceneBase::Update(void)
{
}

void SceneBase::Draw(void)
{
}
