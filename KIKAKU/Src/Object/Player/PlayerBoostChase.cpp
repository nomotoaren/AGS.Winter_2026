#include <cmath>
#include "PlayerConfig.h"
#include "PlayerBoostChase.h"

namespace cfg = PlayerConfig;

namespace
{
	constexpr float EPSILON = 0.001f;

	VECTOR NormalizeSafe(const VECTOR& v)
	{
		return (VSize(v) > EPSILON) ? VNorm(v) : VGet(0.0f, 0.0f, 0.0f);
	}

	VECTOR ToHorizontal(const VECTOR& v)
	{
		return VGet(v.x, 0.0f, v.z);
	}
}

PlayerBoostChase::PlayerBoostChase(void)
	:
	isActive_(false),
	timer_(0.0f),
	dir_(VGet(0.0f, 0.0f, 0.0f)),
	side_(1.0f),
	cooldown_(0.0f),
	afterImageTimer_(0.0f)
{
}

void PlayerBoostChase::Tick(float deltaTime)
{
	// I‚í‚Á‚½’¼Œã‚Í­‚µ‘Ò‚Â(˜A‘Å‚Å‚®‚é‚®‚é‰ñ‚è‘±‚¯‚È‚¢‚æ‚¤‚É)
	if (cooldown_ > 0.0f)
	{
		cooldown_ -= deltaTime;
	}
}

void PlayerBoostChase::Start(const Context& ctx, int sideInput)
{
	isActive_ = true;
	timer_ = 0.0f;
	afterImageTimer_ = 0.0f;

	// ŒÊ‚Ì–c‚ç‚Þ‘¤: “ü—Í‚ª‚ ‚ê‚Î‚»‚Ì‘¤A‚È‚¯‚ê‚Î‘O‰ñ‚Æ‹t
	if (sideInput > 0)
	{
		side_ = 1.0f;
	}
	else if (sideInput < 0)
	{
		side_ = -1.0f;
	}
	else
	{
		side_ = -side_;
	}

	// Å‰‚Ìis•ûŒü = “G‚Ì•ûŒü‚©‚ç‰¡‚ÖU‚Á‚½Œü‚«
	const VECTOR toEnemy = NormalizeSafe(VSub(ctx.targetPos, ctx.playerPos));
	const VECTOR horizontal = NormalizeSafe(ToHorizontal(toEnemy));
	const VECTOR side = VCross(VGet(0.0f, 1.0f, 0.0f), horizontal);

	// “G‚ª‹ß‚¢‚Æ‚«‚ÍŒÊ‚ð¬‚³‚­‚·‚é(‘å‚«‚­U‚é‚ÆA“G‚ÌŽü‚è‚ð‰ñ‚Á‚Ä‚©‚ç‹ß‚Ã‚­–³‘Ê‚È“®‚«‚É‚È‚é)
	const float distance = VSize(VSub(ctx.targetPos, ctx.playerPos));
	float arcRate =
		(distance - cfg::Boost::ARC_MIN_DISTANCE) /
		(cfg::Boost::ARC_FULL_DISTANCE - cfg::Boost::ARC_MIN_DISTANCE);

	if (arcRate < 0.0f) { arcRate = 0.0f; }
	if (arcRate > 1.0f) { arcRate = 1.0f; }

	const float arcAngle = cfg::Boost::ARC_ANGLE * arcRate;

	dir_ = NormalizeSafe(VAdd(
		VScale(toEnemy, cosf(arcAngle)),
		VScale(side, sinf(arcAngle) * side_)));
}

PlayerBoostChase::Result PlayerBoostChase::Update(const Context& ctx)
{
	Result result;

	if (!isActive_)
	{
		return result;
	}

	// UŒ‚‚µ‚½ / ƒ^[ƒQƒbƒg‚ðŽ¸‚Á‚½ / ƒƒbƒNƒIƒ“‰ðœ ¨ I—¹
	if (ctx.interrupted || !ctx.hasTarget)
	{
		return Finish();
	}

	timer_ += ctx.deltaTime;

	// •ÛŒ¯‚Ìƒ^ƒCƒ€ƒAƒEƒg
	if (timer_ > cfg::Boost::MAX_DURATION)
	{
		return Finish();
	}

	VECTOR dir = VSub(ctx.targetPos, ctx.playerPos);
	const float distance = VSize(dir);

	if (distance <= EPSILON)
	{
		return Finish();
	}

	dir = VNorm(dir);

	// Å‰‚Íˆêu‚»‚Ìê‚Å\‚¦‚é(“G‚Ì•û‚ðŒü‚­)
	if (timer_ < cfg::Boost::INITIAL_DELAY)
	{
		result.faceDir = dir;
		return result;
	}

	// “G‚ÌŽè‘O‚Å’âŽ~(­‚µ—]—T‚ðŽ‚½‚¹‚é)
	if (distance <= cfg::Boost::STOP_DISTANCE + 1.0f)
	{
		return Finish();
	}

	const bool isNear = (distance <= cfg::Boost::NEAR_DISTANCE);

	// is•ûŒü‚ð­‚µ‚¸‚Â“G‚Ö‹È‚°‚é(ŽžŠÔ‚ª‚½‚Â‚Ù‚ÇA‹ß‚¢‚Ù‚Ç‹­‚­‹È‚°‚é)
	// ¨ ‰¡‚Ö–c‚ç‚ñ‚Å‚©‚ç“G‚ÖŠª‚«ž‚Þ‚æ‚¤‚ÈŒÊ‚É‚È‚é
	float turn = cfg::Boost::TURN_BASE + timer_ * cfg::Boost::TURN_GROW;

	if (isNear)
	{
		turn += cfg::Boost::TURN_NEAR;
	}

	const float k = 1.0f - expf(-turn * ctx.deltaTime);
	dir_ = NormalizeSafe(VAdd(dir_, VScale(VSub(dir, dir_), k)));

	// Œü‚«‚Íis•ûŒü(“G‚ª‹ß‚¢‚Æ‚«‚Í“G‚Ì•û)
	result.faceDir = isNear ? dir : dir_;

	// ‰Á‘¬(\‚¦‚ªI‚í‚Á‚Ä‚©‚ç­‚µ‚¸‚Â‘¬‚­‚·‚é)
	float speed = cfg::Boost::MAX_SPEED;

	if (timer_ < cfg::Boost::ACCEL_WINDOW)
	{
		const float rate =
			(timer_ - cfg::Boost::INITIAL_DELAY) /
			(cfg::Boost::ACCEL_WINDOW - cfg::Boost::INITIAL_DELAY);

		speed = cfg::Boost::MAX_SPEED * rate;
	}

	// “G‚É‹ß‚Ã‚­‚Ù‚ÇŒ¸‘¬‚µ‚ÄA¨‚¢‚æ‚­“Ë‚Áž‚Ý‚·‚¬‚È‚¢‚æ‚¤‚É‚·‚é
	const float brakeSpeed = cfg::Boost::BRAKE_MIN + distance * cfg::Boost::BRAKE_RATE;
	if (speed > brakeSpeed)
	{
		speed = brakeSpeed;
	}

	// “G‚ð’Ê‚è”²‚¯‚È‚¢‚æ‚¤‚É‚·‚é
	float moveDistance = speed;
	const float maxMove = distance - cfg::Boost::STOP_DISTANCE;

	if (moveDistance > maxMove)
	{
		moveDistance = maxMove;
	}

	result.move = VScale(dir_, moveDistance);

	// ˆê’èŠÔŠu‚ÅŽc‘œ‚ðŽc‚·
	afterImageTimer_ -= ctx.deltaTime;

	if (afterImageTimer_ <= 0.0f)
	{
		result.leaveAfterImage = true;
		afterImageTimer_ = cfg::Boost::AFTER_IMAGE_INTERVAL;
	}

	return result;
}

void PlayerBoostChase::Cancel(void)
{
	isActive_ = false;
	timer_ = 0.0f;
}

PlayerBoostChase::Result PlayerBoostChase::Finish(void)
{
	isActive_ = false;
	timer_ = 0.0f;
	cooldown_ = cfg::Boost::COOLDOWN;

	Result result;
	result.finished = true;
	return result;
}