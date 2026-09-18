#include <DxLib.h>
#include "../../Manager/SceneManager.h"
#include "AnimationController.h"

AnimationController::AnimationController(int modelId)
{
	modelId_ = modelId;

	playType_ = -1;
	isLoop_ = false;

	isStop_ = false;
	switchLoopReverse_ = 0.0f;
	endLoopSpeed_ = 0.0f;
	stepEndLoopStart_ = 0.0f;
	stepEndLoopEnd_ = 0.0f;
}

AnimationController::~AnimationController(void)
{
	for (const auto& anim : animations_)
	{
		MV1DeleteModel(anim.second.model);
	}
}

void AnimationController::Add(int type, const std::string& path, float speed)
{

	Animation anim;

	anim.model = MV1LoadModel(path.c_str());
	anim.animIndex = type;
	anim.speed = speed;

	if (animations_.count(type) == 0)
	{
		// 入れ替え
		animations_.emplace(type, anim);
	}
	else
	{
		// 追加
		animations_[type].model = anim.model;
		animations_[type].animIndex = anim.animIndex;
		animations_[type].attachNo = anim.attachNo;
		animations_[type].totalTime = anim.totalTime;
	}

}

void AnimationController::Play(int type, bool isLoop, 
	float startStep, float endStep, bool isStop, bool isForce)
{

	if (playType_ != type || isForce) {

		if (playType_ != -1)
		{
			// モデルからアニメーションを外す
			playAnim_.attachNo = MV1DetachAnim(modelId_, playAnim_.attachNo);
		}

		// アニメーション種別を変更
		playType_ = type;
		playAnim_ = animations_[type];

		// 初期化
		playAnim_.step = startStep;

		// モデルにアニメーションを付ける
		int animIdx = 0;
		if (MV1GetAnimNum(playAnim_.model) > 1)
		{
			// アニメーションが複数保存されていたら、番号1を指定
			animIdx = 1;
		}
		playAnim_.attachNo = MV1AttachAnim(modelId_, animIdx, playAnim_.model);

		// アニメーション総時間の取得
		if (endStep > 0.0f)
		{
			playAnim_.totalTime = endStep;
		}
		else
		{
			playAnim_.totalTime = MV1GetAttachAnimTotalTime(modelId_, playAnim_.attachNo);

		}

		//printfDx(
		//	"ANIM type:%d totalTime:%f speed:%f\n",
		//	type,
		//	playAnim_.totalTime,
		//	playAnim_.speed
		//);

		// アニメーションループ
		isLoop_ = isLoop;

		// アニメーションしない
		isStop_ = isStop;

		stepEndLoopStart_ = -1.0f;
		stepEndLoopEnd_ = -1.0f;
		switchLoopReverse_ = 1.0f;
	}

}

void AnimationController::Update(void)
{

	// 経過時間の取得
	float deltaTime = SceneManager::GetInstance().GetDeltaTime();

	if (!isStop_)
	{
		// 再生
		playAnim_.step += (deltaTime * playAnim_.speed * switchLoopReverse_);

		// アニメーション終了判定
		bool isEnd = false;
		if (switchLoopReverse_ > 0.0f)
		{
			// 通常再生の場合
			if (playAnim_.step > playAnim_.totalTime)
			{
				isEnd = true;
			}
		}
		else
		{
			// 逆再生の場合
			if (playAnim_.step < playAnim_.totalTime)
			{
				isEnd = true;
			}
		}

		if (isEnd)
		{
			// アニメーションが終了したら
			if (isLoop_)
			{
				// ループ再生
				if (stepEndLoopStart_ > 0.0f)
				{
					// アニメーション終了後の指定フレーム再生
					switchLoopReverse_ *= -1.0f;
					if (switchLoopReverse_ > 0.0f)
					{
						playAnim_.step = stepEndLoopStart_;
						playAnim_.totalTime = stepEndLoopEnd_;
					}
					else
					{
						playAnim_.step = stepEndLoopEnd_;
						playAnim_.totalTime = stepEndLoopStart_;
					}
					playAnim_.speed = endLoopSpeed_;
					
				}
				else
				{
					// 通常のループ再生
					playAnim_.step = 0.0f;
				}
			}
			else
			{
				// ループしない
				playAnim_.step = playAnim_.totalTime;
			}

		}

	}

	// アニメーション設定
	MV1SetAttachAnimTime(modelId_, playAnim_.attachNo, playAnim_.step);

}

void AnimationController::SetEndLoop(float startStep, float endStep, float speed)
{
	stepEndLoopStart_ = startStep;
	stepEndLoopEnd_ = endStep;
	endLoopSpeed_ = speed;
}

int AnimationController::GetPlayType(void) const
{
	return playType_;
}

int AnimationController::CopyPose(int modelId)
{
	if (playType_ == -1)
	{
		return -1;
	}

	Animation anim =
		animations_[playType_];

	int animIdx = 0;

	if (MV1GetAnimNum(anim.model) > 1)
	{
		animIdx = 1;
	}

	int attachNo =
		MV1AttachAnim(
			modelId,
			animIdx,
			anim.model
		);

	MV1SetAttachAnimTime(
		modelId,
		attachNo,
		playAnim_.step
	);

	return attachNo;
}

bool AnimationController::IsEnd(void) const
{

	bool ret = false;

	if (isLoop_)
	{
		// ループ設定されているなら、
		// 無条件で終了しないを返す
		return ret;
	}

	if (playAnim_.step >= playAnim_.totalTime)
	{
		// 再生時間を過ぎたらtrue
		return true;
	}

	return ret;

}

float AnimationController::GetPlayRate(void) const
{
	if (playAnim_.totalTime <= 0.0f)
	{
		return 0.0f;
	}

	return playAnim_.step /
		playAnim_.totalTime;
}

void AnimationController::ClearEndLoop(void)
{
	stepEndLoopStart_ = -1.0f;
	stepEndLoopEnd_ = -1.0f;
	endLoopSpeed_ = 0.0f;
	switchLoopReverse_ = 1.0f;
}