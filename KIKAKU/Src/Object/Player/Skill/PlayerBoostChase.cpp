#include <cmath>
#include "../definition/PlayerConfig.h"
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
	// 終わった直後は少し待つ(連打でぐるぐる回り続けないように)
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

	const float distance = VSize(VSub(ctx.targetPos, ctx.playerPos));

	// 到着までの時間は距離に応じて決める(近ければ一瞬、遠くても長くなりすぎない)
	duration_ = cfg::Boost::DURATION_MIN + distance * cfg::Boost::DURATION_PER_DISTANCE;

	if (duration_ > cfg::Boost::DURATION_MAX) { duration_ = cfg::Boost::DURATION_MAX; }

	// 弧の膨らむ側: 入力があればその側、なければ前回と逆
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

	// 最初の進行方向 = 敵の方向から横へ振った向き
	const VECTOR toEnemy = NormalizeSafe(VSub(ctx.targetPos, ctx.playerPos));
	const VECTOR horizontal = NormalizeSafe(ToHorizontal(toEnemy));
	const VECTOR side = VCross(VGet(0.0f, 1.0f, 0.0f), horizontal);

	// 敵が近いときは弧を小さくする(大きく振ると、敵の周りを回ってから近づく無駄な動きになる)
	float arcRate =
		(distance - cfg::Boost::ARC_MIN_DISTANCE) /
		(cfg::Boost::ARC_FULL_DISTANCE - cfg::Boost::ARC_MIN_DISTANCE);

	if (arcRate < 0.0f) { arcRate = 0.0f; }
	if (arcRate > 1.0f) { arcRate = 1.0f; }

	const float arcAngle = cfg::Boost::ARC_ANGLE * arcRate;

	if (VSize(horizontal) > EPSILON)
	{
		lastHDir_ = horizontal;
	}

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

	// 攻撃した / ターゲットを失った / ロックオン解除 → 終了
	if (ctx.interrupted || !ctx.hasTarget)
	{
		return Finish();
	}

	timer_ += ctx.deltaTime;

	// 保険のタイムアウト
	if (timer_ > cfg::Boost::MAX_DURATION)
	{
		return Finish();
	}

	// 目的地 = 敵と同じ高さで、水平に STOP_DISTANCE 手前の点
	// (敵の真下や真上から突っ込まず、攻撃できる横の位置へ入る。上下に逃げる敵でも体が重ならない)
	const VECTOR toEnemyH = ToHorizontal(VSub(ctx.targetPos, ctx.playerPos));

	if (VSize(toEnemyH) > EPSILON)
	{
		lastHDir_ = VNorm(toEnemyH);
	}

	const VECTOR goal = VSub(ctx.targetPos, VScale(lastHDir_, cfg::Boost::STOP_DISTANCE));

	VECTOR dir = VSub(goal, ctx.playerPos);
	const float distance = VSize(dir);

	if (distance <= cfg::Boost::ARRIVE_MARGIN)
	{
		return Finish(true);
	}

	dir = VNorm(dir);

	// 最初は一瞬その場で構える(敵の方を向く)
	if (timer_ < cfg::Boost::INITIAL_DELAY)
	{
		result.faceDir = dir;
		return result;
	}

	const bool isNear = (distance <= cfg::Boost::NEAR_DISTANCE);

	// 進行方向を少しずつ敵へ曲げる(時間がたつほど、近いほど強く曲げる)
	// → 横へ膨らんでから敵へ巻き込むような弧になる
	float turn = cfg::Boost::TURN_BASE + timer_ * cfg::Boost::TURN_GROW;

	if (isNear)
	{
		turn += cfg::Boost::TURN_NEAR;
	}

	const float k = 1.0f - expf(-turn * ctx.deltaTime);
	dir_ = NormalizeSafe(VAdd(dir_, VScale(VSub(dir, dir_), k)));

	// 向きは進行方向(敵が近いときは敵の方)
	// (横へ向いたまま飛ぶと不自然なので、進行方向より敵の方を強めに向く)
	const VECTOR toEnemyDir = NormalizeSafe(VSub(ctx.targetPos, ctx.playerPos));
	result.faceDir = isNear ? toEnemyDir : NormalizeSafe(VAdd(VScale(dir_, 0.3f), VScale(toEnemyDir, 0.7f)));

	// 「到着時刻」を決めておき、その時刻に敵の懐へ着くように毎フレーム進む量を決める
	// (敵が下がっても、いま居る位置へ向かい直すので、必ず同じ時刻に追いつく)
	const float elapsed = timer_ - cfg::Boost::INITIAL_DELAY;
	float remain = duration_ - elapsed;

	if (remain < ctx.deltaTime)
	{
		remain = ctx.deltaTime;
	}

	const float maxMove = distance;
	float moveDistance = maxMove * (ctx.deltaTime / remain);

	if (moveDistance > maxMove)
	{
		moveDistance = maxMove;
	}

	if (moveDistance > cfg::Boost::MAX_SPEED)
	{
		moveDistance = cfg::Boost::MAX_SPEED;
	}

	result.move = VScale(dir_, moveDistance);

	// 一定間隔で残像を残す
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

PlayerBoostChase::Result PlayerBoostChase::Finish(bool arrived)
{
	isActive_ = false;
	timer_ = 0.0f;
	cooldown_ = cfg::Boost::COOLDOWN;

	Result result;
	result.finished = true;
	result.arrived = arrived;
	return result;
}