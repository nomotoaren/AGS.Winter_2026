#include <DxLib.h>
#include "Transform.h"
#include "../../Manager/ResourceManager.h"
#include "../../Manager/Resource.h"
#include "../../Manager/SceneManager.h"
#include "../../Utility/AsoUtility.h"
#include "SpeechBalloon.h"

SpeechBalloon::SpeechBalloon(TYPE type, const Transform& parent) : transformParent_(parent)
{

	type_ = type;

	Resource res = ResourceManager::GetInstance().Load(ResourceManager::SRC::SPEECH_BALLOON);
	image_ = res.handleId_;

	isVisiblePermanent_ = true;
	visibleTime_ = -1.0f;

	pos_ = { 0.0f, 0.0f, 0.0f };
	localPos_ = { 0.0f, 0.0f, 0.0f };

}

SpeechBalloon::~SpeechBalloon(void)
{
}

void SpeechBalloon::Draw(void)
{

	if (!isVisiblePermanent_)
	{
		visibleTime_ -= SceneManager::GetInstance().GetDeltaTime();
		if (visibleTime_ < 0.0f)
		{
			return;
		}
	}

	// 対象物の姿勢から見た、上方向に3D座標をずらす
	VECTOR relativePos = transformParent_.quaRot.PosAxis(localPos_);
	VECTOR pos = VAdd(transformParent_.pos, relativePos);
	pos_ = ConvWorldPosToScreenPos(pos);

	switch (type_)
	{
	case SpeechBalloon::TYPE::TEXT:
		DrawTextOrg();
		break;
	case SpeechBalloon::TYPE::SPEECH:
		DrawSpeech();
		break;
	}

}

void SpeechBalloon::DrawTextOrg(void)
{

	// カメラのSetCameraNearFarから外れていた場合、表示しない
	if (pos_.z > 0.0f && pos_.z < 1.0f)
	{
		DrawFormatString(
			static_cast<int>(pos_.x), static_cast<int>(pos_.y),
			0xdd0000, "%s", text_.c_str());
	}

}

void SpeechBalloon::DrawSpeech(void)
{

	// カメラのSetCameraNearFarから外れていた場合、表示しない
	if (pos_.z > 0.0f && pos_.z < 1.0f)
	{
		int x = AsoUtility::Round(pos_.x);
		int y = AsoUtility::Round(pos_.y);
		DrawRotaGraph(x, y, 0.5f, 0.0f, image_, true);
		int len = (int)strlen(text_.c_str());
		int width = GetDrawStringWidth(text_.c_str(), len);
		DrawFormatString(x - (width / 2), y - 15, 0x000000, "%s", text_.c_str());
	}

}

void SpeechBalloon::SetText(const std::string& text)
{
	text_ = text;
}

void SpeechBalloon::SetTime(double time)
{
	isVisiblePermanent_ = false;
	visibleTime_ = time;
}

void SpeechBalloon::SetType(TYPE type)
{
	type_ = type;
}

void SpeechBalloon::SetVisiblePermanent(void)
{
	isVisiblePermanent_ = true;
}

void SpeechBalloon::SetLocalPos(const VECTOR& localPos)
{
	localPos_ = localPos;
}
