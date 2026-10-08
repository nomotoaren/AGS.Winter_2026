#pragma once

// プレイヤーの調整用の数値をまとめたファイル
// 挙動や演出を調整したいときは、ここの数字だけを変える
// 使い方: PlayerConfig::Boost::ARC_ANGLE のように「機能名::定数名」で参照する
namespace PlayerConfig
{
	// 変身
	// 変身は「溜めモーション(0~70フレーム)を1回だけ流す」演出
	// 途中でモデルを差し替え、モーションが終わったら変身が終わる
	namespace Transform
	{
		constexpr float ANIM_SPEED = 40.0f;			// 溜めモーションの再生速度(InitAnimation の値と合わせる)
		constexpr float ANIM_END_STEP = 70.0f;		// モーションの最終フレーム
		constexpr float SWAP_STEP = 45.0f;			// このフレームでモデルを差し替える
		constexpr float BURST_STEP = 8.0f;			// このフレームで爆発(光)を出す(大きいほど遅い)
		constexpr float MAX_TIME = 5.0f;			// 保険のタイムアウト(秒)
		constexpr float KI_COST = 30.0f;			// 変身に必要な気
	}

	// 高速接近の軌道(カカロット風に、敵へまっすぐではなく弧を描いて近づく)
	namespace Boost
	{
		constexpr float ARC_ANGLE = 60.0f * 3.14159265f / 180.0f;	// 最初に敵の方向からどれだけ横へ振るか
		constexpr float ARC_MIN_DISTANCE = 250.0f;	// 敵との距離がこれ以下なら弧を描かず、まっすぐ近づく
		constexpr float ARC_FULL_DISTANCE = 800.0f;	// 敵との距離がこれ以上なら弧を最大にする(間は距離に応じて滑らかに変わる)
		constexpr float TURN_BASE = 3.0f;			// 敵へ向かって曲がる速さの最小値(1秒あたり)
		constexpr float TURN_GROW = 14.0f;			// 時間とともに曲がる速さを上げる量
		constexpr float TURN_NEAR = 40.0f;			// 敵に近づいたときに追加で曲がる速さ
		constexpr float NEAR_DISTANCE = 300.0f;		// 「近づいた」とみなす距離
		constexpr float BRAKE_RATE = 0.25f;			// 近づくほど減速する割合
		constexpr float BRAKE_MIN = 10.0f;			// 減速したときの最低速度
		constexpr float COOLDOWN = 0.4f;			// 終わってから次を出せるまでの時間(秒)
		constexpr float STOP_DISTANCE = 70.0f;		// 敵のこの距離まで来たら止まる
		constexpr float MAX_SPEED = 45.0f;			// 最高速度(1フレームの移動量)
		constexpr float INITIAL_DELAY = 0.10f;		// 最初にその場で構える時間(秒)
		constexpr float ACCEL_WINDOW = 0.20f;		// 加速が終わるまでの時間(秒)
		constexpr float MAX_DURATION = 2.0f;		// 保険のタイムアウト(秒)
		constexpr float AFTER_IMAGE_INTERVAL = 0.03f;	// 残像を残す間隔(秒)
	}

	// 攻撃(コンボ・飛び込み・吸い付き)
	// 攻撃すると敵へ向かって飛び込みながら攻撃する。ただし飛び込める時間(=距離)には限りがあり、
	// 届かなければ途中で空振りになる(当たる範囲は GameScene の ATTACK_RANGE で決まる)
	namespace Attack
	{
		constexpr float LUNGE_TIME = 0.35f;			// 飛び込める最大時間(秒)。長くするほど遠くまで届く
		constexpr float FOLLOW_MAX_DISTANCE = 350.0f;	// コンボ中に敵へ吸い付く距離

		constexpr float DISTANCE = 90.0f;			// 攻撃するときの敵との水平距離(届いたらここへ合わせる)
		constexpr float FOLLOW_RATE = 0.25f;		// コンボ中に敵へ吸い付く割合(1フレームで距離の何割を縮めるか)
		constexpr float NEXT_PLAY_RATE_THRESHOLD = 0.75f;	// 次の段へ進めるモーションの再生割合
		constexpr float COMBO8_FOLLOW_END = 0.50f;	// 8段目で敵への吸い付きをやめる時間(秒)

		// コンボ中の瞬間移動
		constexpr float WARP_DISTANCE = 90.0f;		// 4,6,7段目:敵からの距離
		constexpr float WARP_BACK_DISTANCE = 130.0f;	// 8段目:敵の手前の距離
		constexpr float WARP_HEIGHT = 200.0f;		// 8段目:高さ

		constexpr int MAX_COMBO = 8;

		// コンボごとの攻撃判定時間(index 0 = 1段目)
		struct HitWindow { float start; float end; };

		constexpr HitWindow HIT_WINDOWS[MAX_COMBO] =
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

	// 追撃(Fキー接近)
	namespace Chase
	{
		constexpr float MAX_TIME = 2.5f;			// これを超えたら強制終了
		constexpr float STUCK_TIME = 0.3f;			// 前に進めない状態がこれ続いたら終了
		constexpr float STUCK_MIN_MOVE = 1.0f;		// 1フレームにこれ未満しか近づかないと「進めていない」
		constexpr float ARRIVE_MARGIN = 15.0f;		// 到着とみなす余裕(ぴったりすぎて届かないのを防ぐ)
		constexpr float HEIGHT_TOLERANCE = 150.0f;	// 到着判定で許す高さの差
		constexpr float SPEED = 18.0f;				// 1フレームの移動量
		constexpr float STOP_DISTANCE = 80.0f;		// 敵のこの距離で止まる
	}

	// ガード
	namespace Guard
	{
		constexpr float MAX_HP = 100.0f;			// ガード耐久値の最大
		constexpr float RECOVER_SPEED = 25.0f;		// 耐久値の回復速度(1秒あたり)
		constexpr float RECOVER_DELAY = 2.0f;		// 被弾してから回復し始めるまでの時間(秒)
		constexpr float BREAK_TIME = 3.0f;			// ガードブレイクで動けない時間(秒)
		constexpr float BURST_KI_COST = 20.0f;		// ガードバーストに必要な気
		constexpr float BURST_TIME = 0.5f;			// ガードバーストの時間(秒)
		constexpr float BURST_ANIM_END_STEP = 45.0f;	// バーストのモーションの最終フレーム
	}

	// 気溜め
	namespace Charge
	{
		constexpr float END_MAX_TIME = 1.5f;		// 終了モーションの保険のタイムアウト(秒)
	}

	// かめはめ波のエフェクト(blue_laser.efkefc)
	namespace Kamehame
	{
		constexpr bool USE_OLD_BEAM = false;		// true にすると、前のビームのモデルも一緒に描画(手元の気弾は常に描画)
		constexpr float EFFECT_TIME_SCALE = 0.5f;	// エフェクトの再生速度の倍率。ビームが早く出すぎるなら小さく、遅いなら大きくする
		constexpr float EFFECT_SCALE = 6.0f;		// 溜めの大きさ(手元の光の大きさ)
		constexpr float BEAM_SCALE_XY = 10.0f;		// 発射後のビームの太さ
		constexpr float BEAM_SCALE_Z = 66.0f;		// 発射後のビームの長さ(30 x これ = 約2000 = 当たり判定の長さ)
		constexpr float EFFECT_FORWARD = 40.0f;		// 発射のとき、ビームを手元より前へずらす距離(体にめり込んで見えにくくなるのを防ぐ)
		constexpr float EFFECT_FIRE_FRAME = 140.0f;	// エフェクトの中でビームが出るフレーム

		constexpr float SHOT_TIME = 2.0f;			// 構えてから発射するまでの時間(秒)
		constexpr float END_TIME = 3.0f;			// 構えてから終わるまでの時間(秒)
		constexpr float TRANSITION_TIME = 0.12f;	// 発射直後に手元の気弾が消えるまでの時間(秒)
		constexpr float BEAM_LENGTH = 2000.0f;		// ビームの当たり判定の長さ
		constexpr float BEAM_RADIUS = 25.0f;		// ビームの当たり判定の半径
		constexpr float KI_COST = 30.0f;			// 消費する気
		constexpr float ENEMY_CENTER_HEIGHT = 100.0f;	// 敵の足元から狙う位置(胸)までの高さ
	}

	// 移動
	namespace Move
	{
		// ロックオン中の移動基準:敵との水平距離がこれ以下なら、向きが定まらないのでカメラ基準にする
		constexpr float LOCK_BASIS_MIN_DISTANCE = 30.0f;
	}

	// 浮遊(舞空術)の見た目
	//   当たり判定は動かさず、描画用の行列だけ傾けたり揺らしたりする
	namespace Float
	{
		constexpr float BOB_AMPLITUDE = 3.0f;		// 上下ゆれの幅
		constexpr float BOB_SPEED = 2.0f;			// 上下ゆれの速さ(rad/秒)
		constexpr float PIVOT_HEIGHT = 70.0f;		// 傾ける中心の高さ(カプセルの真ん中あたり)
		constexpr float LEAN_PER_SPEED = 0.025f;	// movePow_ 1あたりの傾き(rad)。歩き5で約7度、走り10で約14度
		constexpr float VERTICAL_LEAN = 0.02f;		// 上昇でのけぞる量(movePow_.y 1あたり)
		constexpr float MAX_PITCH = 0.35f;			// 前後の傾きの最大(rad) 約20度
		constexpr float MAX_ROLL = 0.25f;			// 左右の傾きの最大(rad) 約14度
		constexpr float LEAN_SMOOTH = 8.0f;			// 傾きの追従の速さ(大きいほどキビキビ)
		constexpr float ROLL_SIGN = 1.0f;			// 横移動で逆に傾いたら -1.0f に
	}

	// 浮遊移動中の脚のポーズ(片膝を上げて、もう片方を後ろに曲げる舞空術の姿勢)
	//   移動アニメはそのまま再生し、脚のボーンだけをこのポーズへ寄せる
	//   角度は度。太もも:前に上げる向きがプラス / 膝・足首:曲げる向きがプラス
	namespace LegPose
	{
		constexpr float BLEND_SPEED = 6.0f;			// ポーズへ寄せる速さ(大きいほど早く切り替わる)
		constexpr bool IN_IDLE = false;				// true にすると止まっているときもこのポーズ

		// 左脚: 太ももを前へ上げて膝を深く曲げる(後ろから足の裏が見える)
		// 右脚: 少し外へ開いて、「く」の字に軽く曲げて垂らす
		//   ※ ゲーム中に F1 で調整モードにすると、その場で数値を変えて確かめられる(Debug ビルドのみ)
		constexpr float L_THIGH = 45.0f;			// 左の太もも(+で前へ上げる)
		constexpr float L_KNEE = 100.0f;			// 左の膝(深く曲げる)
		constexpr float L_OPEN = 5.0f;				// 左脚を外へ開く角度
		constexpr float R_THIGH = 0.0f;				// 右の太もも(ほぼ真下)
		constexpr float R_KNEE = 40.0f;				// 右の膝(「く」の字くらい)
		constexpr float R_OPEN = 18.0f;				// 右脚を外へ開く角度
		constexpr float FOOT = 30.0f;				// 足首(つま先を下へ伸ばす)
		constexpr float LEAN_RATE = 0.6f;			// 体の傾きの倍率(参考画像は胴体がほぼ立っているので弱め)

		constexpr float SIDE_SPREAD = 12.0f;		// 横移動中に、脚を進行方向と逆へ流す角度
		constexpr float SWAY = 4.0f;				// 脚のふらふら揺れの幅

		// 回転軸はキャラの左右・前後の軸から自動で求めるので、ねじれることはない
		// 向きだけはモデルの作りで逆になることがあるので、そのときはここを -1.0f にする
		constexpr float FORWARD_SIGN = 1.0f;		// 膝が前ではなく後ろへ上がったら(脚全体が逆なら)反転
		constexpr float SIDE_SIGN = 1.0f;			// 脚が進行方向へ流れたら反転
		constexpr float OPEN_SIGN = 1.0f;			// 脚が開かずに交差(内股)したら反転
	}
}