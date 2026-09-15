#pragma once
#include <DxLib.h>
class Transform;

class Capsule
{

public :

	// デバッグ時の簡易カプセル表示の色
	static constexpr int COLOR = 0xffffff;

	// コンストラクタ
	// カプセルをつけるオブジェクトを指定する
	Capsule(const Transform& parent);
	Capsule(const Capsule& base, const Transform& parent);

	// デストラクタ
	~Capsule(void);

	// 描画
	void Draw(void);

	// 親Transformからの相対位置を取得
	VECTOR GetLocalPosTop(void) const;
	VECTOR GetLocalPosDown(void) const;

	// 親Transformからの相対位置をセット
	void SetLocalPosTop(const VECTOR& pos);
	void SetLocalPosDown(const VECTOR& pos);

	// ワールド座標を取得
	VECTOR GetPosTop(void) const;
	VECTOR GetPosDown(void) const;

	// 相対座標を回転させてワールド座標で取得する
	VECTOR GetRotPos(const VECTOR& localPos) const;

	// 半径
	float GetRadius(void) const;
	void SetRadius(float radius);

	// 高さ
	float GetHeight(void) const;

	// カプセルの中心座標
	VECTOR GetCenter(void) const;

private :

	// カプセルをくっつける相手
	const Transform& transformParent_;

	// 親Transformからの相対位置(上側)
	VECTOR localPosTop_;

	// 親Transformからの相対位置(下側)
	VECTOR localPosDown_;

	// 半径
	float radius_;

};
