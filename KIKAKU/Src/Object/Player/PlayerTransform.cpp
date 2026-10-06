#include "../../Manager/EffekseerEffect.h"
#include "PlayerConfig.h"
#include "PlayerTransform.h"

namespace cfg = PlayerConfig;

PlayerTransform::PlayerTransform(void)
	:
	isActive_(false),
	toSuper_(false),
	isSwapped_(false),
	isBurstPlayed_(false),
	timer_(0.0f)
{
}

void PlayerTransform::Start(bool toSuper, const VECTOR& playerPos)
{
	isActive_ = true;
	toSuper_ = toSuper;
	isSwapped_ = false;
	isBurstPlayed_ = false;
	timer_ = 0.0f;

	// 気溜めのエフェクト(変身の爆発と同じ色)。元に戻るときは出さない
	if (toSuper_)
	{
		EffekseerEffect::GetInstance()->PlayChargeEffect(playerPos, true);
	}
}

PlayerTransform::Result PlayerTransform::Update(float deltaTime, const VECTOR& playerPos, bool animEnd)
{
	Result result;

	if (!isActive_)
	{
		return result;
	}

	// このフレームの前にすでに差し替え済みか(終了判定は次のフレームから見る)
	const bool wasSwapped = isSwapped_;

	timer_ += deltaTime;

	EffekseerEffect::GetInstance()->UpdateChargeEffect(playerPos);

	// 今のモーションの再生位置(フレーム)
	const float step = timer_ * cfg::Transform::ANIM_SPEED;

	// 変身するときだけ、少し遅らせて爆発(雷)を出す
	if (!isBurstPlayed_ && toSuper_ && step >= cfg::Transform::BURST_STEP)
	{
		isBurstPlayed_ = true;

		EffekseerEffect::GetInstance()->PlayTransformEffect(
			VAdd(playerPos, VGet(0.0f, 80.0f, 0.0f)));
	}

	// モーションの途中でモデルを差し替えてもらう
	if (!isSwapped_ && step >= cfg::Transform::SWAP_STEP)
	{
		isSwapped_ = true;

		result.swap = true;
		result.step = step;
	}

	// モーションが最後まで流れたら終了(念のためタイムアウトも見る)
	if (wasSwapped && (animEnd || timer_ >= cfg::Transform::MAX_TIME))
	{
		isActive_ = false;
		timer_ = 0.0f;

		EffekseerEffect::GetInstance()->StopChargeEffect();

		result.finished = true;
	}

	return result;
}