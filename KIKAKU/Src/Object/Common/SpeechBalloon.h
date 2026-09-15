#pragma once
#include <string>
#include <DxLib.h>
#include "Transform.h"
class SceneManager;

class SpeechBalloon
{

public:

	// 表示種別
	enum class TYPE
	{
		TEXT,
		SPEECH
	};

	// コンストラクタ
	SpeechBalloon(TYPE type, const Transform& parent);

	// デストラクタ
	~SpeechBalloon(void);

	void Draw(void);
	void DrawTextOrg(void);
	void DrawSpeech(void);

	void SetText(const std::string& text);
	void SetTime(double time);

	void SetType(TYPE type);
	void SetVisiblePermanent(void);

	void SetLocalPos(const VECTOR& localPos);

private:

	TYPE type_;

	int image_;

	// trueの場合、永続的に表示する
	bool isVisiblePermanent_;
	double visibleTime_;

	// 吹き出しを付ける親オブジェクト
	const Transform& transformParent_;

	// 表示座標
	VECTOR pos_;

	// 表示位置の調整用
	VECTOR localPos_;

	// 表示文字
	std::string text_;

};
