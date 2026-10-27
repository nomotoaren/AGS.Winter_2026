#pragma once

// 敵のスタンゲージ(画面右上、HPバーのすぐ下)
//   満タンから減っていくゲージ。空になるとスタンになり、スタン中は残り時間で回復していく
//   HPバー(EnemyHud)の配置に合わせて描く
class EnemyStunBar
{
public:

	EnemyStunBar(void) = default;
	~EnemyStunBar(void) = default;

	// rate: ゲージの割合(0~1) / stunned: スタン中か / time: 点滅用の経過時間(秒)
	void Draw(float rate, bool stunned, float time) const;

	// ゲージが割れた瞬間の「BREAK!」演出(elapsed: 割れてからの経過秒。BREAK_TIME を過ぎたら何も描かない)
	static constexpr float BREAK_TIME = 0.6f;
	void DrawBreak(float elapsed) const;

	// 技の名前の帯(スタンゲージの下に、右から滑り込んで、しばらくして消える。elapsed: 出してからの経過秒)
	static constexpr float SKILL_BANNER_TIME = 2.6f;
	void DrawSkillBanner(const char* name, float elapsed) const;
};