#pragma once

// プレイヤーの調整用の数値（PlayerConfig::機能名::定数名 で参照する）
namespace PlayerConfig
{
	// 変身（溜めモーションを1回流し、途中でモデルを差し替える）
	namespace Transform
	{
		inline constexpr float ANIM_SPEED = 40.0f;			// InitAnimation の値と合わせる
		inline constexpr float ANIM_END_STEP = 70.0f;
		inline constexpr float SWAP_STEP = 45.0f;			// モデルを差し替えるフレーム
		inline constexpr float BURST_STEP = 8.0f;			// 光を出すフレーム
		inline constexpr float MAX_TIME = 5.0f;			// 保険のタイムアウト(秒)
		inline constexpr float KI_COST = 30.0f;
	}

	// 高速接近（弧を描いて敵へ近づく）
	namespace Boost
	{
		inline constexpr float ARC_ANGLE = 8.0f * 3.14159265f / 180.0f;	// 最初に横へ振る角度(rad)
		inline constexpr float ARC_MIN_DISTANCE = 300.0f;	// これ以下なら弧なし
		inline constexpr float ARC_FULL_DISTANCE = 800.0f;	// これ以上なら弧を最大に
		inline constexpr float TURN_BASE = 6.0f;			// 曲がる速さ(1秒あたり)
		inline constexpr float TURN_GROW = 20.0f;
		inline constexpr float TURN_NEAR = 30.0f;
		inline constexpr float NEAR_DISTANCE = 300.0f;
		inline constexpr float COOLDOWN = 0.4f;			// 秒
		inline constexpr float STOP_DISTANCE = 90.0f;		// 敵の手前で止まる水平距離
		inline constexpr float ARRIVE_MARGIN = 10.0f;
		inline constexpr float MAX_SPEED = 90.0f;			// 1フレームの移動量の上限
		inline constexpr float DURATION_MIN = 0.35f;		// 到着までの時間(秒)
		inline constexpr float DURATION_MAX = 0.70f;
		inline constexpr float DURATION_PER_DISTANCE = 0.0002f;
		inline constexpr float INITIAL_DELAY = 0.06f;		// 秒
		inline constexpr float MAX_DURATION = 1.0f;		// 保険のタイムアウト(秒)
		inline constexpr float AFTER_IMAGE_INTERVAL = 0.03f;	// 秒
		inline constexpr bool AUTO_ATTACK = true;			// 着いたらそのまま1段目の攻撃へ
	}

	// 攻撃（届く範囲は GameScene の ATTACK_RANGE）
	namespace Attack
	{
		inline constexpr float LUNGE_TIME = 0.35f;			// 飛び込める最大時間(秒)
		inline constexpr float FOLLOW_MAX_DISTANCE = 350.0f;	// コンボ中に吸い付く距離

		inline constexpr float DISTANCE = 90.0f;			// 攻撃するときの敵との水平距離
		inline constexpr float FOLLOW_RATE = 0.25f;		// 1フレームで縮める割合
		inline constexpr float NEXT_PLAY_RATE_THRESHOLD = 0.75f;	// 次の段へ進めるモーションの割合
		inline constexpr float COMBO8_FOLLOW_END = 0.50f;	// 秒

		// コンボ中の瞬間移動
		inline constexpr float WARP_DISTANCE = 90.0f;		// 4,6,7段目
		inline constexpr float WARP_BACK_DISTANCE = 130.0f;	// 8段目
		inline constexpr float WARP_HEIGHT = 200.0f;		// 8段目

		inline constexpr int MAX_COMBO = 8;

		// 段ごとの攻撃判定時間(秒)
		struct HitWindow { float start; float end; };

		inline constexpr HitWindow HIT_WINDOWS[MAX_COMBO] =
		{
			{ 0.20f, 0.30f },	// 1
			{ 0.30f, 0.40f },	// 2
			{ 0.25f, 0.35f },	// 3
			{ 0.30f, 0.40f },	// 4
			{ 0.35f, 0.45f },	// 5
			{ 0.30f, 0.40f },	// 6
			{ 0.40f, 0.50f },	// 7
			{ 0.50f, 0.60f },	// 8
		};
	}

	// 追撃（Fキー接近）
	namespace Chase
	{
		inline constexpr float MAX_TIME = 2.5f;			// 秒
		inline constexpr float STUCK_TIME = 0.3f;			// 進めない状態がこれ続いたら終了(秒)
		inline constexpr float STUCK_MIN_MOVE = 1.0f;
		inline constexpr float ARRIVE_MARGIN = 15.0f;
		inline constexpr float HEIGHT_TOLERANCE = 150.0f;
		inline constexpr float SPEED = 18.0f;				// 1フレームの移動量
		inline constexpr float STOP_DISTANCE = 80.0f;
	}

	// ガード
	namespace Guard
	{
		inline constexpr float MAX_HP = 200.0f;
		inline constexpr float RECOVER_SPEED = 25.0f;		// 1秒あたり
		inline constexpr float RECOVER_DELAY = 2.0f;		// 秒
		inline constexpr float BREAK_TIME = 3.0f;			// 秒
		inline constexpr float BURST_KI_COST = 20.0f;
		inline constexpr float BURST_TIME = 0.5f;			// 秒
		inline constexpr float BURST_ANIM_END_STEP = 45.0f;
		inline constexpr float ANIM_HOLD_STEP = 20.0f;
		inline constexpr float VANISH_WINDOW = 1.0f;		// ガードで受けてからバニッシュを出せる時間(秒)
		inline constexpr float VANISH_KI_COST = 40.0f;
		inline constexpr float VANISH_TIME = 0.5f;			// 秒
	}

	// バニッシュアタック（敵の背後へ瞬間移動して叩き落とす）
	namespace Vanish
	{
		inline constexpr float MAX_RANGE = 1500.0f;
		inline constexpr float BEHIND_DISTANCE = 140.0f;
		inline constexpr float ABOVE_HEIGHT = 60.0f;
		inline constexpr float SIDE_ANGLE_DEG = 40.0f;		// 真後ろからずらす角度(度)
		inline constexpr float HIT_DELAY = 0.30f;			// 秒
		inline constexpr float CAMERA_CALM_TIME = 1.0f;	// 秒
		inline constexpr int   DAMAGE_RATE = 3;			// 通常攻撃の倍率
		inline constexpr float STUN = 30.0f;
		inline constexpr float KNOCK = 100.0f;
		inline constexpr float HIT_STOP = 0.10f;
		inline constexpr float SHAKE_TIME = 0.15f;
		inline constexpr float SHAKE_POWER = 8.0f;
	}

	// 気溜め
	namespace Charge
	{
		inline constexpr float END_MAX_TIME = 1.5f;		// 保険のタイムアウト(秒)
		inline constexpr float VISUAL_YAW_DEG = -65.0f;
		inline constexpr float ANIM_LOOP_START = 40.0f;
		inline constexpr float ANIM_LOOP_END = 45.0f;
		inline constexpr float ANIM_LOOP_SPEED = 5.0f;
		inline constexpr float ANIM_END_STEP = 70.0f;
	}

	// かめはめ波
	namespace Kamehame
	{
		inline constexpr float EFFECT_TIME_SCALE = 0.5f;	// エフェクトの再生速度の倍率
		inline constexpr float EFFECT_SCALE = 6.0f;		// 溜めの光の大きさ
		inline constexpr float BEAM_SCALE_XY = 10.0f;		// ビームの太さ
		inline constexpr float BEAM_SCALE_Z = 66.0f;		// ビームの長さ(BEAM_LENGTH に合わせる)
		inline constexpr float EFFECT_FORWARD = 40.0f;
		inline constexpr float EFFECT_FIRE_FRAME = 140.0f;	// ビームが出るフレーム

		inline constexpr float SHOT_TIME = 0.8f;			// 秒
		inline constexpr float END_TIME = 2.4f;			// 秒
		inline constexpr float HOLD_MARGIN = 0.2f;			// 相殺勝負中、終わるこの秒数前で止める
		inline constexpr float TRANSITION_TIME = 0.12f;	// 秒
		inline constexpr float ANIM_LOOP_START = 40.0f;
		inline constexpr float ANIM_LOOP_END = 45.0f;
		inline constexpr float ANIM_LOOP_SPEED = 20.0f;
		inline constexpr float BEAM_LENGTH = 2000.0f;		// 当たり判定
		inline constexpr float BEAM_RADIUS = 25.0f;		// 当たり判定
		inline constexpr float KI_COST = 30.0f;
		inline constexpr float ENEMY_CENTER_HEIGHT = 100.0f;	// 足元から胸までの高さ
	}

	// 連続エネルギー弾
	namespace Barrage
	{
		inline constexpr float KI_COST = 20.0f;
		inline constexpr int SHOT_COUNT = 16;
		inline constexpr float START_DELAY = 0.5f;
		inline constexpr float INTERVAL = 0.14f;
		inline constexpr float END_DELAY = 0.6f;
		inline constexpr float RING_RADIUS = 140.0f;
		inline constexpr float AIM_RATE = 0.7f;
		inline constexpr float AIM_SPEED = 300.0f;
		inline constexpr float AIM_MAX = 250.0f;
		inline constexpr float TARGET_HEIGHT = 70.0f;
		inline constexpr float FORWARD_AIM_DIST = 800.0f;
		inline constexpr float SPEED = 2200.0f;
		inline constexpr float MIN_FLIGHT = 0.18f;
		inline constexpr float ARC = 0.08f;
		inline constexpr float LIFE = 1.5f;
		inline constexpr float RADIUS = 9.0f;
		inline constexpr float TAIL_STEP = 14.0f;
		inline constexpr float STUN = 4.0f;
	}

	// 回避ステップ
	namespace Step
	{
		inline constexpr float SPEED_START = 18.0f;		// 1フレームの移動量
		inline constexpr float SPEED_END = 4.0f;
		inline constexpr float DURATION = 0.22f;			// 無敵時間(秒)
		inline constexpr float CHAIN_TIME = 0.18f;			// 次のステップを出せるまで(秒)
		inline constexpr float LEAN = 0.5f;				// rad
		inline constexpr float LEAN_SMOOTH = 22.0f;
		inline constexpr float AFTER_IMAGE_INTERVAL = 0.014f;	// 秒
	}

	// ジャスト回避
	namespace JustDodge
	{
		inline constexpr float WINDOW = 0.16f;				// 秒
		inline constexpr float COOLDOWN = 0.6f;			// 秒
		inline constexpr float KI_GAIN = 20.0f;
		inline constexpr float BUFF_TIME = 2.0f;			// 秒
		inline constexpr float DAMAGE_RATE = 1.5f;
		inline constexpr float SLOW_TIME = 0.4f;			// 秒
		inline constexpr int SLOW_DIV = 3;					// 敵を何フレームに1回動かすか
	}

	// 移動
	namespace Move
	{
		inline constexpr float LOCK_BASIS_MIN_DISTANCE = 30.0f;	// これ以下ならカメラ基準で移動
		inline constexpr float DASH_SPEED = 14.0f;
		inline constexpr float DASH_HOLD_TIME = 0.25f;		// Step::CHAIN_TIME より長くする
	}

	// 浮遊の見た目（描画用の行列だけ傾ける）
	namespace Float
	{
		inline constexpr float BOB_AMPLITUDE = 3.0f;
		inline constexpr float BOB_SPEED = 2.0f;			// rad/秒
		inline constexpr float PIVOT_HEIGHT = 70.0f;
		inline constexpr float LEAN_PER_SPEED = 0.025f;	// rad
		inline constexpr float VERTICAL_LEAN = 0.02f;
		inline constexpr float MAX_PITCH = 0.35f;			// rad
		inline constexpr float MAX_ROLL = 0.25f;			// rad
		inline constexpr float LEAN_SMOOTH = 8.0f;
		inline constexpr float ROLL_SIGN = 1.0f;			// 逆に傾いたら -1
	}

	// 浮遊移動中の脚のポーズ（角度は度。F1 の調整モードで確認できる）
	namespace LegPose
	{
		inline constexpr float BLEND_SPEED = 6.0f;
		inline constexpr bool IN_IDLE = false;				// 止まっているときもこのポーズにするか

		inline constexpr float L_THIGH = 45.0f;
		inline constexpr float L_KNEE = 100.0f;
		inline constexpr float L_OPEN = 5.0f;
		inline constexpr float R_THIGH = 0.0f;
		inline constexpr float R_KNEE = 40.0f;
		inline constexpr float R_OPEN = 18.0f;
		inline constexpr float FOOT = 30.0f;
		inline constexpr float LEAN_RATE = 0.6f;

		inline constexpr float SIDE_SPREAD = 12.0f;
		inline constexpr float SWAY = 4.0f;

		// 向きが逆になったら -1
		inline constexpr float FORWARD_SIGN = 1.0f;
		inline constexpr float SIDE_SIGN = 1.0f;
		inline constexpr float OPEN_SIGN = 1.0f;
	}

	// テンションゲージ
	namespace Tension
	{
		inline constexpr float MAX = 100.0f;
		inline constexpr float GAIN_DEAL = 2.0f;
		inline constexpr float GAIN_TAKEN_RATE = 0.3f;		// 受けたダメージ x これ
		inline constexpr float GAIN_TAKEN_MAX = 6.0f;
		inline constexpr float GAIN_GUARD = 1.0f;
		inline constexpr float GAIN_JUST_DODGE = 25.0f;
	}

	// ヒートアップ（時間は秒）
	namespace HeatUp
	{
		inline constexpr float ACTIVATE_YAW_DEG = -75.0f;	// 度
		inline constexpr float ACTIVATE_FLASH = 0.8f;
		inline constexpr float ACTIVATE_BURST_END = 1.3f;
		inline constexpr float ACTIVATE_CLOSE_END = 2.0f;
		inline constexpr float ACTIVATE_TIME = 2.4f;
		inline constexpr float HOLD_TIME = 0.8f;
		inline constexpr float DURATION = 25.0f;
		inline constexpr float ATTACK_RATE = 1.5f;
		inline constexpr float SPEED_RATE = 1.2f;
		inline constexpr float PURSUIT_AUTO_DELAY = 0.35f;
		inline constexpr int CHAIN_MAX = 5;
	}

	// 追撃（ふっ飛ばした相手へ瞬間移動）
	namespace Pursuit
	{
		inline constexpr float WINDOW = 1.6f;				// 秒
		inline constexpr int CHAIN_MAX = 2;
		inline constexpr float DISTANCE = 120.0f;
		inline constexpr float HEIGHT = 30.0f;
		inline constexpr float MIN_HEIGHT = 100.0f;
		inline constexpr float HIT_DELAY = 0.50f;			// 秒
		inline constexpr float DASH_SPEED = 2200.0f;		// 1秒あたり
		inline constexpr int DAMAGE_RATE = 2;				// 通常攻撃の倍率
		inline constexpr float STUN = 15.0f;
		inline constexpr float KNOCK = 60.0f;
		inline constexpr float HIT_STOP = 0.08f;
		inline constexpr float SHAKE_TIME = 0.12f;
		inline constexpr float SHAKE_POWER = 6.0f;
		inline constexpr float RECOVER_TIME = 0.6f;		// 秒
	}

	// ブレイクした瞬間
	namespace Break
	{
		inline constexpr float KNOCK = 70.0f;
		inline constexpr float KNOCK_UP = 0.1f;			// 0で真横
		inline constexpr float SLOW_TIME = 1.5f;			// 秒
		inline constexpr float FREEZE_TIME = 1.8f;
	}

	// フィニッシュ気弾
	namespace Finish
	{
		inline constexpr int COUNT = 4;					// 何発目がフィニッシュか
		inline constexpr int COUNT_HEAT = 2;				// ヒートアップ中
		inline constexpr float RESET_TIME = 1.5f;			// 秒
		inline constexpr int DAMAGE = 6;
		inline constexpr float STUN = 40.0f;
		inline constexpr float KNOCK = 40.0f;
		inline constexpr float RADIUS = 38.0f;
		inline constexpr float SPEED = 26.0f;				// 1/60秒あたり
		inline constexpr float LIFE = 3.5f;
		inline constexpr float TURN = 3.0f;				// 1秒あたり
		inline constexpr float HOMING_TIME = 1.5f;
	}

	// 相殺勝負（0=負け 1=勝ち）
	namespace Clash
	{
		inline constexpr float START_RATE = 0.5f;
		inline constexpr float PUSH_PER_PRESS = 0.065f;
		inline constexpr float ENEMY_PUSH_PER_SEC = 0.20f;
		inline constexpr float HEAT_BONUS_PER_SEC = 0.12f;
		inline constexpr float MAX_TIME = 7.0f;			// 過ぎたら負け(秒)
		inline constexpr int WIN_DAMAGE = 15;				// 通常攻撃の倍率
		inline constexpr float WIN_STUN = 60.0f;
		inline constexpr float WIN_KNOCK = 100.0f;
		inline constexpr int LOSE_DAMAGE_RATE = 3;
		inline constexpr float LOSE_KNOCK = 40.0f;
		inline constexpr float POINT_MIN = 0.15f;		// ぶつかる光の位置(0=手元 1=敵)
		inline constexpr float POINT_MAX = 0.85f;
		inline constexpr float POINT_FOLLOW = 8.0f;
	}

	// カメラの揺れ(TIME = 秒, POWER = 強さ)
	namespace HitShake
	{
		inline constexpr float MELEE_TIME = 0.08f;
		inline constexpr float MELEE_POWER = 3.0f;
		inline constexpr float FINISH_TIME = 0.15f;
		inline constexpr float FINISH_POWER = 8.0f;
		inline constexpr float CRITICAL_TIME = 0.10f;
		inline constexpr float CRITICAL_POWER = 5.0f;
		inline constexpr float KI_BLAST_TIME = 0.08f;
		inline constexpr float KI_BLAST_POWER = 2.5f;
		inline constexpr float BARRAGE_TIME = 0.05f;
		inline constexpr float BARRAGE_POWER = 1.2f;
		inline constexpr float BEAM_TIME = 0.10f;
		inline constexpr float BEAM_POWER = 1.5f;
		inline constexpr float BREAK_TIME = 0.25f;
		inline constexpr float BREAK_POWER = 9.0f;

		// 受けたダメージが大きいほど強く・長く
		inline constexpr float TAKEN_TIME_MIN = 0.10f;
		inline constexpr float TAKEN_TIME_MAX = 0.30f;
		inline constexpr float TAKEN_POWER_MIN = 3.0f;
		inline constexpr float TAKEN_POWER_MAX = 12.0f;
		inline constexpr float TAKEN_FULL_RATE = 0.05f;	// 最大HPのこの割合で最大の揺れ
		inline constexpr float BARRAGE_TAKEN_RATE = 0.5f;

		inline constexpr float GUARD_TIME = 0.08f;
		inline constexpr float GUARD_POWER = 2.0f;
	}
}