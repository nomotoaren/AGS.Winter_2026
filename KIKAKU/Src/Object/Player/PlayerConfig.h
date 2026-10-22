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
		constexpr float ARC_ANGLE = 8.0f * 3.14159265f / 180.0f;	// 最初に敵の方向からどれだけ横へ振るか
		constexpr float ARC_MIN_DISTANCE = 300.0f;	// 敵との距離がこれ以下なら弧を描かず、まっすぐ近づく
		constexpr float ARC_FULL_DISTANCE = 800.0f;	// 敵との距離がこれ以上なら弧を最大にする(間は距離に応じて滑らかに変わる)
		constexpr float TURN_BASE = 6.0f;			// 敵へ向かって曲がる速さの最小値(1秒あたり)
		constexpr float TURN_GROW = 20.0f;			// 時間とともに曲がる速さを上げる量
		constexpr float TURN_NEAR = 30.0f;			// 敵に近づいたときに追加で曲がる速さ
		constexpr float NEAR_DISTANCE = 300.0f;		// 「近づいた」とみなす距離
		constexpr float BRAKE_RATE = 0.12f;			// 近づくほど減速する割合
		constexpr float BRAKE_MIN = 24.0f;			// 減速したときの最低速度
		constexpr float COOLDOWN = 0.4f;			// 終わってから次を出せるまでの時間(秒)
		constexpr float STOP_DISTANCE = 90.0f;		// 敵の水平方向のこの距離手前(敵と同じ高さ)まで来たら止まる
		constexpr float ARRIVE_MARGIN = 10.0f;		// 目的地にこれ以内まで近づいたら到着とみなす
		constexpr float MAX_SPEED = 90.0f;			// 1フレームの移動量の上限(通常は時間で決まるので届かない)
		constexpr float ARC_RATE = 0.45f;			// 弧の横への膨らみ = 開始時の敵との距離 x これ(大きいほど大きく回り込む)
		constexpr float ARC_MAX = 400.0f;			// 弧の横への膨らみの上限
		constexpr float ARC_LIFT_RATE = 0.25f;		// 上への膨らみ(横の膨らみに対する割合。0で水平な弧)
		constexpr float DURATION_MIN = 0.35f;		// 敵の懐に着くまでの最短時間(秒)。短いほど「一瞬」で着く
		constexpr float DURATION_MAX = 0.70f;		// 同、最長(遠くからでもこれ以上かからない)
		constexpr float DURATION_PER_DISTANCE = 0.0002f;	// 距離1あたりに足す時間(秒)
		constexpr float INITIAL_DELAY = 0.06f;		// 最初にその場で構える時間(秒)
		constexpr float ACCEL_WINDOW = 0.40f;		// 加速が終わるまでの時間(秒)
		constexpr float MAX_DURATION = 1.0f;		// 保険のタイムアウト(秒)
		constexpr float AFTER_IMAGE_INTERVAL = 0.03f;	// 残像を残す間隔(秒)
		constexpr bool AUTO_ATTACK = true;			// 敵の懐に着いたら、そのまま1段目の攻撃に入る(カカロット風)
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
		constexpr float MAX_HP = 200.0f;			// ガード耐久値の最大
		constexpr float RECOVER_SPEED = 25.0f;		// 耐久値の回復速度(1秒あたり)
		constexpr float RECOVER_DELAY = 2.0f;		// 被弾してから回復し始めるまでの時間(秒)
		constexpr float BREAK_TIME = 3.0f;			// ガードブレイクで動けない時間(秒)
		constexpr float BURST_KI_COST = 20.0f;		// ガードバーストに必要な気
		constexpr float BURST_TIME = 0.5f;			// ガードバーストの時間(秒)
		constexpr float BURST_ANIM_END_STEP = 45.0f;	// バーストのモーションの最終フレーム
		constexpr float ANIM_HOLD_STEP = 20.0f;		// ガードの構えが完成するフレーム(要調整)
		constexpr float VANISH_WINDOW = 1.0f;		// ガードで攻撃を受けてから、バニッシュアタックを出せる時間(秒)。受けていない間は出せない
		constexpr float VANISH_KI_COST = 40.0f;		// バニッシュアタック(ガード中に×)に必要な気。バーストより多い
		constexpr float VANISH_TIME = 0.5f;			// バニッシュアタックの硬直時間(秒)
	}

	// バニッシュアタック: ガード中に×で、敵の背後へ瞬間移動して叩き落とす
	namespace Vanish
	{
		constexpr float MAX_RANGE = 1500.0f;		// この距離以内の敵にだけ出せる
		constexpr float BEHIND_DISTANCE = 140.0f;	// 敵の斜め後ろ、この距離に現れる(近すぎるとカメラに入らない)
		constexpr float ABOVE_HEIGHT = 60.0f;		// 敵より少し上に現れる(上から叩く)
		constexpr float SIDE_ANGLE_DEG = 40.0f;		// 真後ろからこの角度ずらす(カメラの旋回を小さくする)
		constexpr float HIT_DELAY = 0.30f;			// 瞬間移動してから、叩き落とすまでの間(秒)
		constexpr float CAMERA_CALM_TIME = 1.0f;	// カメラをゆっくり追従にする時間(秒)
		constexpr int   DAMAGE_RATE = 3;			// 通常攻撃のダメージの倍率
		constexpr float STUN = 30.0f;				// スタンゲージを削る量
		constexpr float KNOCK = 100.0f;				// 叩き落とす強さ
		constexpr float HIT_STOP = 0.10f;
		constexpr float SHAKE_TIME = 0.15f;
		constexpr float SHAKE_POWER = 8.0f;
	}

	// 気溜め
	namespace Charge
	{
		constexpr float END_MAX_TIME = 1.5f;		// 終了モーションの保険のタイムアウト(秒)
		constexpr float VISUAL_YAW_DEG = -65.0f;
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

		constexpr float SHOT_TIME = 0.8f;   // (構えてから発射まで)
		constexpr float END_TIME = 2.4f;   // (発射後のビームが約1.6秒)
		constexpr float TRANSITION_TIME = 0.12f;	// 発射直後に手元の気弾が消えるまでの時間(秒)
		constexpr float ANIM_LOOP_START = 40.0f;	// 終盤ループの開始ステップ(要調整)
		constexpr float ANIM_LOOP_END = 45.0f;		// 終盤ループの終了ステップ(要調整)
		constexpr float ANIM_LOOP_SPEED = 20.0f;	// ループ中の再生速度
		constexpr float BEAM_LENGTH = 2000.0f;		// ビームの当たり判定の長さ
		constexpr float BEAM_RADIUS = 25.0f;		// ビームの当たり判定の半径
		constexpr float KI_COST = 30.0f;			// 消費する気
		constexpr float ENEMY_CENTER_HEIGHT = 100.0f;	// 敵の足元から狙う位置(胸)までの高さ
	}

	// 回避ステップ(×ボタン / LSHIFT。連打で続けて出せる。カカロットの高速ステップ風)
	namespace Step
	{
		constexpr float SPEED_START = 18.0f;		// 出だしの速さ(1フレームの移動量)。大きいほど遠くまで飛ぶ(1回の移動距離 = 約 速さ x 時間 x 30)
		constexpr float SPEED_END = 4.0f; 			// 終わり際の速さ(ここへ向かって減速する)
		constexpr float DURATION = 0.22f;			// 1回のステップの時間(秒)。この間は攻撃を受けない
		constexpr float CHAIN_TIME = 0.18f;			// ステップ開始からこの時間たてば、次のステップを重ねて出せる(連打用)
		constexpr float LEAN = 0.5f;				// ステップ中に、進む向きへ体を倒す角度(rad)。0.5で約29度
		constexpr float LEAN_SMOOTH = 22.0f;		// 倒れる速さ(大きいほどキビキビ)
		constexpr float AFTER_IMAGE_INTERVAL = 0.014f;	// 残像を残す間隔(秒)。小さいほど途切れず帯のようになる
	}

	// ジャスト回避(敵の攻撃が当たる直前に回避すると成功)
	//   成功すると: 敵だけスローモーション / 気が回復 / 一定時間、攻撃のダメージが上がる
	namespace JustDodge
	{
		constexpr float WINDOW = 0.16f;				// ステップを始めてから、この時間以内に攻撃が来たらジャスト(大きいほど簡単)
		constexpr float COOLDOWN = 0.6f;			// 続けて成立しない時間(秒)
		constexpr float KI_GAIN = 20.0f;			// 回復する気
		constexpr float BUFF_TIME = 2.0f;			// 攻撃のダメージが上がる時間(秒)
		constexpr float DAMAGE_RATE = 1.5f;			// その間のダメージ倍率
		constexpr float SLOW_TIME = 0.4f;			// 敵がスローモーションになる時間(秒)
		constexpr int SLOW_DIV = 3;					// 敵を何フレームに1回だけ動かすか(3なら 1/3 の速さ)
	}

	// 移動
	namespace Move
	{
		// ロックオン中の移動基準:敵との水平距離がこれ以下なら、向きが定まらないのでカメラ基準にする
		constexpr float LOCK_BASIS_MIN_DISTANCE = 30.0f;

		// パッドの×長押しダッシュ
		constexpr float DASH_SPEED = 14.0f;			// ダッシュの速さ(右Shiftの走り=10 の2倍。好みで調整)
		constexpr float DASH_HOLD_TIME = 0.25f;		// ×を何秒押し続けたらダッシュか(ステップと区別。回避の時間0.18秒より長くする)
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

	// テンションゲージ(攻撃を当てる・受ける・ジャスト回避で溜まる。満タン+気力MAXで△(T)長押しでヒートアップ)
	namespace Tension
	{
		constexpr float MAX = 100.0f;
		constexpr float GAIN_DEAL = 2.0f;			// 攻撃を1回当てるごと
		constexpr float GAIN_TAKEN_RATE = 0.3f;		// 受けたダメージ x これ
		constexpr float GAIN_TAKEN_MAX = 6.0f;		// 1回に増える上限
		constexpr float GAIN_GUARD = 1.0f;			// ガードで受けたとき
		constexpr float GAIN_JUST_DODGE = 25.0f;	// ジャスト回避に成功したとき
	}

	// ヒートアップ(各ステータス上昇・気力消費なし・追撃が自動・フィニッシュ気弾が出やすい)
	namespace HeatUp
	{
		// 発動の演出(カカロット風): 光の柱 → 光がはじける → 拳と顔のアップ → 引いて金色をまとう
		// この間、プレイヤーは動けず攻撃も受けず、敵も止まる
		constexpr float ACTIVATE_YAW_DEG = -75.0f;	// 発動の演出中、見た目だけ体の向きを回す角度(度)
		constexpr float ACTIVATE_FLASH = 0.8f;		// 青白い光の柱が立つ時間(秒)
		constexpr float ACTIVATE_BURST_END = 1.3f;	// 光がはじけ終わる時刻(秒)
		constexpr float ACTIVATE_CLOSE_END = 2.0f;	// アップが終わる時刻(秒)
		constexpr float ACTIVATE_TIME = 2.4f;		// 演出の長さ(秒)
		constexpr float HOLD_TIME = 0.8f;			// △(T)をこの時間押し続けると発動
		constexpr float DURATION = 25.0f;			// 持続時間(秒)。テンションゲージがこの間に空になる
		constexpr float ATTACK_RATE = 1.5f;			// 攻撃力の倍率
		constexpr float SPEED_RATE = 1.2f;			// 移動速度の倍率
		constexpr float PURSUIT_AUTO_DELAY = 0.35f;	// 追撃のチャンスが出てから、自動で追撃するまでの時間(秒)
		constexpr int CHAIN_MAX = 5;				// 連続で追撃できる回数
	}

	// 追撃(ふっ飛ばした相手へ、攻撃ボタンで瞬間移動して追い打ち)
	namespace Pursuit
	{
		constexpr float WINDOW = 1.6f;				// ふっ飛ばしてから、追撃できる時間(秒)
		constexpr int CHAIN_MAX = 2;				// 通常時に連続で追撃できる回数
		constexpr float DISTANCE = 120.0f;			// 相手のこの距離手前に現れる
		constexpr float HEIGHT = 30.0f;				// 相手の足元からの高さ
		constexpr float MIN_HEIGHT = 100.0f;		// これより低い位置には現れない(床)
		constexpr float HIT_DELAY = 0.50f;			// 敵のところへ飛んでいく最長の時間(秒)。着いたら叩く
		constexpr float DASH_SPEED = 2200.0f;		// 敵のところへ飛んでいく速さ(1秒あたり)
		constexpr int DAMAGE_RATE = 2;				// 通常攻撃のダメージの倍率
		constexpr float STUN = 15.0f;
		constexpr float KNOCK = 60.0f;
		constexpr float HIT_STOP = 0.08f;
		constexpr float SHAKE_TIME = 0.12f;
		constexpr float SHAKE_POWER = 6.0f;
		constexpr float RECOVER_TIME = 0.6f;		// 追撃したあと、待機に戻すまでの時間(秒)
	}

	// ブレイク(スタン)した瞬間の演出
	namespace Break
	{
		constexpr float KNOCK = 70.0f;				// 吹っ飛ばす強さ
		constexpr float KNOCK_UP = 0.1f;			// 吹っ飛ばす向きの成分(0で真横)
		constexpr float SLOW_TIME = 1.5f;			// 敵がスローになる時間(秒)
		constexpr float FREEZE_TIME = 1.8f;
	}

	// フィニッシュ気弾(気弾を連続で撃つと、強力なものを撃つ)
	namespace Finish
	{
		constexpr int COUNT = 4;					// この回数目がフィニッシュ
		constexpr int COUNT_HEAT = 2;				// ヒートアップ中
		constexpr float RESET_TIME = 1.5f;			// 次を撃つまでにこれ以上空くと、回数が戻る(秒)
		constexpr int DAMAGE = 6;
		constexpr float STUN = 40.0f;
		constexpr float KNOCK = 40.0f;
		constexpr float RADIUS = 38.0f;
		constexpr float SPEED = 26.0f;				// 1/60秒あたり
		constexpr float LIFE = 3.5f;
		constexpr float TURN = 3.0f;				// 狙っている敵へ曲がる強さ(1秒あたり)
		constexpr float HOMING_TIME = 1.5f;
	}

	// 相殺勝負(かめはめ波と、敵の大きな気弾がぶつかる)
	namespace Clash
	{
		constexpr float START_RATE = 0.5f;			// 最初の優劣(0=負け 1=勝ち)
		constexpr float PUSH_PER_PRESS = 0.065f;	// 連打1回で押し返す量
		constexpr float ENEMY_PUSH_PER_SEC = 0.20f;	// 敵が押してくる量(1秒あたり)
		constexpr float HEAT_BONUS_PER_SEC = 0.12f;	// ヒートアップ中の押し返しの補正(1秒あたり)
		constexpr float MAX_TIME = 7.0f;			// これを過ぎたら負け
		constexpr int WIN_DAMAGE = 15;				// 勝ったときの敵へのダメージ(通常攻撃の倍率)
		constexpr float WIN_STUN = 60.0f;
		constexpr float WIN_KNOCK = 100.0f;
		constexpr int LOSE_DAMAGE_RATE = 3;			// 負けたときに受ける、気弾のダメージの倍率
		constexpr float LOSE_KNOCK = 40.0f;
		constexpr float POINT_MIN = 0.15f;		// ぶつかる光の、一番自分寄りの位置(0=自分の手元 1=敵)
		constexpr float POINT_MAX = 0.85f;		// ぶつかる光の、一番敵寄りの位置
		constexpr float POINT_FOLLOW = 8.0f;	// 光が行き先へ近づく速さ(大きいほどキビキビ動く)
	}
}