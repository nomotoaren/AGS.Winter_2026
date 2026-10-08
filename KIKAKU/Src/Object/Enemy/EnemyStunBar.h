#pragma once

// 敵のスタンゲージ(画面右上、HPバーのすぐ下)
//   たまっていくゲージ。満タンでスタンになり、スタン中は残り時間で減っていく
//   HPバー(EnemyHud)の配置に合わせて描く
class EnemyStunBar
{
public:

	EnemyStunBar(void) = default;
	~EnemyStunBar(void) = default;

	// rate: ゲージの割合(0~1) / stunned: スタン中か / time: 点滅用の経過時間(秒)
	void Draw(float rate, bool stunned, float time) const;
};