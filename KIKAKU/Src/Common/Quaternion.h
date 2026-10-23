#pragma once
#include <DxLib.h>
// ↓ 他のファイルがこのヘッダ経由で使っている可能性があるので残している
#include <iostream>
#include <algorithm>

// クォータニオン(回転を表す4つの数値)
//   w, x, y, z の順。何も指定しなければ「回転なし」(w=1, x=y=z=0)
class Quaternion
{

public:

	static constexpr float kEpsilonNormalSqrt = 1e-15F;

	double w;
	double x;
	double y;
	double z;

	Quaternion(void);
	Quaternion(const VECTOR& rad);
	Quaternion(double w, double x, double y, double z);

	~Quaternion(void);

	// --- 作る ---

	// オイラー角(ラジアン)からクォータニオンへ変換
	static Quaternion Euler(const VECTOR& rad);
	static Quaternion Euler(double radX, double radY, double radZ);

	// 指定軸を指定角(ラジアン)だけ回転させる
	static Quaternion AngleAxis(double rad, VECTOR axis);

	// 向きベクトルを向く回転
	static Quaternion LookRotation(VECTOR dir);
	static Quaternion LookRotation(VECTOR dir, VECTOR up);

	// 行列からクォータニオンに変換
	static Quaternion GetRotation(MATRIX mat);

	// 2つのベクトルの間の回転
	static Quaternion FromToRotation(VECTOR fromDir, VECTOR toDir);

	// 回転なし
	static Quaternion Identity(void);

	// --- 合成・変換 ---

	// クォータニオンの合成
	static Quaternion Mult(const Quaternion& q1, const Quaternion& q2);
	Quaternion Mult(const Quaternion& q) const;

	// 座標を回転させる
	static VECTOR PosAxis(const Quaternion& q, VECTOR pos);
	VECTOR PosAxis(VECTOR pos) const;

	// クォータニオンからオイラー角へ変換
	static VECTOR ToEuler(const Quaternion& q);
	VECTOR ToEuler(void) const;

	// クォータニオンから行列へ変換
	static MATRIX ToMatrix(const Quaternion& q);
	MATRIX ToMatrix(void) const;

	// 回転を「軸」と「角度(ラジアン)」に分ける
	void ToAngleAxis(float* angle, VECTOR* axis);

	// --- 基本ベクトル(この回転をかけたあとの向き) ---
	VECTOR GetForward(void) const;
	VECTOR GetBack(void) const;
	VECTOR GetRight(void) const;
	VECTOR GetLeft(void) const;
	VECTOR GetUp(void) const;
	VECTOR GetDown(void) const;

	// --- 計算 ---

	// 内積
	static double Dot(const Quaternion& q1, const Quaternion& q2);
	double Dot(const Quaternion& b) const;

	// 正規化(長さを1にする)
	static Quaternion Normalize(const Quaternion& q);
	Quaternion Normalized(void) const;
	void Normalize(void);

	// 逆クォータニオン
	Quaternion Inverse(void) const;

	// 球面補間(t は 0?1 に収められる)
	static Quaternion Slerp(Quaternion from, Quaternion to, double t);

	// 球面補間(t を 0?1 に収めない)
	static Quaternion SlerpUnclamped(Quaternion a, Quaternion b, float t);

	// from から to へ、最大 maxDegreesDelta 度だけ近づける
	static Quaternion RotateTowards(const Quaternion& from, const Quaternion& to, float maxDegreesDelta);

	// 2つの回転の間の角度(度)
	static double Angle(const Quaternion& q1, const Quaternion& q2);

	// --- 大きさ ---
	double Length(void) const;
	double LengthSquared(void) const;

	// x, y, z 成分だけを取り出す
	VECTOR xyz(void) const;

private:

	// 基本ベクトルに、この回転をかけたものを返す
	VECTOR GetDir(VECTOR dir) const;

};