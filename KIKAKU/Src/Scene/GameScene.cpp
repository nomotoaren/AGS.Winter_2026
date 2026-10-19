#include <DxLib.h>
#include "../Application.h"
#include "../Utility/AsoUtility.h"
#include "../Manager/SceneManager.h"
#include "../Manager/Camera.h"
#include "../Manager/InputManager.h"
#include "../Manager/PadInput.h"
#include "../Manager/EffekseerEffect.h"
#include "../Object/Common/Capsule.h"
#include "../Object/Common/Collider.h"
#include "../Object/SkyDome.h"
#include "../Object/Stage.h"
#include "../Object/Player/Player.h"
#include "../Object/Enemy/EnemyBase.h"
#include "../Object/Enemy/MeleeEnemy.h"
#include "../Object/Planet.h"
#include "../Renderer/PixelMaterial.h"
#include "../Renderer/PixelRenderer.h"
#include "../Renderer/ModelMaterial.h"
#include "../Renderer/ModelRenderer.h"
#include "GameScene.h"
#include <cmath>
#include <cfloat>
#include <unordered_map>

namespace
{
	// プレイヤーと敵が重ならない最小の水平距離
	// (攻撃中に保つ距離 AttackDistance=90 より小さくしておく)
	constexpr float BODY_DISTANCE = 65.0f;
	// この高さ以上離れていたら重なりとみなさない(上や下にいるときは通す)
	constexpr float BODY_HEIGHT_RANGE = 130.0f;
	// 1フレームで押し出す最大量(急に飛ばないようにする)
	constexpr float BODY_MAX_PUSH = 25.0f;
	// 押し出しの配分(プレイヤー側 / 敵側)
	constexpr float BODY_PLAYER_RATE = 0.65f;

	// 攻撃が届く高さの差(これを超えると当たらない)
	constexpr float ATTACK_HEIGHT_RANGE = 150.0f;
	// 水平距離がこれ以下のときは向き判定をしない(真上・真下の敵用)
	constexpr float ATTACK_DOT_MIN_DISTANCE = 20.0f;

	// 相殺勝負(かめはめ波と敵の大きな気弾のぶつかり合い)の状態
	struct ClashState
	{
		bool active = false;
		MeleeEnemy* enemy = nullptr;
		VECTOR pos = { 0.0f, 0.0f, 0.0f };	// ぶつかっている光の位置
		float rate = 0.5f;					// 優劣(0=負け 1=勝ち)
		float timer = 0.0f;
		bool isBeam = false;				// 相手は大きな気弾ではなく、ビーム
		VECTOR enemySide = { 0.0f, 0.0f, 0.0f };	// 敵側の光線の出どころ
	};

	ClashState g_clash;

	float Clamp01(float v)
	{
		return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
	}

	// 2本の線分の最短距離(c1, c2 は、それぞれの線分上の最も近い点)
	float SegmentDistance(const VECTOR& p1, const VECTOR& q1, const VECTOR& p2, const VECTOR& q2,
		VECTOR& c1, VECTOR& c2)
	{
		const VECTOR d1 = VSub(q1, p1);
		const VECTOR d2 = VSub(q2, p2);
		const VECTOR r = VSub(p1, p2);
		const float a = VDot(d1, d1);
		const float e = VDot(d2, d2);
		const float f = VDot(d2, r);
		float s = 0.0f;
		float t = 0.0f;

		if (a <= 1.0e-6f && e <= 1.0e-6f)
		{
			s = 0.0f;
			t = 0.0f;
		}
		else if (a <= 1.0e-6f)
		{
			t = Clamp01(f / e);
		}
		else
		{
			const float c = VDot(d1, r);

			if (e <= 1.0e-6f)
			{
				s = Clamp01(-c / a);
			}
			else
			{
				const float bb = VDot(d1, d2);
				const float denom = a * e - bb * bb;

				s = (denom > 1.0e-6f) ? Clamp01((bb * f - c * e) / denom) : 0.0f;
				t = (bb * s + f) / e;

				if (t < 0.0f)
				{
					t = 0.0f;
					s = Clamp01(-c / a);
				}
				else if (t > 1.0f)
				{
					t = 1.0f;
					s = Clamp01((bb - c) / a);
				}
			}
		}

		c1 = VAdd(p1, VScale(d1, s));
		c2 = VAdd(p2, VScale(d2, t));

		return VSize(VSub(c1, c2));
	}

	// かめはめ波が敵に当たったか(ビームが終わったら追撃のチャンスを出す)
	bool g_kamehameHit = false;
}

GameScene::GameScene(void)
	:
	player_(nullptr),
	skyDome_(nullptr),
	stage_(nullptr),
	kamehameDamageTimer_(0.0f),
	wasKamehame_(false),
	wasKamehameBeam_(false),
	gameState_(GAME_STATE::PLAY),
	clearTimer_(0.0f),
	stageNo_(STAGE_NO::STAGE_1),
	attackUIHandle_(-1),
	isHitStop_(false),
	hitStopTimer_(0.0f),
	lockOnTarget_(nullptr),
	isLockOn_(false),
	mode_(MODE::MAIN),
	postEffectScreen_(-1),
	stageStartTimer_(0.0f)
{
}

GameScene::~GameScene(void)
{
	DeleteGraph(postEffectScreen_);
}

void GameScene::Init(void)
{

	// HUD(自分・敵のHPバーなど)は、エフェクトや弾より手前に出す
	SceneManager::GetInstance().SetOverlayDraw([this]()
		{
			for (auto& enemy : enemies_)
			{
				if (enemy && !enemy->IsDead())
				{
					enemy->DrawOverlay();
				}
			}

			if (player_)
			{
				player_->DrawHud();
			}
		});
	g_clash = ClashState();
	g_kamehameHit = false;

	// プレイヤー
	player_ = std::make_unique<Player>();
	player_->Init();

	// ステージ
	stage_ = std::make_unique<Stage>(*player_);
	stage_->Init();

	// ステージの初期設定
	stage_->ChangeStage(Stage::NAME::MAIN_PLANET);

	// スカイドーム
	skyDome_ = std::make_unique<SkyDome>(player_->GetTransform());
	skyDome_->Init();

	mainCamera.SetFollow(&player_->GetTransform());
	mainCamera.ChangeMode(Camera::MODE::FOLLOW);

	// ポストエフェクト用スクリーン
	postEffectScreen_ = MakeScreen(
		Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y, true);

	// ポストエフェクト用(モノトーン)
	monoMaterial_ = std::make_unique<PixelMaterial>("Monotone.cso", 1);
	monoMaterial_->AddConstBuf({ 1.0f, 1.0f, 1.0f, 1.0f });
	monoMaterial_->AddTextureBuf(SceneManager::GetInstance().GetMainScreen());
	monoRenderer_ = std::make_unique<PixelRenderer>(*monoMaterial_);
	monoRenderer_->MakeSquereVertex(
		Vector2(0, 0),
		Vector2(Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y)
	);

	// ポストエフェクト用(走査線)
	scanMaterial_ = std::make_unique<PixelMaterial>("ScanLine.cso", 2);
	scanMaterial_->AddConstBuf({ 1.0f, 1.0f, 1.0f, 1.0f });
	scanMaterial_->AddConstBuf({ 0.0f, 0.0f, 0.0f, 0.0f });
	scanMaterial_->AddTextureBuf(SceneManager::GetInstance().GetMainScreen());
	scanRenderer_ = std::make_unique<PixelRenderer>(*scanMaterial_);
	scanRenderer_->MakeSquereVertex(
		Vector2(0, 0),
		Vector2(Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y)
	);

	// ポストエフェクト用(ビネット)
	vineMaterial_ = std::make_unique<PixelMaterial>("Vignette.cso", 1);
	vineMaterial_->AddConstBuf({ 3.5f, 0.0f, 0.0f, 0.0f });
	vineMaterial_->AddTextureBuf(SceneManager::GetInstance().GetMainScreen());
	vineRenderer_ = std::make_unique<PixelRenderer>(*vineMaterial_);
	vineRenderer_->MakeSquereVertex(
		Vector2(0, 0),
		Vector2(Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y)
	);

	// ポストエフェクト用(レンズの歪み)
	lensMaterial_ = std::make_unique<PixelMaterial>("LensDistortion.cso", 1);
	lensMaterial_->AddConstBuf({ 3.5f, 0.0f, 0.0f, 0.0f });
	lensMaterial_->AddTextureBuf(SceneManager::GetInstance().GetMainScreen());
	lensRenderer_ = std::make_unique<PixelRenderer>(*lensMaterial_);
	lensRenderer_->MakeSquereVertex(
		Vector2(0, 0),
		Vector2(Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y)
	);

	// 初期モード
	mode_ = MODE::MAIN;

	MakeStage3();

	gameState_ = GAME_STATE::PLAY;
	clearTimer_ = 0.0f;
	stageStartTimer_ = 0.0f;

	SetGlobalAmbientLight(
		GetColorF(
			1.0f,
			1.0f,
			1.0f,
			1.0f
		)
	);

	attackUIHandle_ =
		LoadGraph(
			"Data/Image/AttackUI.png"
		);
}

void GameScene::Update(void)
{
	InputManager& ins = InputManager::GetInstance();

	if (isHitStop_)
	{
		hitStopTimer_ -=
			SceneManager::GetInstance().GetDeltaTime();

		if (hitStopTimer_ <= 0.0f)
		{
			hitStopTimer_ = 0.0f;
			isHitStop_ = false;
		}
		else
		{
			return;
		}
	}

	switch (gameState_)
	{
	case GAME_STATE::PLAY:
	{

		if (stageStartTimer_ < STAGE_START_UI_TIME)
		{
			stageStartTimer_ +=
				SceneManager::GetInstance().GetDeltaTime();
		}

		// ゲーム更新
		skyDome_->Update();

		stage_->Update();

		// 常時ロックオン
		if (lockOnTarget_ == nullptr ||
			lockOnTarget_->IsDead())
		{
			EnemyBase* nearestEnemy = nullptr;
			float nearestDistance = FLT_MAX;

			for (auto& enemy : enemies_)
			{
				if (!enemy)
				{
					continue;
				}

				if (enemy->IsDead())
				{
					continue;
				}

				VECTOR toEnemy =
					VSub(
						enemy->GetTransform().pos,
						player_->GetTransform().pos
					);

				float distance =
					VSize(toEnemy);

				if (distance < nearestDistance)
				{
					nearestDistance = distance;
					nearestEnemy = enemy.get();
				}
			}

			if (nearestEnemy != nullptr)
			{
				lockOnTarget_ = nearestEnemy;
				isLockOn_ = true;

				player_->SetLockOn(true);

				mainCamera.SetLockOnTarget(
					&lockOnTarget_->GetTransform()
				);

				mainCamera.ChangeMode(
					Camera::MODE::LOCK_ON
				);
			}
		}

		// ロックオン中
		if (isLockOn_)
		{
			// 高速移動中はカメラを落ち着かせる(向きの追従をゆっくり、位置の追従を速く)
			mainCamera.SetBoostChase(player_->IsBoostChasing());

			if (lockOnTarget_ == nullptr ||
				lockOnTarget_->IsDead())
			{
				isLockOn_ = false;
				lockOnTarget_ = nullptr;

				player_->SetLockOn(false);
				player_->ClearAttackTarget();

				mainCamera.SetLockOnTarget(nullptr);
				mainCamera.ChangeMode(
					Camera::MODE::FOLLOW
				);
			}
			else
			{
				VECTOR targetPos =
					lockOnTarget_->GetTransform().pos;

				// かめはめ波中は敵を追尾しない
				if (!player_->IsKamehame())
				{
					player_->SetAttackTarget(
						targetPos
					);

					if (player_->GetCombo() != 8)
					{
						player_->LookAtTarget(
							targetPos
						);
					}
				}
			}
		}
		else
		{
			EnemyBase* nearestEnemy = nullptr;
			float nearestDistance = 300.0f;

			for (auto& enemy : enemies_)
			{
				if (!enemy)
				{
					continue;

				}

				if (enemy->IsDead())
				{
					continue;
				}

				VECTOR toEnemy =
					VSub(
						enemy->GetTransform().pos,
						player_->GetTransform().pos
					);

				float distance =
					VSize(toEnemy);

				if (distance < nearestDistance)
				{
					nearestDistance = distance;
					nearestEnemy = enemy.get();
				}
			}

			if (nearestEnemy != nullptr)
			{
				player_->SetAttackTarget(
					nearestEnemy->GetTransform().pos
				);
			}
			else
			{
				player_->ClearAttackTarget();
			}
		}

		player_->Update();

		// バニッシュアタック: ガード中に×で、敵の斜め後ろへ瞬間移動し、少し間をおいて叩き落とす
		{
			static EnemyBase* vanishTarget = nullptr;
			static float vanishTimer = 0.0f;
			static VECTOR vanishDir = { 0.0f, 0.0f, 1.0f };

			if (player_->ConsumeVanish())
			{
				auto* target = lockOnTarget_;

				// ロックオンしていなければ、一番近い敵
				if (target == nullptr || target->IsDead())
				{
					target = nullptr;
					float best = PlayerConfig::Vanish::MAX_RANGE;

					for (auto& enemy : enemies_)
					{
						if (!enemy || enemy->IsDead())
						{
							continue;
						}

						const float d = VSize(VSub(enemy->GetTransform().pos, player_->GetTransform().pos));

						if (d < best)
						{
							best = d;
							target = enemy.get();
						}
					}
				}

				if (target != nullptr)
				{
					const VECTOR enemyPos = target->GetTransform().pos;

					// 敵の向きの後ろ側(水平)。少し横にずらして、カメラが大きく回らないようにする
					VECTOR back = target->GetTransform().quaRot.GetForward();
					back.y = 0.0f;

					if (VSize(back) > 0.001f)
					{
						back = VNorm(back);
					}
					else
					{
						back = VGet(0.0f, 0.0f, 1.0f);
					}

					// 真後ろから左右どちらかへずらした2つの候補のうち、今のプレイヤーに近い方を選ぶ
					const float rad = PlayerConfig::Vanish::SIDE_ANGLE_DEG * 3.14159265f / 180.0f;
					const VECTOR playerNow = player_->GetTransform().pos;
					VECTOR warpPos = enemyPos;
					float bestDist = 1.0e9f;

					for (int k = 0; k < 2; k++)
					{
						const float ang = (k == 0) ? rad : -rad;

						// 敵の向きを ang だけ回した向きの「反対側」(= 斜め後ろ)
						const VECTOR dir = VGet(
							back.x * cosf(ang) - back.z * sinf(ang),
							0.0f,
							back.x * sinf(ang) + back.z * cosf(ang));

						VECTOR cand = VSub(enemyPos, VScale(dir, PlayerConfig::Vanish::BEHIND_DISTANCE));
						cand.y = enemyPos.y + PlayerConfig::Vanish::ABOVE_HEIGHT;

						const float d = VSize(VSub(cand, playerNow));

						if (d < bestDist)
						{
							bestDist = d;
							warpPos = cand;
						}
					}

					player_->VanishTo(warpPos, enemyPos);

					vanishTarget = target;
					vanishTimer = PlayerConfig::Vanish::HIT_DELAY;
					vanishDir = back;
				}
			}

			// 少し間をおいてから叩き落とす(消えて、現れて、叩く、が目で追えるように)
			if (vanishTarget != nullptr)
			{
				vanishTimer -= SceneManager::GetInstance().GetDeltaTime();

				if (vanishTarget->IsDead())
				{
					vanishTarget = nullptr;
				}
				else if (vanishTimer <= 0.0f)
				{
					EnemyBase* target = vanishTarget;
					vanishTarget = nullptr;

					const VECTOR enemyPos = target->GetTransform().pos;
					const VECTOR back = vanishDir;

					const int dmg =
						player_->GetAttackDamage(Player::ATTACK_DAMAGE) * PlayerConfig::Vanish::DAMAGE_RATE;

					target->Damage(dmg);
					target->AddStun(PlayerConfig::Vanish::STUN);
					player_->AddDamagePopup(VAdd(enemyPos, VGet(0.0f, 130.0f, 0.0f)), dmg);

					MeleeEnemy* meleeEnemy = dynamic_cast<MeleeEnemy*>(target);

					if (meleeEnemy != nullptr)
					{
						meleeEnemy->LookAtPlayer();
						meleeEnemy->StartSlamDown();
					}

					const VECTOR slamDir = VNorm(VGet(back.x, -0.8f, back.z));
					target->AddKnockBack(slamDir, PlayerConfig::Vanish::KNOCK);

					EffekseerEffect::GetInstance()->
						PlayHitEffect(VAdd(enemyPos, VGet(0.0f, 80.0f, 0.0f)), 0.0f);

					isHitStop_ = true;
					hitStopTimer_ = PlayerConfig::Vanish::HIT_STOP;
					mainCamera.StartShake(PlayerConfig::Vanish::SHAKE_TIME, PlayerConfig::Vanish::SHAKE_POWER);

					// (追撃はブレイクしたときだけ)
				}
			}
		}

		// 追撃: ふっ飛ばした敵の元へ瞬間移動して、追い打ちする
		{
			static EnemyBase* pursuitTarget = nullptr;
			static float pursuitTimer = 0.0f;
			static VECTOR pursuitDir = { 0.0f, 0.0f, 1.0f };

			if (player_->ConsumePursuit())
			{
				auto* target = lockOnTarget_;

				if (target == nullptr || target->IsDead())
				{
					target = nullptr;
					float best = FLT_MAX;

					for (auto& enemy : enemies_)
					{
						if (!enemy || enemy->IsDead())
						{
							continue;
						}

						const float d = VSize(VSub(enemy->GetTransform().pos, player_->GetTransform().pos));

						if (d < best)
						{
							best = d;
							target = enemy.get();
						}
					}
				}

				if (target != nullptr)
				{
					// 瞬間移動はせず、敵のところまで速く飛んでいく(下で毎コマ動かす)
					pursuitTarget = target;
					pursuitTimer = PlayerConfig::Pursuit::HIT_DELAY;
					pursuitDir = VGet(0.0f, 0.0f, 1.0f);
				}
			}

			if (pursuitTarget != nullptr)
			{
				pursuitTimer -= SceneManager::GetInstance().GetDeltaTime();

				if (pursuitTarget->IsDead())
				{
					pursuitTarget = nullptr;
				}
				else
				{
					// 敵の手前まで、決まった速さで近づく
					const VECTOR enemyNow = pursuitTarget->GetTransform().pos;
					const VECTOR playerNow = player_->GetTransform().pos;

					VECTOR flat = VSub(enemyNow, playerNow);
					flat.y = 0.0f;
					flat = (VSize(flat) > 0.001f) ? VNorm(flat) : pursuitDir;
					pursuitDir = flat;

					VECTOR stop = VSub(enemyNow, VScale(flat, PlayerConfig::Pursuit::DISTANCE));
					stop.y = enemyNow.y + PlayerConfig::Pursuit::HEIGHT;

					if (stop.y < PlayerConfig::Pursuit::MIN_HEIGHT)
					{
						stop.y = PlayerConfig::Pursuit::MIN_HEIGHT;
					}

					const VECTOR toStop = VSub(stop, playerNow);
					const float remain = VSize(toStop);
					const float step = PlayerConfig::Pursuit::DASH_SPEED * SceneManager::GetInstance().GetDeltaTime();

					const bool arrived = (remain <= step) || (pursuitTimer <= 0.0f);

					if (arrived)
					{
						player_->DashStepTo(stop, enemyNow);
					}
					else
					{
						player_->DashStepTo(VAdd(playerNow, VScale(toStop, step / remain)), enemyNow);
					}

					if (arrived)
					{
						EnemyBase* target = pursuitTarget;
						pursuitTarget = nullptr;

						const VECTOR enemyPos = target->GetTransform().pos;
						const VECTOR dir = pursuitDir;

						const int dmg =
							player_->GetAttackDamage(Player::ATTACK_DAMAGE) * PlayerConfig::Pursuit::DAMAGE_RATE;

						target->Damage(dmg);
						target->AddStun(PlayerConfig::Pursuit::STUN);
						player_->AddDamagePopup(VAdd(enemyPos, VGet(0.0f, 130.0f, 0.0f)), dmg);

						MeleeEnemy* meleeEnemy = dynamic_cast<MeleeEnemy*>(target);

						if (meleeEnemy != nullptr)
						{
							meleeEnemy->LookAtPlayer();
						}

						// また遠くへふっ飛ばす
						target->AddKnockBack(VNorm(VGet(dir.x, 0.35f, dir.z)), PlayerConfig::Pursuit::KNOCK);

						EffekseerEffect::GetInstance()->
							PlayHitEffect(VAdd(enemyPos, VGet(0.0f, 80.0f, 0.0f)), 0.0f);

						isHitStop_ = true;
						hitStopTimer_ = PlayerConfig::Pursuit::HIT_STOP;
						mainCamera.StartShake(PlayerConfig::Pursuit::SHAKE_TIME, PlayerConfig::Pursuit::SHAKE_POWER);

						// 追撃はブレイク後の最初の一発だけ(続けての追撃はなし)
					}
				}
			}
		}

		// 相殺勝負: かめはめ波が敵の大きな気弾にぶつかったら、連打で押し合う
		{
			ClashState& c = g_clash;
			const float dt = SceneManager::GetInstance().GetDeltaTime();

			if (!c.active)
			{
				if (player_->IsKamehameBeam())
				{
					const VECTOR bs = player_->GetKamehameStartPos();
					const VECTOR be = player_->GetKamehameEndPos();
					const VECTOR bv = VSub(be, bs);
					const float len2 = VDot(bv, bv);

					for (auto& enemy : enemies_)
					{
						if (!enemy || enemy->IsDead())
						{
							continue;
						}

						MeleeEnemy* me = dynamic_cast<MeleeEnemy*>(enemy.get());

						if (me == nullptr)
						{
							continue;
						}

						bool found = false;
						bool isBeam = false;
						VECTOR hitPos = { 0.0f, 0.0f, 0.0f };
						VECTOR enemySide = { 0.0f, 0.0f, 0.0f };

						// 大きな気弾にぶつかった
						VECTOR bp = { 0.0f, 0.0f, 0.0f };
						float br = 0.0f;
						int bd = 0;

						if (me->GetBigBlast(bp, br, bd))
						{
							float t = (len2 > 0.001f) ? VDot(VSub(bp, bs), bv) / len2 : 0.0f;
							t = Clamp01(t);

							const VECTOR closest = VAdd(bs, VScale(bv, t));

							if (VSize(VSub(bp, closest)) <= br + player_->GetKamehameRadius())
							{
								found = true;
								hitPos = bp;
								enemySide = VAdd(me->GetTransform().pos, VGet(0.0f, 100.0f, 0.0f));
							}
						}

						// 敵のビームにぶつかった
						if (!found)
						{
							VECTOR bo = { 0.0f, 0.0f, 0.0f };
							VECTOR bdir = { 0.0f, 0.0f, 1.0f };
							float brad = 0.0f;
							int bdmg = 0;

							if (me->GetBeam(bo, bdir, brad, bdmg))
							{
								VECTOR c1 = { 0.0f, 0.0f, 0.0f };
								VECTOR c2 = { 0.0f, 0.0f, 0.0f };

								const float d = SegmentDistance(bs, be, bo, VAdd(bo, VScale(bdir, 3000.0f)), c1, c2);

								// 敵のビームは太いので、中心線が少し近づけばぶつかったことにする
								if (d <= brad * 0.6f + player_->GetKamehameRadius())
								{
									found = true;
									isBeam = true;
									hitPos = VScale(VAdd(c1, c2), 0.5f);
									enemySide = bo;
								}
							}
						}

						if (!found)
						{
							continue;
						}

						c.active = true;
						c.enemy = me;
						c.isBeam = isBeam;
						c.pos = hitPos;
						c.enemySide = enemySide;
						c.rate = PlayerConfig::Clash::START_RATE;
						c.timer = 0.0f;

						if (isBeam)
						{
							me->SetBeamClash(true);
						}
						else
						{
							me->SetBigBlastFrozen(true);
						}

						// ビームのエフェクトは、ぶつかった所までにする(あとは自前で描く)
						EffekseerEffect::GetInstance()->SetKamehameEffectScale(
							PlayerConfig::Kamehame::BEAM_SCALE_XY,
							PlayerConfig::Kamehame::BEAM_SCALE_XY,
							VSize(VSub(hitPos, bs)) / 30.0f);

						mainCamera.StartShake(0.2f, 6.0f);
						break;
					}
				}
			}
			else
			{
				c.timer += dt;

				const bool press =
					ins.IsTrgDown(KEY_INPUT_F) ||
					ins.IsTrgDown(KEY_INPUT_U) ||
					PadInput::IsFightTrg() ||
					PadInput::IsKiBlastTrg();

				if (press) { c.rate += PlayerConfig::Clash::PUSH_PER_PRESS; }

				c.rate -= PlayerConfig::Clash::ENEMY_PUSH_PER_SEC * dt;

				if (player_->IsHeatUp()) { c.rate += PlayerConfig::Clash::HEAT_BONUS_PER_SEC * dt; }

				if (c.rate > 1.0f) { c.rate = 1.0f; }

				VECTOR bp = { 0.0f, 0.0f, 0.0f };
				float br = 0.0f;
				int bd = 0;
				VECTOR bo = { 0.0f, 0.0f, 0.0f };
				VECTOR bdir = { 0.0f, 0.0f, 1.0f };

				const bool alive = !c.enemy->IsDead() &&
					(c.isBeam ? c.enemy->GetBeam(bo, bdir, br, bd) : c.enemy->GetBigBlast(bp, br, bd));

				// かめはめ波が途切れた(被弾など) / 敵や気弾がなくなった → 勝負を終える
				if (!alive || !player_->IsKamehame())
				{
					// 相手のビーム・気弾を動かし直す
					if (c.isBeam) { c.enemy->SetBeamClash(false); }
					else if (alive) { c.enemy->SetBigBlastFrozen(false); }

					c.active = false;
					c.enemy = nullptr;
					player_->SetClash(false, 0.5f);
				}
				else
				{
					// ぶつかっている光は、優劣に応じて自分と敵の間を動く
					// ぶつかっている光の位置は動かさない(連打で押し引きされるのはゲージだけ)
					c.enemySide = c.isBeam ? bo : VAdd(c.enemy->GetTransform().pos, VGet(0.0f, 100.0f, 0.0f));

					player_->SetClash(true, c.rate);

					const bool win = (c.rate >= 1.0f);
					const bool lose = (c.rate <= 0.0f) || (c.timer >= PlayerConfig::Clash::MAX_TIME);

					if (win || lose)
					{
						const VECTOR hitPos = c.pos;
						MeleeEnemy* me = c.enemy;

						if (c.isBeam) { me->EndBeam(); }
						else { me->KillBigBlast(); }

						VECTOR dir = VSub(me->GetTransform().pos, player_->GetTransform().pos);
						dir.y = 0.0f;
						dir = (VSize(dir) > 0.001f) ? VNorm(dir) : VGet(0.0f, 0.0f, 1.0f);

						EffekseerEffect::GetInstance()->PlayHitEffect(hitPos, 0.0f);

						if (win)
						{
							// 押し勝った: 大きな気弾を押し返して、敵に大ダメージ
							const VECTOR enemyPos = me->GetTransform().pos;
							const int dmg = player_->GetAttackDamage(PlayerConfig::Clash::WIN_DAMAGE);

							me->Damage(dmg);
							me->AddStun(PlayerConfig::Clash::WIN_STUN);
							player_->AddDamagePopup(VAdd(enemyPos, VGet(0.0f, 130.0f, 0.0f)), dmg);

							me->AddKnockBack(VNorm(VGet(dir.x, 0.2f, dir.z)), PlayerConfig::Clash::WIN_KNOCK);

							EffekseerEffect::GetInstance()->
								PlayHitEffect(VAdd(enemyPos, VGet(0.0f, 80.0f, 0.0f)), 0.0f);

							isHitStop_ = true;
							hitStopTimer_ = 0.15f;
							mainCamera.StartShake(0.3f, 14.0f);

							// (追撃はブレイクしたときだけ)
						}
						else
						{
							// 押し負けた: 大きな気弾を受ける
							player_->Damage(bd * PlayerConfig::Clash::LOSE_DAMAGE_RATE);
							player_->AddKnockBack(dir, PlayerConfig::Clash::LOSE_KNOCK);

							mainCamera.StartShake(0.3f, 12.0f);
						}

						c.active = false;
						c.enemy = nullptr;
						player_->SetClash(false, 0.5f);
					}
				}
			}
		}

		// ガードバースト
		if (player_->IsGuardBurstTrigger())
		{
			VECTOR playerPos =
				player_->GetTransform().pos;

			// バースト時のカメラ揺れ
			mainCamera.StartShake(
				0.12f,
				6.0f
			);

			for (auto& enemy : enemies_)
			{
				if (!enemy)
				{
					continue;
				}

				if (enemy->IsDead())
				{
					continue;
				}

				VECTOR enemyPos =
					enemy->GetTransform().pos;

				VECTOR dir =
					VSub(
						enemyPos,
						playerPos
					);

				float distance =
					VSize(dir);

				// バーストの範囲
				if (distance > 250.0f)
				{
					continue;
				}

				if (distance <= 0.001f)
				{
					continue;
				}

				dir =
					VNorm(dir);

				MeleeEnemy* meleeEnemy =
					dynamic_cast<MeleeEnemy*>(
						enemy.get()
						);

				if (meleeEnemy != nullptr)
				{
					meleeEnemy->GuardBurst();
				}

				enemy->AddKnockBack(
					dir,
					40.0f
				);
			}
		}
		// かめはめ波中もカメラは切り替えない(ロックオン / 追従のまま)
		wasKamehame_ = player_->IsKamehame();
		//// かめはめ波カメラ
		//bool isKamehame =
		//	player_->IsKamehame();

		//bool isKamehameBeam =
		//	player_->IsKamehameBeam();

		////if (isKamehame &&
		////	!wasKamehame_)
		////{
		////	mainCamera.ChangeMode(
		////		Camera::MODE::KAMEHAME
		////	);
		////}

		////if (isKamehameBeam &&
		////	!wasKamehameBeam_)
		////{
		////	mainCamera.ChangeMode(
		////		Camera::MODE::KAMEHAME_SHOT
		////	);
		////}

		//if (!isKamehame &&
		//	wasKamehame_)
		//{
		//	if (isLockOn_ &&
		//		lockOnTarget_ != nullptr)
		//	{
		//		mainCamera.SetLockOnTarget(
		//			&lockOnTarget_->GetTransform()
		//		);

		//		mainCamera.ChangeMode(
		//			Camera::MODE::LOCK_ON
		//		);
		//	}
		//	else
		//	{
		//		mainCamera.ChangeMode(
		//			Camera::MODE::FOLLOW
		//		);
		//	}
		//}

		//wasKamehame_ =
		//	isKamehame;

		// ジャスト回避が成立したら、しばらく敵だけをスローモーションにする
		//   (敵を数フレームに1回だけ更新する。プレイヤーは普通に動けるので、反撃できる)
		static float justSlowTimer = 0.0f;
		static int justSlowFrame = 0;

		if (player_->ConsumeJustDodge())
		{
			justSlowTimer = PlayerConfig::JustDodge::SLOW_TIME;
			justSlowFrame = 0;
		}

		// ブレイク(スタン)した瞬間: 敵をスローで吹っ飛ばし、追撃のチャンスを作る
		{
			static std::unordered_map<EnemyBase*, bool> prevStun;

			for (auto& enemy : enemies_)
			{
				if (!enemy || enemy->IsDead())
				{
					continue;
				}

				const bool now = enemy->IsStunned();
				bool& prev = prevStun[enemy.get()];

				if (now && !prev)
				{
					VECTOR away = VSub(enemy->GetTransform().pos, player_->GetTransform().pos);
					away.y = 0.0f;
					away = (VSize(away) > 0.001f) ? VNorm(away) : VGet(0.0f, 0.0f, 1.0f);

					enemy->AddKnockBack(VNorm(VGet(away.x, 0.3f, away.z)), PlayerConfig::Break::KNOCK);

					justSlowTimer = PlayerConfig::Break::SLOW_TIME;
					justSlowFrame = 0;

					EffekseerEffect::GetInstance()->
						PlayHitEffect(VAdd(enemy->GetTransform().pos, VGet(0.0f, 80.0f, 0.0f)), 0.0f);
					mainCamera.StartShake(0.25f, 8.0f);

					player_->OpenPursuit(false);
				}

				prev = now;
			}
		}

		bool updateEnemy = true;

		if (justSlowTimer > 0.0f)
		{
			justSlowTimer -= SceneManager::GetInstance().GetDeltaTime();
			justSlowFrame++;

			updateEnemy = (justSlowFrame % PlayerConfig::JustDodge::SLOW_DIV) == 0;
		}

		// Enemy更新
		for (auto& enemy : enemies_)
		{
			if (!enemy)
			{
				continue;
			}

			if (enemy->IsDead())
			{
				continue;
			}

			if (updateEnemy)
			{
				enemy->Update();
			}
		}

		// プレイヤーと敵が重ならないように押し出す
		for (auto& enemy : enemies_)
		{
			if (!enemy || enemy->IsDead())
			{
				continue;
			}

			const VECTOR playerPos = player_->GetTransform().pos;
			const VECTOR enemyPos = enemy->GetTransform().pos;

			// 上や下に離れているときは重なりとみなさない
			if (std::fabs(enemyPos.y - playerPos.y) > BODY_HEIGHT_RANGE)
			{
				continue;
			}

			// 敵 → プレイヤー の水平方向
			VECTOR diff = VSub(playerPos, enemyPos);
			diff.y = 0.0f;

			const float dist = VSize(diff);

			if (dist >= BODY_DISTANCE)
			{
				continue;
			}

			VECTOR dir;

			if (dist > 0.001f)
			{
				dir = VScale(diff, 1.0f / dist);
			}
			else
			{
				// ぴったり重なっているときは、プレイヤーの後ろ側へ逃がす
				VECTOR forward = player_->GetForward();
				forward.y = 0.0f;

				dir = (VSize(forward) > 0.001f)
					? VScale(VNorm(forward), -1.0f)
					: VGet(0.0f, 0.0f, -1.0f);
			}

			float overlap = BODY_DISTANCE - dist;

			if (overlap > BODY_MAX_PUSH)
			{
				overlap = BODY_MAX_PUSH;
			}

			player_->PushOut(VScale(dir, overlap * BODY_PLAYER_RATE));
			enemy->PushOut(VScale(dir, -overlap * (1.0f - BODY_PLAYER_RATE)));
		}

		// Enemyの攻撃判定
		//for (auto& enemyBase : enemies_)
		//{
		//	if (!enemyBase)
		//	{
		//		continue;
		//	}

		//	if (enemyBase->IsDead())
		//	{
		//		continue;
		//	}

		//	MeleeEnemy* enemy =
		//		dynamic_cast<MeleeEnemy*>(
		//			enemyBase.get()
		//			);

		//	if (enemy == nullptr)
		//	{
		//		continue;
		//	}

		//	if (!enemy->IsAttackHitTiming())
		//	{
		//		continue;
		//	}

		//	if (enemy->HasAttackHit())
		//	{
		//		continue;
		//	}

		//	VECTOR enemyPos =
		//		enemy->GetTransform().pos;
		//	// Playerを狙っている
		//	VECTOR playerPos =
		//		player_->GetTransform().pos;

		//	VECTOR toPlayer =
		//		VSub(
		//			playerPos,
		//			enemyPos
		//		);

		//	float distance =
		//		VSize(toPlayer);

		//	if (distance <= ActorBase::ATTACK_RANGE)
		//	{
		//		if (!player_->IsDodging())
		//		{
		//			VECTOR hitDir =
		//				VSub(
		//					player_->GetTransform().pos,
		//					enemy->GetTransform().pos
		//				);

		//			if (VSize(hitDir) > 0.001f)
		//			{
		//				hitDir = VNorm(hitDir);

		//				if (player_->IsGuard())
		//				{
		//					// ガード耐久値を減らす
		//					if (enemy->GetAttackCombo() == 4)
		//					{
		//						player_->GuardDamage(80.0f);
		//					}
		//					else
		//					{
		//						player_->GuardDamage(20.0f);
		//					}

		//					// ガード中はダメージなしで少しだけ押される
		//					float guardKnockPower = 1.5f;

		//					if (enemy->GetAttackCombo() == 4)
		//					{
		//						guardKnockPower = 5.0f;
		//					}

		//					player_->AddKnockBack(
		//						hitDir,
		//						guardKnockPower
		//					);

		//					// ガードエフェクト
		//					VECTOR effectPos =
		//						player_->GetTransform().pos;

		//					effectPos.y += 80.0f;

		//					EffekseerEffect::GetInstance()->
		//						PlayHitEffect(
		//							effectPos,
		//							0.0f
		//						);

		//					// ガードした瞬間に少し止める
		//					isHitStop_ = true;
		//					hitStopTimer_ = 0.03f;
		//				}
		//				else
		//				{
		//					player_->LookAtDamageEnemy(
		//						enemy->GetTransform().pos
		//					);

		//					player_->Damage(
		//						MeleeEnemy::ATTACK_DAMAGE
		//					);

		//					float knockPower = 5.0f;

		//					if (enemy->GetAttackCombo() == 4)
		//					{
		//						knockPower = 30.0f;
		//					}

		//					player_->AddKnockBack(
		//						hitDir,
		//						knockPower
		//					);
		//				}
		//			}
		//		}
		//		enemy->SetAttackHit();
		//	}
		//}

		// Playerの攻撃判定
		if (player_->IsAttackHitTiming() &&
			!player_->HasAttackHit())
		{
			const int combo = player_->GetCombo();

			// 形態に応じたダメージ(変身すると倍率が変わる)
			const int attackDamage =
				player_->GetAttackDamage(Player::ATTACK_DAMAGE);

			// 攻撃範囲(水平距離)と高さの許容範囲
			float attackRange = ActorBase::ATTACK_RANGE;
			float attackHeight = ATTACK_HEIGHT_RANGE;

			// 8段目は上から叩き落とすので広め
			if (combo == 8)
			{
				attackRange = 280.0f;
				attackHeight = 300.0f;
			}

			const VECTOR playerPos =
				player_->GetTransform().pos;

			VECTOR forward = player_->GetForward();
			forward.y = 0.0f;
			const bool hasForward = VSize(forward) > 0.001f;
			if (hasForward)
			{
				forward = VNorm(forward);
			}

			for (auto& enemy : enemies_)
			{
				// 回避中(無敵)の敵には当たらない
				if (!enemy || enemy->IsDead() || enemy->IsInvincible())
				{
					continue;
				}

				const VECTOR enemyPos =
					enemy->GetTransform().pos;

				const VECTOR toEnemy =
					VSub(enemyPos, playerPos);

				// 水平距離と高さの差を別々に見る
				// (3D距離だと、敵が上下にいるだけで届かなくなるため)
				VECTOR horizontal = toEnemy;
				horizontal.y = 0.0f;

				const float horizontalDist = VSize(horizontal);
				const float verticalDist = std::fabs(toEnemy.y);

				if (horizontalDist > attackRange ||
					verticalDist > attackHeight)
				{
					continue;
				}

				// 向き判定(真上・真下など水平距離がほぼ0のときは判定しない)
				if (hasForward &&
					horizontalDist > ATTACK_DOT_MIN_DISTANCE)
				{
					const float dot =
						VDot(forward, VScale(horizontal, 1.0f / horizontalDist));

					if (dot < ActorBase::ATTACK_DOT)
					{
						continue;
					}
				}

				MeleeEnemy* meleeEnemy =
					dynamic_cast<MeleeEnemy*>(enemy.get());

				// 敵がガード中(8段目はガードを崩すので貫通)
				if (meleeEnemy != nullptr &&
					meleeEnemy->IsGuard() &&
					combo != 8)
				{
					// ガード耐久値を減らす(攻撃力が高いほど削れる)
					meleeEnemy->GuardDamage(
						20.0f * player_->GetAttackRate()
					);

					VECTOR effectPos = enemyPos;
					effectPos.y += 80.0f;

					EffekseerEffect::GetInstance()->
						PlayHitEffect(effectPos, 0.0f);

					isHitStop_ = true;
					hitStopTimer_ = 0.03f;

					player_->SetAttackHit();

					break;
				}

				// HIT
				if (meleeEnemy != nullptr)
				{
					meleeEnemy->LookAtPlayer();
				}

				enemy->Damage(attackDamage);
				enemy->AddStun(combo == 8 ? 40.0f : 8.0f);	// フィニッシュは大きくスタン値を溜める
				player_->AddDamagePopup(VAdd(enemyPos, VGet(0.0f, 130.0f, 0.0f)), attackDamage);

				float knockPower = 0.0f;

				switch (combo)
				{
				case 1: knockPower = 5.0f;   break;
				case 2: knockPower = 7.0f;   break;
				case 3: knockPower = 5.0f;   break;
				case 4: knockPower = 7.0f;   break;
				case 5: knockPower = 9.0f;   break;
				case 6: knockPower = 7.0f;   break;
				case 7: knockPower = 4.0f;   break;
				case 8: knockPower = 100.0f; break;
				}

				VECTOR hitDir = toEnemy;
				if (VSize(hitDir) > 0.001f)
				{
					hitDir = VNorm(hitDir);
				}

				// 8段目は下に叩き落とす
				if (combo == 8)
				{
					VECTOR slamDir = { forward.x, -0.8f, forward.z };
					hitDir = VNorm(slamDir);

					if (meleeEnemy != nullptr)
					{
						meleeEnemy->StartSlamDown();
					}

					// 8段目が当たったら高速追撃できる
					player_->SetCanChase(true);
					// (追撃はブレイクしたときだけ)	// ふっ飛ばしたので、追撃できる
				}

				enemy->AddKnockBack(hitDir, knockPower);

				VECTOR effectPos = enemyPos;
				effectPos.y += 80.0f;

				EffekseerEffect::GetInstance()->
					PlayHitEffect(effectPos, 0.0f);

				isHitStop_ = true;

				if (combo == 8)
				{
					hitStopTimer_ = 0.10f;
					mainCamera.StartShake(0.15f, 8.0f);
				}
				else
				{
					hitStopTimer_ = 0.05f;
					mainCamera.StartShake(0.08f, 3.0f);
				}

				player_->SetAttackHit();

				// 1攻撃で1体だけ
				break;
			}
		}

		// 気弾の攻撃判定
		for (auto& blast : player_->GetKiBlasts())
		{
			if (blast->IsDead())
			{
				continue;
			}

			VECTOR blastPos =
				blast->GetPos();

			for (auto& enemy : enemies_)
			{
				if (!enemy)
				{
					continue;
				}

				if (enemy->IsDead())
				{
					continue;
				}

				VECTOR enemyPos =
					enemy->GetTransform().pos;

				enemyPos.y += 70.0f;

				float distance =
					VSize(
						VSub(
							blastPos,
							enemyPos
						)
					);

				if (distance > 45.0f)
				{
					continue;
				}

				{
					const int blastDamage = player_->GetAttackDamage(1);
					enemy->Damage(blastDamage);
					enemy->AddStun(10.0f);
					player_->AddDamagePopup(VAdd(enemyPos, VGet(0.0f, 40.0f, 0.0f)), blastDamage);
				}

				blast->Hit();

				break;
			}
		}

		// かめはめ波ダメージタイマー
		// フィニッシュ気弾
		for (auto& finish : player_->GetFinishBlasts())
		{
			if (finish->IsDead())
			{
				continue;
			}

			for (auto& enemy : enemies_)
			{
				if (!enemy || enemy->IsDead())
				{
					continue;
				}

				const VECTOR enemyPos = enemy->GetTransform().pos;
				const VECTOR center = VAdd(enemyPos, VGet(0.0f, 70.0f, 0.0f));

				if (VSize(VSub(finish->GetPos(), center)) > finish->GetRadius() + 45.0f)
				{
					continue;
				}

				const int dmg = player_->GetAttackDamage(PlayerConfig::Finish::DAMAGE);

				enemy->Damage(dmg);
				enemy->AddStun(PlayerConfig::Finish::STUN);
				player_->AddDamagePopup(VAdd(enemyPos, VGet(0.0f, 130.0f, 0.0f)), dmg);

				enemy->AddKnockBack(finish->GetDir(), PlayerConfig::Finish::KNOCK);

				EffekseerEffect::GetInstance()->PlayHitEffect(center, 0.0f);

				isHitStop_ = true;
				hitStopTimer_ = 0.08f;
				mainCamera.StartShake(0.12f, 6.0f);

				finish->Kill();

				// ふっ飛ばしたので、追撃できる
				// (追撃はブレイクしたときだけ)
				break;
			}
		}

		if (player_->IsKamehameBeam())
		{
			kamehameDamageTimer_ +=
				SceneManager::GetInstance().GetDeltaTime();
		}
		else
		{
			kamehameDamageTimer_ = 0.0f;

			// かめはめ波が当たったまま終わったら、追撃できる
			if (g_kamehameHit)
			{
				g_kamehameHit = false;
				// (追撃はブレイクしたときだけ)
			}

			for (auto& enemy : enemies_)
			{
				if (!enemy)
				{
					continue;
				}

				MeleeEnemy* meleeEnemy =
					dynamic_cast<MeleeEnemy*>(
						enemy.get()
						);

				if (meleeEnemy != nullptr)
				{
					meleeEnemy->SetKamehameHit(false);
				}
			}
		}

		// かめはめ波の攻撃判定
		if (player_->IsKamehameBeam() && !g_clash.active)
		{
			VECTOR beamStart =
				player_->GetKamehameStartPos();

			VECTOR beamEnd =
				player_->GetKamehameEndPos();

			VECTOR beamVec =
				VSub(
					beamEnd,
					beamStart
				);

			float beamLengthSq =
				VDot(
					beamVec,
					beamVec
				);

			if (beamLengthSq > 0.001f)
			{
				for (auto& enemy : enemies_)
				{
					if (!enemy)
					{
						continue;
					}

					if (enemy->IsDead())
					{
						continue;
					}

					MeleeEnemy* meleeEnemy =
						dynamic_cast<MeleeEnemy*>(
							enemy.get()
							);

					// 最初は「ビームに当たっていない」にする
					if (meleeEnemy != nullptr)
					{
						meleeEnemy->SetKamehameHit(false);
					}

					VECTOR enemyPos =
						enemy->GetTransform().pos;

					// 敵の中心
					enemyPos.y += 70.0f;

					VECTOR startToEnemy =
						VSub(
							enemyPos,
							beamStart
						);

					float t =
						VDot(
							startToEnemy,
							beamVec
						) /
						beamLengthSq;

					if (t < 0.0f)
					{
						t = 0.0f;
					}

					if (t > 1.0f)
					{
						t = 1.0f;
					}

					VECTOR closestPos =
						VAdd(
							beamStart,
							VScale(
								beamVec,
								t
							)
						);

					VECTOR diff =
						VSub(
							enemyPos,
							closestPos
						);

					float distance =
						VSize(diff);

					static constexpr float ENEMY_HIT_RADIUS =
						45.0f;

					float hitRadius =
						player_->GetKamehameRadius() +
						ENEMY_HIT_RADIUS;

					// ビームに当たっていない
					if (distance > hitRadius)
					{
						continue;
					}

					// ビームに当たっている
					if (meleeEnemy != nullptr)
					{
						VECTOR pushDir =
							player_->GetForward();

						if (VSize(pushDir) > 0.001f)
						{
							pushDir =
								VNorm(pushDir);
						}

						meleeEnemy->SetKamehameHit(
							true,
							pushDir
						);
					}

					// ダメージだけ0.15秒ごと
					if (kamehameDamageTimer_ >= 0.15f)
					{
						{
							const int beamDamage = player_->GetAttackDamage(3);
							enemy->Damage(beamDamage);
							enemy->AddStun(4.0f);
							player_->AddDamagePopup(VAdd(enemyPos, VGet(0.0f, 60.0f, 0.0f)), beamDamage);
							g_kamehameHit = true;
						}

						kamehameDamageTimer_ = 0.0f;
					}
				}
			}
		}
		break;
	}
	case GAME_STATE::CLEAR:

		clearTimer_ +=
			SceneManager::GetInstance().GetDeltaTime();

		// 1秒後から入力受付
		if (clearTimer_ >= 1.0f)
		{
			if (ins.IsTrgDown(KEY_INPUT_SPACE))
			{
				// STAGE1 → STAGE2
				if (stageNo_ == STAGE_NO::STAGE_1)
				{
					ChangeGameStage(
						STAGE_NO::STAGE_2
					);
				}

				// STAGE2 → STAGE3
				else if (stageNo_ == STAGE_NO::STAGE_2)
				{
					ChangeGameStage(
						STAGE_NO::STAGE_3
					);
				}
			}
		}
		break;

	case GAME_STATE::GAME_CLEAR:

		// GAME CLEAR演出用に同じタイマーを使う
		clearTimer_ +=
			SceneManager::GetInstance().GetDeltaTime();

		// 2秒後からSPACE受付
		if (clearTimer_ >= 2.0f)
		{
			if (ins.IsTrgDown(KEY_INPUT_SPACE))
			{
				SceneManager::GetInstance().ChangeScene(
					SceneManager::SCENE_ID::TITLE
				);
			}
		}

		break;
	}
}

void GameScene::Draw(void)
{

	// 背景
	skyDome_->Draw();
	stage_->Draw();

	player_->Draw();

	for (auto& enemy : enemies_)
	{
		if (!enemy)
		{
			continue;
		}

		if (enemy->IsDead())
		{
			continue;
		}

		enemy->Draw();
	}

	// 相殺勝負: ぶつかっている光
	if (g_clash.active && g_clash.enemy != nullptr)
	{
		const VECTOR bs = player_->GetKamehameStartPos();
		const VECTOR es = g_clash.enemySide;
		const VECTOR bp = g_clash.pos;

		const float tt = (float)GetNowCount() * 0.001f;
		const float pulse = 0.5f + 0.5f * sinf(tt * 40.0f);

		SetUseLighting(FALSE);

		// 自分側のビーム(青)
		SetDrawBlendMode(DX_BLENDMODE_ADD, 170);
		DrawCapsule3D(bs, bp, 24.0f + 5.0f * pulse, 10, GetColor(60, 140, 255), GetColor(60, 140, 255), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_ADD, 230);
		DrawCapsule3D(bs, bp, 11.0f, 8, GetColor(230, 245, 255), GetColor(230, 245, 255), TRUE);

		// 敵側(紫)
		SetDrawBlendMode(DX_BLENDMODE_ADD, 170);
		DrawCapsule3D(es, bp, 24.0f + 5.0f * pulse, 10, GetColor(190, 70, 255), GetColor(190, 70, 255), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_ADD, 230);
		DrawCapsule3D(es, bp, 11.0f, 8, GetColor(255, 225, 255), GetColor(255, 225, 255), TRUE);

		// ぶつかっている光の球
		SetDrawBlendMode(DX_BLENDMODE_ADD, 120);
		DrawSphere3D(bp, 80.0f + 30.0f * pulse, 20, GetColor(255, 220, 120), GetColor(255, 220, 120), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_ADD, 200);
		DrawSphere3D(bp, 48.0f + 14.0f * pulse, 20, GetColor(255, 250, 200), GetColor(255, 250, 200), TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		DrawSphere3D(bp, 28.0f, 16, GetColor(255, 255, 255), GetColor(255, 255, 255), TRUE);

		SetUseLighting(TRUE);
	}

	// STAGE CLEAR演出
	if (gameState_ == GAME_STATE::CLEAR)
	{
		// 0.0 ～ 1.0
		float rate = clearTimer_ / 0.5f;

		if (rate > 1.0f)
		{
			rate = 1.0f;
		}

		// 背景を暗くする
		int bgAlpha =
			static_cast<int>(150.0f * rate);

		SetDrawBlendMode(
			DX_BLENDMODE_ALPHA,
			bgAlpha
		);

		DrawBox(
			0,
			0,
			Application::SCREEN_SIZE_X,
			Application::SCREEN_SIZE_Y,
			GetColor(0, 0, 0),
			TRUE
		);

		SetDrawBlendMode(
			DX_BLENDMODE_NOBLEND,
			255
		);


		// STAGE CLEAR 拡大演出
		// 0.5倍 → 1.0倍
		float scale =
			0.5f + 0.5f * rate;

		int fontSize =
			static_cast<int>(48.0f * scale);

		int fontHandle =
			CreateFontToHandle(
				nullptr,
				fontSize,
				3
			);

		const char* text = "STAGE CLEAR!";

		int textWidth =
			GetDrawStringWidthToHandle(
				text,
				-1,
				fontHandle
			);

		int drawX =
			Application::SCREEN_SIZE_X / 2
			- textWidth / 2;

		int drawY =
			Application::SCREEN_SIZE_Y / 2
			- fontSize / 2;

		// 文字もフェードイン
		int textAlpha =
			static_cast<int>(255.0f * rate);

		SetDrawBlendMode(
			DX_BLENDMODE_ALPHA,
			textAlpha
		);

		DrawStringToHandle(
			drawX,
			drawY,
			text,
			GetColor(255, 230, 80),
			fontHandle
		);

		SetDrawBlendMode(
			DX_BLENDMODE_NOBLEND,
			255
		);

		DeleteFontToHandle(fontHandle);

		// NEXT STAGE
		// 1秒経過してから表示
		if (clearTimer_ >= 1.0f)
		{
			const char* nextText =
				"SPACE : NEXT STAGE";

			int nextWidth =
				GetDrawStringWidth(
					nextText,
					-1
				);

			DrawString(
				Application::SCREEN_SIZE_X / 2
				- nextWidth / 2,

				Application::SCREEN_SIZE_Y / 2
				+ 70,

				nextText,

				GetColor(255, 255, 255)
			);
		}
	}

	if (gameState_ == GAME_STATE::GAME_CLEAR)
	{
		float rate =
			clearTimer_ / 1.0f;

		if (rate > 1.0f)
		{
			rate = 1.0f;
		}

		// 背景暗転
		int bgAlpha =
			static_cast<int>(
				180.0f * rate
				);

		SetDrawBlendMode(
			DX_BLENDMODE_ALPHA,
			bgAlpha
		);

		DrawBox(
			0,
			0,
			Application::SCREEN_SIZE_X,
			Application::SCREEN_SIZE_Y,
			GetColor(0, 0, 0),
			TRUE
		);

		SetDrawBlendMode(
			DX_BLENDMODE_NOBLEND,
			255
		);

		// GAME CLEAR
		float scale =
			0.5f + 0.5f * rate;

		int fontSize =
			static_cast<int>(
				64.0f * scale
				);

		int fontHandle =
			CreateFontToHandle(
				nullptr,
				fontSize,
				3
			);

		const char* text =
			"GAME CLEAR";

		int width =
			GetDrawStringWidthToHandle(
				text,
				-1,
				fontHandle
			);

		SetDrawBlendMode(
			DX_BLENDMODE_ALPHA,
			static_cast<int>(255.0f * rate)
		);

		DrawStringToHandle(
			Application::SCREEN_SIZE_X / 2
			- width / 2,

			Application::SCREEN_SIZE_Y / 2
			- 80,

			text,

			GetColor(100, 220, 255),

			fontHandle
		);

		SetDrawBlendMode(
			DX_BLENDMODE_NOBLEND,
			255
		);

		DeleteFontToHandle(
			fontHandle
		);

		// サブタイトル
		const char* subText =
			"ALL STAGES COMPLETED";

		int subWidth =
			GetDrawStringWidth(
				subText,
				-1
			);

		DrawString(
			Application::SCREEN_SIZE_X / 2
			- subWidth / 2,

			Application::SCREEN_SIZE_Y / 2
			+ 10,

			subText,

			GetColor(255, 255, 255)
		);

		// タイトルへ戻る
		if (clearTimer_ >= 2.0f)
		{
			const char* backText =
				"SPACE : TITLE";

			int backWidth =
				GetDrawStringWidth(
					backText,
					-1
				);

			DrawString(
				Application::SCREEN_SIZE_X / 2
				- backWidth / 2,

				Application::SCREEN_SIZE_Y / 2
				+ 90,

				backText,

				GetColor(180, 180, 180)
			);
		}
	}

	int mainScreen = SceneManager::GetInstance().GetMainScreen();

	// ポストエフェクト(モノクロ)
	//-----------------------------------------
	if (mode_ < MODE::MONO) { return; }

	SetDrawScreen(postEffectScreen_);

	// 画面を初期化
	ClearDrawScreen();

	monoRenderer_->Draw();

	// メインに戻す
	SetDrawScreen(mainScreen);
	DrawGraph(0, 0, postEffectScreen_, false);
	//-----------------------------------------

	// ポストエフェクト(走査線)
	//-----------------------------------------
	if (mode_ < MODE::SCAN) { return; }

	SetDrawScreen(postEffectScreen_);

	// 画面を初期化
	ClearDrawScreen();

	scanMaterial_->SetConstBuf(
		1,
		{ SceneManager::GetInstance().GetTotalTime(), 0.0f, 0.0f, 0.0f }
	);
	scanRenderer_->Draw();

	// メインに戻す
	SetDrawScreen(mainScreen);
	DrawGraph(0, 0, postEffectScreen_, false);
	//-----------------------------------------


	// ポストエフェクト(レンズの歪み)
	//-----------------------------------------
	if (mode_ < MODE::LENS) { return; }

	SetDrawScreen(postEffectScreen_);

	// 画面を初期化
	ClearDrawScreen();

	lensRenderer_->Draw();

	// メインに戻す
	SetDrawScreen(mainScreen);
	DrawGraph(0, 0, postEffectScreen_, false);
	//-----------------------------------------


	// ポストエフェクト(ビネット)
	//-----------------------------------------
	if (mode_ < MODE::VINE) { return; }

	SetDrawScreen(postEffectScreen_);

	// 画面を初期化
	ClearDrawScreen();

	vineRenderer_->Draw();

	// メインに戻す
	SetDrawScreen(mainScreen);
	DrawGraph(0, 0, postEffectScreen_, false);
	//-----------------------------------------
}

void GameScene::ChangeGameStage(STAGE_NO stageNo)
{
	stageNo_ = stageNo;

	// 敵を作り直すので、相殺勝負の相手は忘れる
	g_clash = ClashState();
	g_kamehameHit = false;
	player_->SetClash(false, 0.5f);

	enemies_.clear();

	// Playerに登録しているColliderを一旦削除
	player_->ClearCollider();

	// ステージ本体のColliderを戻す
	auto planet =
		stage_->GetPlanet(
			Stage::NAME::MAIN_PLANET
		).lock();

	if (planet)
	{
		player_->AddCollider(
			planet->GetTransform().collider
		);
	}

	// 新しいステージ作成
	switch (stageNo_)
	{
	case STAGE_NO::STAGE_1:

		MakeStage1();

		break;


	case STAGE_NO::STAGE_2:

		MakeStage2();

		break;

	case STAGE_NO::STAGE_3:

		MakeStage3();
		break;
	}

	// 状態リセット
	gameState_ = GAME_STATE::PLAY;

	clearTimer_ = 0.0f;

	// ステージ開始時のタイマーリセット
	stageStartTimer_ = 0.0f;
}

void GameScene::DrawStageStartUI(void)
{
	// タイマーが経過したら描画終了
	if (stageStartTimer_ >= STAGE_START_UI_TIME)
	{
		return;
	}

	// 表示する文字
	const char* stageText = "";
	const char* subText = "";

	switch (stageNo_)
	{
	case STAGE_NO::STAGE_1:
		stageText = "STAGE 1";
		subText = "FIRST SHIFT";
		break;
	case STAGE_NO::STAGE_2:
		stageText = "STAGE 2";
		subText = "DOUBLE SWITCH";
		break;
	case STAGE_NO::STAGE_3:
		stageText = "STAGE 3";
		subText = "GHOST PUZZLE";
		break;
	default:
		break;
	}

	// フェード
	int alpha = 255;

	// 最初の0.5秒の領域展開
	if (stageStartTimer_ < 0.5f)
	{
		float rate =
			stageStartTimer_ / 0.5f;

		alpha =
			static_cast<int>(
				255.0f * rate
				);
	}

	// 最後の0.5秒の領域収縮
	else if (stageStartTimer_ > STAGE_START_UI_TIME - 0.5f)
	{
		float rate =
			(STAGE_START_UI_TIME - stageStartTimer_)
			/ 0.5f;

		alpha =
			static_cast<int>(
				255.0f * rate
				);
	}

	// 拡大率
	float scale = 1.0f;

	// 最初の0.5秒だけ
	if (stageStartTimer_ < 0.5f)
	{
		float rate =
			stageStartTimer_ / 0.5f;

		scale =
			0.7f + 0.3f * rate;
	}

	// 画面中央
	const int centerX =
		Application::SCREEN_SIZE_X / 2;

	const int centerY =
		Application::SCREEN_SIZE_Y / 2 - 100;

	// 半透明描画
	SetDrawBlendMode(
		DX_BLENDMODE_ALPHA,
		alpha
	);

	// 上のライン
	DrawLine(
		centerX - 180,
		centerY - 65,

		centerX + 180,
		centerY - 65,

		GetColor(100, 220, 255),

		2
	);

	// 下のライン
	DrawLine(
		centerX - 180,
		centerY + 65,

		centerX + 180,
		centerY + 65,

		GetColor(100, 220, 255),

		2
	);

	// STAGE文字
	int fontSize =
		static_cast<int>(
			48.0f * scale
			);

	// フォント作成
	int fontHandle =
		CreateFontToHandle(
			nullptr,
			fontSize,
			3
		);

	// 文字の横幅を取得
	int stageWidth =
		GetDrawStringWidthToHandle(
			stageText,
			-1,
			fontHandle
		);

	// 中央に描画
	DrawStringToHandle(
		centerX - stageWidth / 2,
		centerY - 35,

		stageText,

		GetColor(255, 255, 255),

		fontHandle
	);

	// フォント解放
	DeleteFontToHandle(
		fontHandle
	);

	// サブタイトル
	int subWidth =
		GetDrawStringWidth(
			subText,
			-1
		);

	DrawString(
		centerX - subWidth / 2,
		centerY + 30,

		subText,

		GetColor(100, 220, 255)
	);

	// ブレンドモードを元に戻す
	SetDrawBlendMode(
		DX_BLENDMODE_NOBLEND,
		255
	);
}

void GameScene::MakeStage1(void)
{
}

void GameScene::MakeStage2(void)
{
}

void GameScene::MakeStage3(void)
{
	std::unique_ptr<MeleeEnemy> enemy =
		std::make_unique<MeleeEnemy>(
			*player_
		);

	enemy->Init();

	enemies_.push_back(
		std::move(enemy)
	);
}

void GameScene::MakeStage4(void)
{
}